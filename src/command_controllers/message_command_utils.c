/**
* Copyright (C) 2015-2026 unfacd works
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_UFSRVWEBSOCK_H
#include "config_ufsrvwebsock.h"
#endif

#include "main.h"
#include "message_command_utils.h"
#include "message_command_controller.h"
#include "message_command_context_type.h"
#include "command_base_context_type.h"
#include <ufnetcorelib/ufnet_msgcore/fence/fence.h>
#include <ufnetcorelib/ufnet_msgcore/fence/fence_proto.h>
#include <ufnetcorelib/ufnet_msgcore/fence/utils_fence.h>
#include <ufnetcorelib/ufnet_msgcore/UnfacdService.pb-c.h>
#include <ufnetcorelib/ufnet_msgcore/user/users_protobuf.h>
#include "protocol_websocket.h"
#include "ufsrvcmd_user_callbacks.h"
#include "staged_messages.h"
#include <uflib/ufsrvuid.h>
#include <uflib/utils_time.h>
#include <ufnetcorelib/ufnet_msgcore/session.h>
#include <ufnetcorelib/ufnet_msgcore/user/user_redis_backend.h>
#include <ufsrv_core/delegator_sessions/thread_context_type.h>
#include "command_broadcast/inter_broadcast_user_message.h"

extern __thread ThreadContext ufsrv_thread_context;

void
PrepareMarshalMessageForUserMessage(MarshalMessageEnvelopeForUser *envelope_ptr, Session *sesn_ptr, Fence *f_ptr, UfsrvEvent *event_ptr, DataMessage *data_msg_ptr_orig, enum _MessageCommand__CommandTypes command_type, enum _CommandArgs command_arg) {
  envelope_ptr->envelope->ufsrvcommand								  =	envelope_ptr->ufsrv_command_wire;

  envelope_ptr->envelope->ufsrvcommand->msgcommand		  =	envelope_ptr->message_command;
  envelope_ptr->envelope->ufsrvcommand->ufsrvtype			  =	UFSRV_COMMAND_WIRE__UFSRV_TYPE__UFSRV_MESSAGE;
  envelope_ptr->envelope->ufsrvcommand->header				  =	envelope_ptr->header;

  envelope_ptr->message_command->header									=	envelope_ptr->header;

  if (IS_PRESENT(envelope_ptr->user_records)) {
    envelope_ptr->message_command->to										=	envelope_ptr->user_records;
    envelope_ptr->message_command->to[0]								=	envelope_ptr->user_record;
    envelope_ptr->message_command->n_to									=	1;
  }

//  envelope_ptr->message_command->originator = MakeUserRecordFromSessionInProto (sesn_ptr, envelope_ptr->originator, PROTO_USER_RECORD_MINIMAL, PROTO_USER_RECORD_BYREF);

  envelope_ptr->envelope->sourceufsrvuid		=	UFSRV_SOURCE_ORIGIN;
  envelope_ptr->envelope->timestamp					=	GetTimeNowInMillis(); envelope_ptr->envelope->has_timestamp = 1;

  envelope_ptr->header->when								=	envelope_ptr->envelope->timestamp; 	envelope_ptr->header->has_when = 1;
  envelope_ptr->header->cid									=	SESSION_ID(sesn_ptr); 							envelope_ptr->header->has_cid = 1;
  envelope_ptr->header->command							=	command_type;
  envelope_ptr->header->args								=	command_arg;												envelope_ptr->header->has_args = 1;

  if (IS_PRESENT(envelope_ptr->fence_records)) {
    envelope_ptr->message_command->fences							=	envelope_ptr->fence_records;
    envelope_ptr->message_command->fences[0]					=	envelope_ptr->fence_record;
    envelope_ptr->message_command->n_fences						=	1;
  }

  if (IS_PRESENT(event_ptr)) {
    envelope_ptr->header->when_eid					=	event_ptr->when; 					envelope_ptr->header->has_when_eid = 1;
    envelope_ptr->header->eid								=	event_ptr->eid; 					envelope_ptr->header->has_eid = 1;
    envelope_ptr->header->gid								=	event_ptr->gid; 					envelope_ptr->header->has_gid = 1;
  }

  envelope_ptr->header->when								=	envelope_ptr->envelope->timestamp; 					envelope_ptr->header->has_when = 1;

  if (IS_PRESENT(data_msg_ptr_orig)) {
    envelope_ptr->header->when_client				=	data_msg_ptr_orig->ufsrvcommand->msgcommand->header->when;
    envelope_ptr->header->has_when_client		=	1;
    envelope_ptr->header->args_client				=	PROTO_MESSAGECOMMAND_HEADER_ARGS(data_msg_ptr_orig);
    envelope_ptr->header->has_args_client		=	1;
  }

}

Envelope__Type GetProtocolType(MessageCommand *msg_ptr, void(^on_ready)(Envelope__Type))
{
	if (likely(msg_ptr->n_messages > 0 && IS_PRESENT(msg_ptr->messages[0]))) {
		on_ready((Envelope__Type)msg_ptr->messages[0]->protocol_type);
    return (Envelope__Type)msg_ptr->messages[0]->protocol_type;
	}

  on_ready(ENVELOPE__TYPE__UNKNOWN);
  return ENVELOPE__TYPE__UNKNOWN;
}

/**
 * @brief Collect the fence members that a message should be fanned out to, excluding the originator and members who block them.
 */
