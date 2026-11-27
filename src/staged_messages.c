/*

 Copyright (c) 2015-2026 unfacd works

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 */


#ifdef HAVE_CONFIG_UFSRVWEBSOCK_H
#include "config_ufsrvwebsock.h"
#endif

#include "staged_messages.h"
#include "main.h"
#include <ufsrv_core/delegator_sessions/thread_context_type.h>
#include <uflib/utils.h>
#include <ufsrv_core/cache_backend/redis.h>
#include <ufnetcorelib/ufnet_msgcore/user/user_backend.h>
#include <ufsrv_core/system/log_message_literals.h>

#include <hiredis/hiredis.h>
#include "ufnet_msgcore/redis_commands_staged_messages.h"

extern __thread ThreadContext ufsrv_thread_context;

static UFSRVResult *_DbBackendGetMessageStatus(unsigned long eid);

/** \addtogroup staged_messages
 *  Operations that handle the management of staged messages
 *  @{
 */

/**
 * 	@brief: For each individual, ready to be sent, user-addressable message we store an index entry and hash entry for the actual message ( protobuf packed message)
 */
UFSRVResult *
StoreStagedMessageCacheRecordForUser(Session *sesn_ptr, TransmissionMessage *tmsg_ptr, unsigned long userid)
{
  int	rescode	= RESCODE_PROG_NULL_POINTER;

  if (unlikely(IS_EMPTY(sesn_ptr)))			goto return_generic_error;
  if (unlikely(IS_EMPTY(tmsg_ptr)))			goto return_final;

  PersistenceBackend	*pers_ptr	= THREAD_CONTEXT_USRMSG_CACHEBACKEND;

  (*pers_ptr->send_command_multi)(sesn_ptr, THREAD_CONTEXT_USRMSG_CACHEBACKEND, "MULTI");
  (*pers_ptr->send_command_multi)(sesn_ptr, THREAD_CONTEXT_USRMSG_CACHEBACKEND, REDIS_CMD_STAGED_OUTMSG_EVENT_RECORD_ADD, userid, GetTimeNowInMillis(), tmsg_ptr->fid, tmsg_ptr->gid);
  (*pers_ptr->send_command_multi)(sesn_ptr, THREAD_CONTEXT_USRMSG_CACHEBACKEND, REDIS_CMD_STAGED_OUTMSG_MSG_RECORD_ADD, userid, tmsg_ptr->fid, tmsg_ptr->gid, tmsg_ptr->len, tmsg_ptr->msg_packed,tmsg_ptr->len);
  (*pers_ptr->send_command_multi)(sesn_ptr, THREAD_CONTEXT_USRMSG_CACHEBACKEND, "EXEC");

  size_t		commands_processed = 4,
          commands_successful = 4;

  {
    size_t 			i;
    redisReply	*replies[commands_processed];

    //TODO: we need error recovery for intermediate errors
    for (i=0; i<commands_processed; i++) {
      if ((RedisGetReply((UfsrvSession *)sesn_ptr, pers_ptr, (void  *)&replies[i])) != REDIS_OK) {
        commands_successful--;

        if ((replies[i] != NULL)) {
          syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p', cid:'%lu', uid:'%lu', idex:'%lu'): ERROR PROCESSING MULTI SET COMMAND. ERROR: '%s'", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), userid, i, replies[i]->str);
        } else {
          syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p', cid:'%lu', i:'%lu'): ERROR COULD ISSUE GET COMMAND: BACKEND CONNECTIVITY ERROR", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), i);
        }
      }

      if (!IS_EMPTY(replies[i]))	freeReplyObject(replies[i]);
    }
  }

  if (commands_successful != commands_processed)	{rescode = RESCODE_BACKEND_DATA_PARTIALSET; goto return_final;}

  return_success:
  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_SUCCESS, RESCODE_BACKEND_DATA_SETCREATED);

  return_final:
  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, rescode);

  return_generic_error:
  syslog(LOG_DEBUG, LOGSTR_NULL_PARAM, __func__, pthread_self(), LOGCODE_PROTO_MISSING_PARAM, "Target Session *");
  return _ufsrv_result_generic_error;

}

/** @} */

///////////// TRANSMITTED / STAGED INTRA MESSAGES \\\\\\\\\\\\\\\\\


/**
 * 	@brief: Get one or set-of records from the staging store for transmitted INTRA messages. retrieval can be positional, or named.
 *
 * 	@param payload_name: used for named record, as opposed to positional (last, first etc)
 * 	@dynamic_meory redisReply *: EXPORTS
 */
UFSRVResult *
GetStagedMessageCacheRecordForIntraCommand(Session *sesn_ptr, IncomingMessageDescriptor *msg_desc_ptr, const char *payload_name, enum StoredMessageOptions msg_opts)
{
  int rescode	= RESCODE_PROG_NULL_POINTER;
  const char 					*command_template;
  PersistenceBackend	*pers_ptr	= THREAD_CONTEXT_USRMSG_CACHEBACKEND;
  redisReply 					*redis_ptr;

  if (msg_opts == MSGOPT_GET_FIRST)						command_template = REDIS_CMD_INTRAMESSAGE_LIST_GET_EARLIEST;
  else if (msg_opts == MSGOPT_GET_LAST)				command_template = REDIS_CMD_INTRAMESSAGE_LIST_GET_LAST;
  else if (msg_opts == MSGOPT_GETALL)					command_template = REDIS_CMD_INTRAMESSAGE_LIST_GETALL;
  else if (msg_opts == MSGOPT_GETNAMED)
  {
    if (unlikely(!IS_STR_LOADED(payload_name)))		goto return_final;
    command_template = REDIS_CMD_INTRAMESSAGE_LIST_GET_NAMED;
  }
  else goto return_final;

  if (msg_opts == MSGOPT_GETNAMED) {
    size_t	command_buf_sz = strlen(payload_name) + strlen(command_template) + 10;
    char 		command_buf[command_buf_sz];

    snprintf(command_buf, command_buf_sz, command_template, msg_desc_ptr->instance_descriptor_ptr->server_class, msg_desc_ptr->instance_descriptor_ptr->ufsrv_geogroup);
    redis_ptr = (*pers_ptr->send_command)(sesn_ptr, THREAD_CONTEXT_USRMSG_CACHEBACKEND, command_buf);
  } else {
    redis_ptr = (*pers_ptr->send_command)(sesn_ptr, THREAD_CONTEXT_USRMSG_CACHEBACKEND, command_template,
                                          msg_desc_ptr->instance_descriptor_ptr->server_class, msg_desc_ptr->instance_descriptor_ptr->ufsrv_geogroup);
  }

  if (unlikely(IS_EMPTY(redis_ptr))) {rescode = RESCODE_BACKEND_CONNECTION; goto return_error_backend_connection;}
  if (redis_ptr->elements > 1 && IS_STR_LOADED(redis_ptr->element[0]->str))	goto return_success;

  //must have encountered error in reply
  syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p', cid:'%lu', redis_error:'%s'): ERROR COULD ISSUE GET COMMAND", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), IS_PRESENT(redis_ptr->str)?redis_ptr->str:"unspecified error");
  freeReplyObject(redis_ptr);
  goto return_final;

  return_success:
  _RETURN_RESULT_SESN(sesn_ptr, redis_ptr, RESULT_TYPE_SUCCESS, RESCODE_BACKEND_DATA);

  return_error_backend_connection:
  syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p', cid:'%lu'): ERROR COULD ISSUE GET COMMAND: BACKEND CONNECTIVITY ERROR", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr));
  goto return_final;

  return_final:
  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, rescode);

  return_generic_error:
  syslog(LOG_DEBUG, LOGSTR_NULL_PARAM, __func__, pthread_self(), LOGCODE_PROTO_MISSING_PARAM, "Target Session *");
  return _ufsrv_result_generic_error;

}