bool
MessageCommandCollectFenceTargetUids(unsigned long fid, unsigned long originator_uid, CollectionResult *out)
{
  if (IS_EMPTY(out)) return false;

  FenceUidsCollectorCtx ctx = {
      .use_filter = true,
      .uids       = &originator_uid,
      .uids_sz    = 1,
      .exclude    = true
  };

  const CollectionDescriptorV2 *collection_descriptor = ProvideUidListFenceMembersCollectionDescriptor();

  if (!FenceMembersCollectUids(collection_descriptor, fid, &ctx, out)) {
    syslog(LOG_DEBUG, "%s {pid:'%lu', fid:'%lu', uid_originator:'%lu', seen:'%zu', malformed:'%zu', filtered:'%zu'}: ERROR: COULD NOT COLLECT FENCE TARGET UIDS",
           __func__, pthread_self(), fid, originator_uid, ctx.n_seen, ctx.n_malformed, ctx.n_filtered);

    return false;
  }

  if (!out->count) return true;

  unsigned long *uids = out->items;
  bool *blocked = calloc(out->count, sizeof(*blocked));
  if (!blocked) goto policy_error;

  bool checked = UserRedisShareListContainsBatch(THREAD_CONTEXT_SESSION_CACHEBACKEND, SHARELIST_BLOCKED,
                                                uids, out->count, originator_uid, blocked);
  size_t allowed = 0;
  if (checked) {
    for (size_t i = 0; i < out->count; i++) {
      if (!blocked[i]) uids[allowed++] = uids[i];
    }
  }
  free(blocked);
  if (!checked) goto policy_error;

  out->count = allowed;
  return true;

policy_error:
  CollectionResultFree(collection_descriptor, out, NULL);
  return false;
}

UFSRVResult *
MarshalMessageToFenceTargets(InstanceContextForSession *ctx_sesn_ptr, WebSocketMessage *wsm_ptr_received, DataMessage *data_msg_ptr_received, unsigned long fid, FenceEvent *fence_event_ptr, const CollectionDescriptor *targets_ptr, const CollectionDescriptor *target_uids_ptr, CollectionDescriptor *delivery_results_ptr)
{
  Session *sesn_ptr = AS_SESSION(ctx_sesn_ptr->sesn_ptr);

  unsigned long fence_call_flags = (FENCE_CALLFLAG_SEARCH_BACKEND | FENCE_CALLFLAG_HASH_FENCE_LOCALLY |
                                    FENCE_CALLFLAG_KEEP_FENCE_LOCKED | FENCE_CALLFLAG_LOCK_FENCE_BLOCKING);

  FindFenceById(NULL, fid, fence_call_flags);
  InstanceHolderForFence *instance_f_ptr = (InstanceHolderForFence *)THREAD_CONTEXT_UFSRV_RESULT_USERDATA;

  if (IS_EMPTY(instance_f_ptr)) {
    int fence_rescode = (THREAD_CONTEXT_UFSRV_RESULT_TYPE_ERR) ? (int)THREAD_CONTEXT_UFSRV_RESULT_CODE_
                                                               : RESCODE_FENCE_DOESNT_EXIST;
    _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, fence_rescode)
  }

  Fence *f_ptr = FenceOffInstanceHolder(instance_f_ptr);

  /* the fence object is shared and may be mutated by any thread: take the fields the envelope needs
     while it is locked, then let go before the fan-out. Recipients come from the staged snapshot,
     never from the fence member list. */
  bool          fence_lock_already_owned = (THREAD_CONTEXT_UFSRV_RESULT_CODE_ == RESCODE_PROG_LOCKED_BY_THIS_THREAD);
  unsigned long fence_id                 = FENCE_ID(f_ptr);
  time_t        fence_msg_expiry         = FENCE_MSG_EXPIRY(f_ptr);

  if (!fence_lock_already_owned) FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, THREAD_CONTEXT_UFSRV_RESULT_PTR);

	Envelope          command_envelope = ENVELOPE__INIT;
	CommandHeader     header           = COMMAND_HEADER__INIT;
	UfsrvCommandWire  ufsrv_command    = UFSRV_COMMAND_WIRE__INIT;
	MessageCommand    message_command  = MESSAGE_COMMAND__INIT;
	FenceRecord       fence_record;
	FenceRecord      *fence_records[1];
	UserRecord        user_record_originator;

	MarshalMessageEnvelopeForMessage envelope_marshal = {
	    .envelope               = &command_envelope,
	    .ufsrv_command_wire     = &ufsrv_command,
	    .message_command        = &message_command,
	    .header                 = &header,
	    .fence_record           = &fence_record,
	    .fence_records          = fence_records,
	    .user_record_originator = &user_record_originator,
	};

	/* packaging is owned by the delivery module: this site only supplies the context and the args */
	PrepareMarshalMessageForMessage(&envelope_marshal, sesn_ptr, fence_id, fence_msg_expiry, fence_event_ptr,
	                                data_msg_ptr_received, MESSAGE_COMMAND__COMMAND_TYPES__SAY, COMMAND_ARGS__SYNCED, true);

	size_t targets_sz = (IS_PRESENT(targets_ptr) && IS_PRESENT(targets_ptr->collection)) ? targets_ptr->collection_sz : 0;
	bool   have_target_uids = IS_PRESENT(target_uids_ptr) && IS_PRESENT(target_uids_ptr->collection) && (target_uids_ptr->collection_sz >= targets_sz);

  for (size_t i = 0; i < targets_sz; i++) {
    InstanceHolderForSession *target = (InstanceHolderForSession *)targets_ptr->collection[i];
    Session *recipient = SessionOffInstanceHolder(target);
    size_t result_index = have_target_uids ? sCollectionUidAt(target_uids_ptr, i) : i;
    UFSRVResult *result = MessageCommandDeliverEnvelopeToUser(ctx_sesn_ptr, target, wsm_ptr_received,
                                                             fence_id, &command_envelope);
    sStagedDeliveryResultRecord(delivery_results_ptr, result_index, SESSION_USERID(recipient), result);
  }

	DestructFenceRecordsProto(fence_records, message_command.n_fences, false, false);

	_RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_SUCCESS, RESCODE_UFSRV_INTERBROADCAST)
}