/**
 * 	TODO: This is not finalised yet
 */
UFSRVResult *
RemoveStagedMessageCacheRecordForIntraCommand(Session *sesn_ptr, IncomingMessageDescriptor *msg_desc_ptr,  const char *payload_name, unsigned long call_flags)
{
  int 		rescode										= RESCODE_PROG_NULL_POINTER;

  if (unlikely(IS_EMPTY(sesn_ptr)))							goto return_generic_error;
  if (unlikely(!IS_STR_LOADED(payload_name)))		goto return_final;

  PersistenceBackend	*pers_ptr	= THREAD_CONTEXT_USRMSG_CACHEBACKEND;
  redisReply 					*redis_ptr;

  {
    size_t	command_buf_sz = strlen(payload_name) + MBUF;
    char 		command_buf[command_buf_sz + 1];

    snprintf(command_buf, command_buf_sz, REDIS_CMD_INTRAMESSAGE_RECORD_REM,
             msg_desc_ptr->instance_descriptor_ptr->server_class, msg_desc_ptr->instance_descriptor_ptr->ufsrv_geogroup,
             payload_name);
    redis_ptr = (*pers_ptr->send_command)(sesn_ptr, THREAD_CONTEXT_USRMSG_CACHEBACKEND, command_buf);
  }

  if (unlikely(IS_EMPTY(redis_ptr))) {rescode = RESCODE_BACKEND_CONNECTION; goto return_error_backend_connection;}

  return_success:
  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_SUCCESS, rescode);

  return_error_backend_connection:
  syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p', cid:'%lu'): ERROR COULD ISSUE GET COMMAND: BACKEND CONNECTIVITY ERROR", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr));
  goto return_final;

  return_final:
  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, rescode);

  return_generic_error:
  syslog(LOG_DEBUG, LOGSTR_NULL_PARAM, __func__, pthread_self(), LOGCODE_PROTO_MISSING_PARAM, "Target Session *");
  return _ufsrv_result_generic_error;

}

int
DbBackendInsertMessageRecord(const ParsedMessageDescriptor *msg_descriptor_ptr)
{
#define SQL_INSERT_NEW_INCOMING_MESSAGE "INSERT INTO messages (id_events, fid, type, rawmsg, timestamp, originator, originator_device) VALUES ('%lu', '%lu', '%u', '%s', '%lu', '%lu', '%d')"

  char *sql_query_str;
  sql_query_str = mdsprintf(SQL_INSERT_NEW_INCOMING_MESSAGE, msg_descriptor_ptr->gid, msg_descriptor_ptr->fid, msg_descriptor_ptr->msg_type, msg_descriptor_ptr->rawmsg, msg_descriptor_ptr->timestamp, msg_descriptor_ptr->userid_from, DEFAULT_DEVICE_ID);

#ifdef __UF_TESTING
  syslog(LOG_DEBUG, "%s (pid:'%lu', th_ctx:'%p'): GENERATED SQL QUERY: '%s'", __func__, pthread_self(), THREAD_CONTEXT_PTR, sql_query_str);
#endif
  int sql_result = h_query_insert(THREAD_CONTEXT_DB_BACKEND, sql_query_str);

  if (sql_result != H_OK) {
    syslog(LOG_DEBUG, "%s (pid:'%lu', th_ctx:'%p'): ERROR: COULD NOT EXECUTE QUERY: '%s'", __func__, pthread_self(), THREAD_CONTEXT_PTR, sql_query_str);
  }

  free(sql_query_str);

  return sql_result;

#undef SQL_INSERT_NEW_FENCE
}

/**
 * @brief Flag event status (flagged vs non flagged)
 * @param eid
 * @param status
 * @return
 */
int
DbBackendUpdateMessageStatus(unsigned long gid, unsigned  long uid_flagged_by, enum EventStatus status)
{
#define SQL_UPDATE_MESSAGE_STATUS 	 "UPDATE messages SET status = '%d' WHERE id_events = '%lu'"

  char *sql_query_str = mdsprintf(SQL_UPDATE_MESSAGE_STATUS, status, gid);

#ifdef __UF_TESTING
  syslog(LOG_DEBUG, "%s {th_ctx:'%p'}: GENERATED SQL QUERY: '%s'", __func__, THREAD_CONTEXT_PTR, sql_query_str);
#endif

  int sql_result = h_query_update(THREAD_CONTEXT_DB_BACKEND, sql_query_str);

  if (sql_result != H_OK) {
    syslog(LOG_DEBUG, "%s {th_ctx:'%p', event_rowid:'%lu'}: ERROR: COULD EXECUTE QUERY: '%s'", __func__, THREAD_CONTEXT_PTR, gid, sql_query_str);
  }

  free(sql_query_str);

  return sql_result;

#undef SQL_UPDATE_MESSAGE_STATUS
}

static UFSRVResult *
_DbBackendGetMessageStatus(unsigned long eid)
{
#define SQL_GET_MESSAGE_STATUS "SELECT status FROM messages WHERE eid = '%lu'"
#define COLUMN_STATUS(x)	    ((struct _h_type_int *)result.data[0][0].t_data)->value

  struct _h_result result;

  char *sql_query_str = mdsprintf(SQL_GET_MESSAGE_STATUS, eid);

#if __UF_TESTING
  syslog(LOG_DEBUG, "%s {th_ctx:'%p', eid:'%lu'}: GENERATED SQL QUERY: '%s'", __func__, THREAD_CONTEXT_PTR, eid, sql_query_str);
#endif

  int sql_result = h_query_select(THREAD_CONTEXT_DB_BACKEND, sql_query_str, &result);

  if (sql_result != H_OK) {
    syslog(LOG_DEBUG, "%s {th_ctx:'%p', eid:'%lu'}: ERROR: COULD EXECUTE QUERY: '%s'", __func__, THREAD_CONTEXT_PTR, eid, sql_query_str);

    free(sql_query_str);

    THREAD_CONTEXT_RETURN_RESULT_ERROR(NULL, RESCODE_BACKEND_DATA)
  }

  free(sql_query_str);

  //we should ever only find 1 or zero really
  if (result.nb_rows > 0) {
    unsigned status = COLUMN_STATUS(result);

    h_clean_result(&result);

    THREAD_CONTEXT_RETURN_RESULT_SUCCESS((void *)(uintptr_t)status, RESCODE_BACKEND_DATA)
  } else {
#ifdef __UF_TESTING
    syslog(LOG_DEBUG, "%s {th_ctx:'%p', eid:'%lu'}: ERROR: COULD RETRIEVE MESSAGE", __func__, THREAD_CONTEXT_PTR, eid);
#endif
  }

  exit_user_not_found:
  h_clean_result(&result);

  THREAD_CONTEXT_RETURN_RESULT_ERROR(NULL, RESCODE_BACKEND_DATA_EMPTYSET)

#undef COLUMN_STATUS
#undef SQL_GET_MESSAGE_STATUS

}

#if 0
//NOT SURE THIS IS NEEDED we already implement GET ABOVE
/**
 * 	@brief: global store of incoming user messages
 */
UFSRVResult *
GetMessageFromCacheRecords (Session *sesn_ptr, unsigned long call_flags)