UFSRVResult *
MarshalMessageToFence(InstanceContextForSession *ctx_sesn_ptr, WebSocketMessage *wsm_ptr_received, DataMessage *data_msg_ptr_received, InstanceContextForFence *ctx_f_ptr, FenceEvent *fence_event_ptr)
{
  Session *sesn_ptr = AS_SESSION(ctx_sesn_ptr->sesn_ptr);
  Fence   *f_ptr    = ctx_f_ptr->f_ptr;

	FenceRawSessionList raw_session_list = {0};

	GetRawMemberUsersListForFence(sesn_ptr, ctx_f_ptr->instance_f_ptr, FENCE_CALLFLAG_INCLUDE_REMOTE_SESSIONS, &raw_session_list);//no locking

	/* read the id before dropping the lock: the fence object is shared and may be mutated */
	unsigned long fence_id = FENCE_ID(f_ptr);

	if (!ctx_f_ptr->lock_already_owned)	{
	  FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, SESSION_RESULT_PTR(sesn_ptr));
	  ctx_f_ptr->is_locked = false;
	}

	CollectionDescriptor targets_collection = {
	    .collection    = AS_COLLECTION_TYPE(raw_session_list.sessions),
	    .collection_sz = raw_session_list.sessions_sz
	};

	UFSRVResult *res_ptr = MarshalMessageToFenceTargets(ctx_sesn_ptr, wsm_ptr_received, data_msg_ptr_received,
	                                                    fence_id, fence_event_ptr,
	                                                    &targets_collection, NULL, NULL);

	DestructFenceRawSessionList(&raw_session_list, false);

	return res_ptr;
}

UFSRVResult *
MarshalFenceMessageToUser(InstanceContextForSession *ctx_sesn_ptr, InstanceHolderForSession *instance_sesn_ptr_target, WebSocketMessage *wsm_ptr_received, unsigned long fid, Envelope *command_envelope_ptr)
{
  Session *sesn_ptr = AS_SESSION(ctx_sesn_ptr->sesn_ptr);
  Session *sesn_ptr_target = IS_PRESENT(instance_sesn_ptr_target)? SessionOffInstanceHolder(instance_sesn_ptr_target) : NULL;

	CommandHeader *command_header_ptr	=	command_envelope_ptr->ufsrvcommand->header;
	command_header_ptr->cid						=	SESSION_ID((sesn_ptr_target?sesn_ptr_target:sesn_ptr)); command_header_ptr->has_cid = 1;

	UfsrvCommandMarshallingDescriptor ufsrv_description = {command_header_ptr->eid, fid, command_header_ptr->when, &EnvelopeMetaData, command_envelope_ptr, command_header_ptr->gid};

#ifdef __UF_TESTING
	syslog (LOG_DEBUG, "%s {pid:'%lu', o:'%p', cid:'%lu', cid_target:'%lu' uname_target:'%s', fid:'%lu'} Sending Message to User ", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr),
				SESSION_ID((sesn_ptr_target? sesn_ptr_target : sesn_ptr)), SESSION_USERNAME((sesn_ptr_target? sesn_ptr_target : sesn_ptr)), fid);