{
	int rescode										= RESCODE_PROG_NULL_POINTER;
	Fence 							*f_ptr		=	NULL;
	PersistenceBackend	*pers_ptr	= THREAD_CONTEXT_USRMSG_CACHEBACKEND;
	redisReply 					*redis_ptr;

	redis_ptr=(*pers_ptr->send_command)(sesn_ptr, THREAD_CONTEXT_USRMSG_CACHEBACKEND, REDIS_CMD_INMESSAGES_LIST_GETALL, SESSION_USERID(sesn_ptr));

	if (unlikely(IS_EMPTY(redis_ptr))) {rescode=RESCODE_BACKEND_CONNECTION; goto return_error_backend_connection;}
	if (redis_ptr->elements==0)
	{
		if (redis_ptr->type==REDIS_REPLY_ERROR)	goto return_error_backend_error;
		if (redis_ptr->type==REDIS_REPLY_NIL)		goto return_error_backend_nil;

		rescode=RESCODE_BACKEND_DATA_EMPTYSET; goto return_empty_set;
	}

	{
	   //we now have a list of UID's
#ifdef __UF_TESTING
//	syslog(LOG_DEBUG, "%s {pid:'%lu, o:'%p', fid:'%lu'}: RESULT: Fence contains (%lu) users in it", __func__, pthread_self(), sesn_ptr, f_ptr, FENCE_ID(f_ptr), redis_ptr->elements);
#endif
		size_t		i, success_counter=0;
		redisReply	*sesn_ptr_list[redis_ptr->elements];
		UFSRVResult	res;

		//retrieve the raw user cache record for each uid and index it into list
		for (i=0; i < redis_ptr->elements; ++i)
		{
			 unsigned long 	user_id		= strtol(redis_ptr->element[i]->str, NULL, 10);
			 UFSRVResult 		*res_ptr	= SessionGetFromBackendRaw (sesn_ptr, sesn_ptr, user_id, 0, &res);

			 if (_RESULT_TYPE_SUCCESS(res_ptr))
			 {
				 redisReply *redis_ptr_user=((redisReply *)res_ptr->result_user_data);
				 *(sesn_ptr_list+success_counter++)=redis_ptr_user;
			 }
			 else
			 {
				 //TODO: this a stale UID which must cleanse. perhaps log it in a list for out of band processing?
				 syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p', cid:'%lu', fo:'%p', fid:'%lu', uid:'%lu'): ERROR: FOUND POTENTIALLY STALE UID --> TODO: IMPELMENT CLEANSING",
							 __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), f_ptr, FENCE_ID(f_ptr), user_id);
			 }
		}

		//we now have a list of user record each entry represented by raw redisReply *
		if (success_counter)
		{
		}

		if (success_counter < redis_ptr->elements)
		{
		 syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p', cid:'%lu'): ERROR: Received '%lu' elements: BUT ONLY PROCESED '%lu' WITH SUCCESS...",
				 __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), redis_ptr->elements, success_counter);
		}

		good_finish:
		for (i=0; i < success_counter; ++i)	 freeReplyObject(sesn_ptr_list[i]);
		freeReplyObject(redis_ptr);
		SESNSTATUS_UNSET(sesn_ptr->ufsrv_session.stat, SESNSTATUS_EPHEMERAL);

		_RETURN_RESULT_SESN(sesn_ptr, f_ptr, RESULT_TYPE_SUCCESS, RESCODE_BACKEND_DATA);

	}
	/////////
	return_error_backend_connection:
	syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p', cid:'%lu'): ERROR COULD ISSUE GET COMMAND: BACKEND CONNECTIVITY ERROR", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr));
	goto return_final;

	return_error_backend_error:
	syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p', cid:'%lu'): ERROR COULD NOT GET: REPLY ERROR '%s'", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), redis_ptr->str);
	goto on_return_free;

	return_error_backend_nil:
	syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p', cid:'%lu'): ERROR COULD NOT GET: NIL REPLY ERROR '%s'", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), redis_ptr->str);
	goto on_return_free;

	return_empty_set:
#ifdef __UF_TESTING
	syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p): NOTICE: EMPTY SET",  __func__, pthread_self(), sesn_ptr);
#endif
	goto on_return_free;

	on_return_free:
	freeReplyObject(redis_ptr);

	return_final:
	_RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, rescode);

	return_generic_error:
	syslog(LOG_DEBUG, LOGSTR_NULL_PARAM, __func__, pthread_self(), LOGCODE_PROTO_MISSING_PARAM, "Target Session *");
	return _ufsrv_result_generic_error;


}
#endif

#if 0
UFSRVResult *
AddMessageCacheRecordForUsers (Session *sesn_ptr, IncomingMessageDescriptor *msg_desc_ptr, UfsrvMsgCommandType msg_type, const unsigned char *b64encoded_rawmsg, unsigned long call_flags)
{
	size_t 							i,
											actually_processed=0;
	int rescode										= RESCODE_PROG_NULL_POINTER;
	PersistenceBackend	*pers_ptr	= THREAD_CONTEXT_USRMSG_CACHEBACKEND;
	redisReply 					*redis_ptr;

	(*pers_ptr->send_command_multi)(sesn_ptr, "MULTI");
	for (i=0; i<msg_desc_ptr->userids_to.collection_sz; i++)
	{
		(*pers_ptr->send_command_multi)(sesn_ptr, THREAD_CONTEXT_USRMSG_CACHEBACKEND, REDIS_CMD_INMESSAGE_RECORD_ADD, *((unsigned long **)msg_desc_ptr->userids_to.collection)[i], time(NULL),
				msg_type, msg_desc_ptr->fid, msg_desc_ptr->userid_from, b64encoded_rawmsg);

#ifdef __UF_FULLDEBUG
		syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', uid:'%lu', idx:'%lu'}: Processing Collection item for user...", __func__, pthread_self(), sesn_ptr, *((unsigned long **)msg_desc_ptr->userids_to.collection)[i], i);
#endif
	}
	(*pers_ptr->send_command_multi)(sesn_ptr, "EXEC");


	actually_processed=msg_desc_ptr->userids_to.collection_sz+2;
	redisReply	*replies[actually_processed];
	for (i=0; i<actually_processed; i++)	replies[i]=NULL;

	//TODO: we need error recover for intermediate errors
	for (i=0; i<actually_processed; i++)
	{
		if ((RedisGetReply((UfsrvSession *)sesn_ptr, pers_ptr, (void *)&replies[i]) != REDIS_OK))
		{
			syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', cmd_idx:'%lu'}: ERROR: REDIS COMMAND IN MULTI SET FAILED", __func__, pthread_self(), sesn_ptr, i);

			if ((replies[i] != NULL) && (replies[i]->type != REDIS_REPLY_NIL))
			{
				//error msg
				//syslog (LOG_DEBUG, LOGSTR_BACKENDCACHE_SHARED_CONTACTS, __func__, pthread_self(), sesn_ptr, i, replies[i]->str, LOGCODE_BACKENDCACHE_SHARED_CONTACTS_HIT, "Found shared contact token");
			}
		}
	}

	return_success:
	_RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_SUCCESS, rescode);

	return_error_backend_connection:
	syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p', cid:'%lu'): ERROR COULD ISSUE GET COMMAND: BACKEND CONNECTIVITY ERROR", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr));
	goto return_final;

	return_final:
	_RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, rescode);

	return_generic_error:
	syslog(LOG_DEBUG, LOGSTR_NULL_PARAM, __func__, pthread_self(), LOGCODE_PROTO_MISSING_PARAM, "Target Session *");
	return _ufsrv_result_generic_error;

}
#endif