#endif

  UFSRVResult *res_ptr = UfsrvCommandInvokeUserCommand(ctx_sesn_ptr,
                                (IS_PRESENT(instance_sesn_ptr_target)? (&(InstanceContextForSession) {instance_sesn_ptr_target, AS_UFSRV_SESSION(sesn_ptr_target)}) : NULL),
                                IS_EMPTY(wsm_ptr_received)? (&(WebSocketMessage){.request=NULL, .type=WEB_SOCKET_MESSAGE__TYPE__REQUEST}) : wsm_ptr_received,
                                NULL, &ufsrv_description, uMSG_V1_IDX);

  if (IS_PRESENT(res_ptr))  return res_ptr;

	_RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_SUCCESS, RESCODE_PROG_NULL_POINTER)

}

bool
IsContactSharing(Session *sesn_ptr, UserContactRecord **contact_records_ptr)
{
  bool          is_allowed_status   = true;
  UserContactRecord *contact_record_ptr = contact_records_ptr[0];
  if (contact_record_ptr->has_ufsrvuid && contact_record_ptr->ufsrvuid.data != NULL && contact_record_ptr->ufsrvuid.len == CONFIG_MAX_UFSRV_ID_SZ) {
    bool          lock_already_owned  = false;
    unsigned long sesn_call_flags     =	(CALL_FLAG_LOCK_SESSION|CALL_FLAG_LOCK_SESSION_BLOCKING|
                                         CALL_FLAG_HASH_SESSION_LOCALLY|CALL_FLAG_HASH_UID_LOCALLY| CALL_FLAG_HASH_USERNAME_LOCALLY|
                                         CALL_FLAG_ATTACH_FENCE_LIST_TO_SESSION|CALL_FLAG_REMOTE_SESSION);

    GetSessionForThisUserByUserId(sesn_ptr, UfsrvUidGetSequenceId((const UfsrvUid *)contact_record_ptr->ufsrvuid.data), &lock_already_owned, sesn_call_flags);
    InstanceHolderForSession *instance_sesn_ptr_shared = (InstanceHolderForSession *)THREAD_CONTEXT_UFSRV_RESULT_USERDATA;
    if (IS_PRESENT(instance_sesn_ptr_shared)) {
      Session *sesn_ptr_shared = SessionOffInstanceHolder(instance_sesn_ptr_shared);
      if (!IsUserOnShareListContacts(sesn_ptr_shared, sesn_ptr)) {
        is_allowed_status = false;
      }

      if (!lock_already_owned)				SessionUnLockCtx(THREAD_CONTEXT_PTR, AS_UFSRV_SESSION(sesn_ptr_shared), __func__);
    }
  }

  return is_allowed_status;

}

UFSRVResult *
MarshalCommandToUser(InstanceHolderForSession *instance_sesn_ptr, InstanceHolderForSession *instance_sesn_ptr_target, WebSocketMessage *wsm_ptr_received, Envelope *command_envelope_ptr, unsigned req_cmd_idx)
{
  CommandHeader *command_header_ptr	=	command_envelope_ptr->ufsrvcommand->msgcommand->header;

  UfsrvCommandMarshallingDescriptor ufsrv_descpription = {command_header_ptr->eid, 0, command_header_ptr->when, &EnvelopeMetaData, command_envelope_ptr};

  UfsrvCommandInvokeUserCommand(&(InstanceContextForSession) {instance_sesn_ptr, AS_UFSRV_SESSION(SessionOffInstanceHolder(instance_sesn_ptr))},
                                IS_EMPTY(instance_sesn_ptr_target)? NULL : &(InstanceContextForSession) {instance_sesn_ptr_target, AS_UFSRV_SESSION(SessionOffInstanceHolder(instance_sesn_ptr_target))},
                                IS_EMPTY(wsm_ptr_received)? (&(WebSocketMessage){.request=NULL, .type=WEB_SOCKET_MESSAGE__TYPE__REQUEST}) : wsm_ptr_received, NULL, &ufsrv_descpription, req_cmd_idx);

  _RETURN_RESULT_SESN(SessionOffInstanceHolder(instance_sesn_ptr), NULL, RESULT_TYPE_SUCCESS, RESCODE_PROG_NULL_POINTER)

}