#if 0
//NOT IMPELEMENTED
UFSRVResult *
DbBackendInsertUserMessageByProto (Session *sesn_ptr, MessageCommand *msg_cmd_ptr, unsigned char *msg_cmd_raw)
{

	unsigned long verification_code;

	{


#define SQL_INSERT_USER_MESSAGE "INSERT INTO messages (fid, source, source_device, timestamp, message, recipients) VALUES ('%s', '%s')"

		const char	*username;
		const char	*nickname_unvalidated;
		char		*nickname_validated=NULL;;
		char *msg_cmd_b64encoded=base64_encode(msg_cmd_raw, int length);
		//this will hold top level fields
		struct json_object *jobj_account=json_object_new_object();
		{
			username=json_object_get_string(json__get(jobj_device, "number"));

			nickname_unvalidated=json_object_get_string(json__get(jobj_device, ACCOUNT_JSONATTR_NICKNAME));
			if (nickname_unvalidated!=NULL)
			{
				AccountNicknameValidateForUniqueness(sesn_ptr, NULL, nickname_unvalidated, false);//can't store because account doesnt exist yet
				if (SESSION_RESULT_TYPE_SUCCESS(sesn_ptr))
				{
					nickname_validated=strdup(nickname_unvalidated);

					//store it at account level
					json_object_object_add (jobj_account, ACCOUNT_JSONATTR_NICKNAME, json_object_new_string(nickname_validated));
				}
				else
				{
					//no need to check if current nickname  equals roposed because there is no current
					json_object_object_add (jobj_account, ACCOUNT_JSONATTR_NICKNAME, json_object_new_string(CONFIG_DEFAULT_PREFS_STRING_VALUE));
				}

				//remove it from device level; it was there as a carried reference only
				json_object_object_del (jobj_device, ACCOUNT_JSONATTR_NICKNAME);
			}
			else
			{
				nickname_unassigned:
				json_object_object_add (jobj_account, ACCOUNT_JSONATTR_NICKNAME, json_object_new_string(CONFIG_DEFAULT_PREFS_STRING_VALUE));
#ifdef __UF_TESTING
				syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', username:'%s'}: No Nickname was present: assigning default", __func__, pthread_self(), sesn_ptr, SESSION_USERNAME(sesn_ptr));
#endif
			}

			json_object_object_add (jobj_account, "number", json_object_new_string(username));
			//TODO: supplied in another stream. but is currently saved at device level, so this maybe bogus entry
			json_object_object_add (jobj_account, ACCOUNT_JSONATTR_IDENTITY_KEY, json_object_new_string(CONFIG_DEFAULT_PREFS_STRING_VALUE));
			json_object_object_add (jobj_account, "authenticated_device", jobj_device);

			//attach master device to array of devices
			struct json_object *jarray_accounts=json_object_new_array();
			json_object_array_add (jarray_accounts, jobj_device);

			//attach the arry to the main account node
			json_object_object_add (jobj_account, "devices", jarray_accounts);
		}

		const char *json_account_str=json_object_to_json_string(jobj_account);
		syslog(LOG_DEBUG, "%s (cid='%lu'): GENERATED JSON ACCOUNT: '%s'", __func__, SESSION_ID(sesn_ptr), json_account_str);

		char *sql_query_str;
		sql_query_str=mdsprintf(SQL_INSERT_NEW_ACCOUNT, username, json_account_str);

#ifdef __UF_TESTING
		syslog(LOG_DEBUG, "%s (cid='%lu'): GENERATED SQL QUERY: '%s'", __func__, SESSION_ID(sesn_ptr), sql_query_str);
#endif
		int sql_result=h_query_insert(THREAD_CONTEXT_DB_BACKEND, sql_query_str);

		//this seems to be necessary, because the same object is attached twice. The reference in the array remains (valgrind complaint)
		json_object_object_del(jobj_account, "authenticated_device");
		json_object_put(jobj_account);

		if (sql_result!=H_OK)
		{
			syslog(LOG_DEBUG, "%s (cid='%lu'): ERROR: COULD EXEUTE QUERY: '%s'", __func__, SESSION_ID(sesn_ptr), sql_query_str);
		}

		free (sql_query_str);

		//cache. Should be done more broadly
		if (nickname_validated)
		{
			BackendDirectoryNicknameSet (sesn_ptr, nickname_validated);
			SESSION_USERNICKNAME(sesn_ptr)=nickname_validated;//strdup reference gets deleted at user session destruction time
		}

		return sql_result;
	}

	return H_ERROR;

#undef SQL_INSERT_NEW_ACCOUNT
}
#endif