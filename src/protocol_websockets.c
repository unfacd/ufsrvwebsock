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
#include <ufnetcorelib/ufnet_msgcore/gpc_utils.h>
#include <ufnetcorelib/ufnet_msgcore/misc.h>
#include <ufsrv_core/system/thread_utils.h>
#include <uflib/recycler/recycler.h>
#include <uflib/net/utils_net.h>
#include <uflib/buffer_descriptor/buffer_descriptor.h>
#include <ufnetcorelib/ufnet_core/protocol/protocol_io.h>
#include "protocol_websocket.h"
#include "protocol_websocket_io.h"
#include "protocol_websocket_routines.h"
#include <ufnetcorelib/ufnet_msgcore/type_providers/type_provider_fence.h>
#include <ufnetcorelib/ufnet_msgcore/fence/fence.h>
#include <ufnetcorelib/ufnet_core/user_prefs/user_prefs.h>
#include "ufsrvcmd_parser.h"
#include "data_ufsrvcmd_callbacks.h"//array of indexed callbacks
#include "ufsrvcmd_user_data.h"//array of ufsrvcmd for users indexed callbacks
#include <ufsrv_core/delegator_sessions/sessions_delegator_type.h>
#include <ufnetcorelib/ufnet_core/ufsrvcore_utils.h>
#include <ufnetcorelib/ufnet_core/ufsrv_worker_user_data_type.h>
#include <ufnetcorelib/ufnet_msgcore/crypto_zk.h>
#include <ufsrv_core/delegator_sessions/delegator_session_worker_thread.h>
#include <ufsrv_core/ratelimit_v2/ratelimit.h>
#include <ufsrv_core/http/http_request.h>
#include <ufnetcorelib/ufnet_core/user_prefs/user_prefs_fence.h>
#include <uflib/ufsrvuid.h>

#include "inter_broadcast_fence.h"
#include <session_auth_routing.h>
#include <ufnetcorelib/ufnet_core/staged_message_descriptor_type.h>
#include "command_controllers/location_command_utils.h"
#include "staged_messages.h"
#include <ufnetcorelib/ufnet_msgcore/staged_messages_ufsrv.h>
#include <ufnetcorelib/ufnet_msgcore/fence/typepool_context_data_fence_type.h>
#include <ufnetcorelib/ufnet_msgcore/fence/utils_fence.h>
#include <ufnetcorelib/ufnet_msgcore/type_providers/session_provider.h>
#include <ufsrv_core/system/log_message_literals.h>

#include "ufsrv_core/ufsrv_config/generated/ufsrv_config_accessor_decls_generated.h"

//ype and array data defined in ufsrvcmd_data.h
const UfsrvCommand *const ufsrvcmd_server_bound_callbacks_ptr = ufsrvcmd_server_bound_callbacks_array;
//service commands originating client -> server
const size_t              ufsrvcmd_maxidx = sizeof(ufsrvcmd_server_bound_callbacks_array) / sizeof(UfsrvCommand);

const UfsrvCommandResponse *const ufsrvcmd_client_bound_callbacks_ptr = ufsrvcmd_client_bound_callbacks_array;
//service commands originating server -> client
const size_t ufsrvcmd_user_maxidx = sizeof(ufsrvcmd_client_bound_callbacks_array) / sizeof(UfsrvCommandResponse);


extern __thread ThreadContext ufsrv_thread_context;

static UFSRVResult *_ParseUfsrvCommandMessage(InstanceContextForSession *, SocketMessage *sock_msg_ptr,
                                              unsigned frame_offset, size_t len) __attribute__((always_inline));
static UFSRVResult *_UfsrvCommandInvokeCommandCallback(InstanceContextForSession *, WebSocketMessage *wsm_ptr,
                                                       size_t cmdidx) __attribute__((always_inline));
static UFSRVResult *_DecodeAndParseWebSocketWireMessage(InstanceContextForSession *ctx_ptr, WebSocketMessage *wsm_ptr,
                                                        const UfsrvCommand *ufsrv_cmd_ptr, json_object *jobj_msg);

/**
 *  relay to a processing callback. replaces MarshalServiceCommandToClient() below
 *  @param payload for majority of commands this is a structure describing to how to size, pack and marshal the payload message to be sent back in accordance with agreed data representation (with the client).
 *                 Most command will use UfsrvCommandMarshallingDescriptor and message will be formatted in protocol buffer
 *  @param jobj_in Optionally, the caller can pass in, typically loaded json object, which will be unpacked and sent as string in the response. A jobj object is always passed in regardless, for convenience.
 *
 */
UFSRVResult *
UfsrvCommandInvokeUserCommand(InstanceContextForSession *ctx_ptr, InstanceContextForSession *  ctx_ptr_target,
                              WebSocketMessage *         wsm_ptr_received, json_object *jobj_in,
                              WireProtocolData *         payload, unsigned                     req_cmd_idx)
{
  if (req_cmd_idx <= ufsrvcmd_user_maxidx) {
#if UFSRVWEBSOCK_DEBUG_BUILD_L3
    syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p'}: ISSUING A UFSRVCMD TO USER USING CMDIDX: '%u'", __func__, pthread_self(),
           ctx_ptr->sesn_ptr, req_cmd_idx);
#endif

    const UfsrvCommandResponse *p = NULL;

    p = (ufsrvcmd_client_bound_callbacks_ptr + (req_cmd_idx));
    if (IS_PRESENT(p)) {
      json_object *jobj = NULL;

      //we allow the user to pass in null jobj,in which case we manage the creation and destruction of it
      if (IS_EMPTY(jobj_in)) jobj = json_object_new_object();
      else jobj                   = jobj_in;

      UFSRVResult *r_ptr = (*p->callback)(ctx_ptr, ctx_ptr_target, wsm_ptr_received, jobj, payload);

      if (!IS_EMPTY(jobj)) json_object_put(jobj);

      return r_ptr;
    }
    else {
      syslog(LOG_ERR, "%s {pid:'%lu' o:'%p'}: pointer into UfsrvCommand array invalid: terminating...", __func__,
             pthread_self(), ctx_ptr->sesn_ptr);

      _RETURN_RESULT_SESN(AS_SESSION(ctx_ptr->sesn_ptr), NULL, RESULT_TYPE_ERR, RESCODE_PROG_NULL_POINTER)
    }
  }

  _RETURN_RESULT_SESN(AS_SESSION(ctx_ptr->sesn_ptr), NULL, RESULT_TYPE_ERR, RESCODE_PROG_NULL_POINTER)
}

/**
 *  This is the final gateway before passing a message for transmission through socket. It uses the adapter envelope TransmissionMessage to abstract
 *  different type of message formats (Text, Protobuf (SocketMessageType)).
 *  @param tmsg_ptr: tmsg_ptr is not memory managed by this function. Caller must deallocate.
 *
 *  TransmissionMessage of type WebSocketMessage we allocate the msg_packed reference as opaque object and calculate length. We don't manage the
 *  deallocation of this object directly. Upon successful transmission it gets memory managed with regular free function.
 *
 *  TransmissionMessage of type Text we allocate msg_pack and calculate length. Owner must deallocate original msg string,
 *  Upon successful transmission msg_packed  gets memory managed with regular free function.
 *
 *  @dynamic_memory: allocates packed protobuf msg or strdup msg (if text).
 *   All references contained in TransmissionMessage will have been referenced in out.raw
 *  and managed separately
 *
 *  @dynamic_memory WebSocketRequest and WebSocketResponse contained objects are opaque and must be free'd where they originated
 *
 *  @call_flag CALL_FLAG_DONT_LOCK_SESSION: (NOT IN USE) if set, target session won't be locked, as it is assumed locked by the caller
 *
 *  @locked sesn_ptr_this:
 *  @locks sesn_ptr_target:		unless CALL_FLAG_DONT_LOCK_SESSION
 *  @unlocks sesn_ptr_target: unless CALL_FLAG_DONT_LOCK_SESSION
 */
int
UfsrvCommandMarshalTransmission(InstanceContextForSession *ctx_ptr_this, InstanceContextForSession *ctx_ptr_target,
                                TransmissionMessage *      tmsg_ptr, unsigned long                  call_flags)
{
  bool                       lock_already_owned = false;
  HttpRequestContext *       http_ptr           = NULL;
  InstanceContextForSession *ctx_ptr            = NULL;
  Session *                  sesn_ptr           = NULL;

  ctx_ptr  = ctx_ptr_this;
  sesn_ptr = AS_SESSION(ctx_ptr->sesn_ptr);

  if (IS_PRESENT(ctx_ptr_target)) sesn_ptr = AS_SESSION(ctx_ptr_target->sesn_ptr);

  const Protocol *protocol = UfsrvSessionProtocol(&sesn_ptr->ufsrv_session);
  bool cloud_on_io_error = protocol && protocol->protocol_controls.cloudmsg_on_ioderror;
  int rescode = 1;

  if (tmsg_ptr->type == TRANSMSG_PROTOBUF) {
    tmsg_ptr->len        = web_socket_message__get_packed_size((WebSocketMessage*)tmsg_ptr->msg);
    tmsg_ptr->msg_packed = calloc(1, tmsg_ptr->len);
    web_socket_message__pack((WebSocketMessage*)tmsg_ptr->msg, tmsg_ptr->msg_packed);
  }
  else if (tmsg_ptr->type == TRANSMSG_TEXT) {
    tmsg_ptr->msg_packed = strdup((char*)tmsg_ptr->msg); //owner should free tmsg_ptr->msg
    tmsg_ptr->len        = strlen(tmsg_ptr->msg);
  }

  //retain a copy
  unsigned char msg_packed_copy[tmsg_ptr->len];
  size_t        msg_packed_copy_sz = tmsg_ptr->len;
  memcpy(msg_packed_copy, tmsg_ptr->msg_packed, msg_packed_copy_sz);

  if (tmsg_ptr->eid > 0 && cloud_on_io_error) StoreStagedMessageCacheRecordForUser(sesn_ptr, tmsg_ptr,
    IS_PRESENT(ctx_ptr_target) ? SESSION_USERID(AS_SESSION(ctx_ptr_target->sesn_ptr)) : SESSION_USERID(sesn_ptr));

lock_target_session:
  if (IS_PRESENT(ctx_ptr_target)) {
    if (!(call_flags & CALL_FLAG_DONT_LOCK_SESSION)) { //todo: this flag is currently not use
      SessionLockRWCtx(THREAD_CONTEXT_PTR, ctx_ptr_target->sesn_ptr, _LOCK_TRY_FLAG_TRUE, __func__);

      if (_RESULT_TYPE_EQUAL(THREAD_CONTEXT_UFSRV_RESULT(THREAD_CONTEXT), RESULT_TYPE_ERR)) {
        syslog(LOG_DEBUG, "%s {pid:'%lu', eid:'%lu', gid:'%lu', cid_target:'%lu'}: ERROR: SEND LOCKED OUT", __func__, pthread_self(), tmsg_ptr->eid, tmsg_ptr->gid, SESSION_ID(AS_SESSION(ctx_ptr_target->sesn_ptr)));

        return 0;
      }
      lock_already_owned = (_RESULT_CODE_EQUAL(THREAD_CONTEXT_UFSRV_RESULT(THREAD_CONTEXT), RESCODE_PROG_LOCKED_BY_THIS_THREAD));
    }

    sesn_ptr = AS_SESSION(ctx_ptr_target->sesn_ptr); //locked herein if CALL_FLAG_DONT_LOCK_SESSION wasn't set
  }
  else {
    sesn_ptr = AS_SESSION(ctx_ptr_this->sesn_ptr); //locked in calling environment
  }

  if (SESNSTATUS_IS_SET(ctx_ptr_this->sesn_ptr->stat, SESNSTATUS_EPHEMERAL)) {
    http_ptr = GetHttpRequestContextUfsrvWorker(AS_SESSION(ctx_ptr_this->sesn_ptr));
    if (IS_PRESENT(ctx_ptr_target)) SessionLoadEphemeralMode(AS_SESSION(ctx_ptr_target->sesn_ptr));
  }
  else {
    http_ptr = GetHttpRequestContext(AS_SESSION(ctx_ptr_this->sesn_ptr));
  }

  bool send_failed = false;

  if ((SESSION_SOCKETFD(sesn_ptr) <= 0) || (SendToSocket(InstanceHolderFromClientContext(sesn_ptr), tmsg_ptr, 0) < 0)) {
    //0 means partial write, so it is not error
    rescode     = -1;
    send_failed = true;

    goto return_restore;
  }

  syslog(LOG_DEBUG, "%s {pid:'%lu', eid:'%lu', gid:'%lu', cid_target:'%lu', ack:'%d'}: NOTICE: SENT", __func__, pthread_self(), tmsg_ptr->eid, tmsg_ptr->gid, IS_PRESENT(ctx_ptr_target) ? SESSION_ID(AS_SESSION(ctx_ptr_target->sesn_ptr)) : SESSION_ID(sesn_ptr), IS_PRESENT(ctx_ptr_target) ? 0 : 1);

return_restore:
  if (IS_PRESENT(ctx_ptr_target)) {
    if (SESNSTATUS_IS_SET(ctx_ptr_this->sesn_ptr->stat, SESNSTATUS_EPHEMERAL)) SessionUnLoadEphemeralMode(
      AS_SESSION(ctx_ptr_target->sesn_ptr));
    if (!(call_flags & CALL_FLAG_DONT_LOCK_SESSION)) if (!lock_already_owned) SessionUnLockCtx(
      THREAD_CONTEXT_PTR, ctx_ptr_target->sesn_ptr, __func__);
  }

  if (tmsg_ptr->eid > 0 && cloud_on_io_error) {
    if (send_failed) {
      UfsrvCommandMarshalCloudMessagingNotification(sesn_ptr, http_ptr, NULL);
    } else {
      DeleteStagedMessageCacheRecordForUser(sesn_ptr, &(StagedMessageDescriptor){
                                              .gid    = tmsg_ptr->gid, .fid = tmsg_ptr->fid,
                                              .userid = IS_PRESENT(ctx_ptr_target) ?
                                                          SESSION_USERID(AS_SESSION(ctx_ptr_target->sesn_ptr)) :
                                                          SESSION_USERID(sesn_ptr)
                                            });
    }
  }

  return rescode;
}

/**
 * @brief Staged-delivery gateway to the socket, scoped to a single recipient.
 *
 * The staged delivery model has no originator context: each recipient is delivered to
 * independently by a job worker, so unlike @ref UfsrvCommandMarshalTransmission there is no
 * @c ctx_ptr_this to read, lock or carry a result. This function keeps the same wire behaviour and
 * target-session locking discipline, but returns the outcome explicitly instead of asking callers
 * to distinguish success from contention with a numeric code.
 *
 * @param ctx_ptr_target Recipient context. Its session is try-locked for the duration of the write.
 * @param tmsg_ptr       Message to pack and send. @c msg_packed is allocated here; the caller owns it.
 * @param call_flags     @c CALL_FLAG_DONT_LOCK_SESSION skips the target lock when the caller already holds it.
 *
 * @return Explicit result, never inferred from a sentinel integer:
 * @retval RESULT_TYPE_SUCCESS RESCODE_PROG_NULL_POINTER           the write was handed to the socket;
 * @retval RESULT_TYPE_ERR     RESCODE_PROG_WONTLOCK               the target session lock was contended;
 * @retval RESULT_TYPE_ERR     RESCODE_IO_CONNECTIONCLOSED         the socket write failed;
 * @retval RESULT_TYPE_ERR     RESCODE_PROG_MEMORY_EXHAUSTED       message packing could not allocate;
 * @retval RESULT_TYPE_ERR     RESCODE_PROG_NULL_POINTER           recipient context or message missing.
 *
 * @dynamic_memory allocates @c tmsg_ptr->msg_packed (protobuf packing or text copy).
 * @locks sesn_ptr_target: try-write, unless @c CALL_FLAG_DONT_LOCK_SESSION.
 * @unlocks sesn_ptr_target: on every path that acquired it.
 */
UFSRVResult *
UfsrvCommandMarshalTransmissionStaged(InstanceContextForSession *ctx_ptr_target, TransmissionMessage *tmsg_ptr, unsigned long call_flags)
{
  if (unlikely(IS_EMPTY(ctx_ptr_target) || IS_EMPTY(ctx_ptr_target->sesn_ptr) || IS_EMPTY(tmsg_ptr))) {
    THREAD_CONTEXT_RETURN_RESULT_ERROR(NULL, RESCODE_PROG_NULL_POINTER)
  }

  Session *sesn_ptr_target = AS_SESSION(ctx_ptr_target->sesn_ptr);
  const Protocol *protocol = UfsrvSessionProtocol(&sesn_ptr_target->ufsrv_session);
  bool cloud_on_io_error = protocol && protocol->protocol_controls.cloudmsg_on_ioderror;

  if (tmsg_ptr->type == TRANSMSG_PROTOBUF) {
    tmsg_ptr->len        = web_socket_message__get_packed_size((WebSocketMessage *)tmsg_ptr->msg);
    tmsg_ptr->msg_packed = calloc(1, tmsg_ptr->len);
    web_socket_message__pack((WebSocketMessage *)tmsg_ptr->msg, tmsg_ptr->msg_packed);
  }
  else if (tmsg_ptr->type == TRANSMSG_TEXT) {
    tmsg_ptr->msg_packed = strdup((char *)tmsg_ptr->msg);
    tmsg_ptr->len        = strlen((char *)tmsg_ptr->msg);
  }

  if (unlikely(IS_EMPTY(tmsg_ptr->msg_packed))) {
    THREAD_CONTEXT_RETURN_RESULT_ERROR(NULL, RESCODE_PROG_MEMORY_EXHAUSTED)
  }

  bool lock_already_owned = false;

  if (tmsg_ptr->eid > 0 && cloud_on_io_error) {
    StoreStagedMessageCacheRecordForUser(sesn_ptr_target, tmsg_ptr, SESSION_USERID(sesn_ptr_target));
  }

  if (!(call_flags & CALL_FLAG_DONT_LOCK_SESSION)) {
    SessionLockRWCtx(THREAD_CONTEXT_PTR, AS_UFSRV_SESSION(sesn_ptr_target), _LOCK_TRY_FLAG_TRUE, __func__);

    if (_RESULT_TYPE_EQUAL(THREAD_CONTEXT_UFSRV_RESULT(THREAD_CONTEXT), RESULT_TYPE_ERR)) {
      syslog(LOG_DEBUG, "%s {pid:'%lu', eid:'%lu', gid:'%lu', cid_target:'%lu', rescode:'%d'}: ERROR: SEND LOCKED OUT: NOT DELIVERED",
             __func__, pthread_self(), tmsg_ptr->eid, tmsg_ptr->gid, SESSION_ID(sesn_ptr_target),
             _RESULT_CODE(THREAD_CONTEXT_UFSRV_RESULT(THREAD_CONTEXT)));

      /* nothing took ownership of the packed buffer: release it before reporting the contention */
      free(tmsg_ptr->msg_packed);
      tmsg_ptr->msg_packed = NULL;
      tmsg_ptr->len        = 0;

      THREAD_CONTEXT_RETURN_RESULT_ERROR(NULL, RESCODE_PROG_WONTLOCK)
    }

    lock_already_owned = (_RESULT_CODE_EQUAL(THREAD_CONTEXT_UFSRV_RESULT(THREAD_CONTEXT),
                                             RESCODE_PROG_LOCKED_BY_THIS_THREAD));
  }

  bool send_failed = false;

  if ((SESSION_SOCKETFD(sesn_ptr_target) <= 0)
      || (SendToSocket(InstanceHolderFromClientContext(sesn_ptr_target), tmsg_ptr, 0) < 0)) {
    send_failed = true;
  } else {
    syslog(LOG_DEBUG, "%s {pid:'%lu', eid:'%lu', gid:'%lu', cid_target:'%lu'}: NOTICE: SENT", __func__, pthread_self(),
           tmsg_ptr->eid, tmsg_ptr->gid, SESSION_ID(sesn_ptr_target));
  }

  if (!(call_flags & CALL_FLAG_DONT_LOCK_SESSION) && !lock_already_owned) {
    SessionUnLockCtx(THREAD_CONTEXT_PTR, AS_UFSRV_SESSION(sesn_ptr_target), __func__);
  }

  if (tmsg_ptr->eid > 0 && cloud_on_io_error) {
    if (send_failed) {
      UfsrvCommandMarshalCloudMessagingNotification(sesn_ptr_target,
                                                    GetHttpRequestContextUfsrvWorker(sesn_ptr_target), NULL);
    } else {
      DeleteStagedMessageCacheRecordForUser(sesn_ptr_target, &(StagedMessageDescriptor){
                                              .gid    = tmsg_ptr->gid,
                                              .fid    = tmsg_ptr->fid,
                                              .userid = SESSION_USERID(sesn_ptr_target)
                                            });
    }
  }

  if (send_failed) {
    THREAD_CONTEXT_RETURN_RESULT_ERROR(NULL, RESCODE_IO_CONNECTIONCLOSED)
  }

  THREAD_CONTEXT_RETURN_RESULT_SUCCESS(NULL, RESCODE_PROG_NULL_POINTER)
}

void
ResetWebSocketProtocolSession(WebSocketSession *ws_ptr, bool is_self_destruct)
{
  if (IS_PRESENT(ws_ptr)) {
    if (IS_PRESENT(ws_ptr->protocol_header.key1)) {
      free(ws_ptr->protocol_header.key1);
      LOAD_NULL(ws_ptr->protocol_header.key1);
    }

    if (is_self_destruct) {
      free(ws_ptr);
    }
  }
}

/**
 * 	@brief: Relies on the presence of the "dry_run":true param to test if the token is valid without actually contacting the end user.
 * 	Defaults to false. Useful for checking if the app was uninstalled, hence we'd be in a position suspend user's account.
 * 	@note see https://firebase.google.com/docs/reference/fcm/rest/v1/projects.messages/send
 */
bool
IsUserCloudRegistered(Session *sesn_ptr, HttpRequestContext *http_ptr)
{
#define GCM_DRYRUN_REQUEST_JSON_ORIG	"{\"to\":\"%s\", \"dry_run\":true, \"data\": { \"notification\":\"yes\"} }"
#define GCM_DRYRUN_REQUEST_JSON "{\"validate_only\": boolean, \"message\":{ \"token\":\"%s\", \"data\": { \"notification\":\"yes\"}} }"
  int   rescode                                                                = RESCODE_PROG_NULL_POINTER;
  char  json_payload[sizeof(GCM_DRYRUN_REQUEST_JSON) + CONFIG_CM_TOKEN_SZ_MAX] = {0};
  char *gcm_id                                                                 = NULL;

  if (!IS_STR_LOADED(SESSION_CMTOKEN(sesn_ptr))) {
    ReloadCMToken(sesn_ptr, NULL);

    if (IS_EMPTY(SESSION_CMTOKEN(sesn_ptr))) goto return_db_error;
  }

  gcm_id = SESSION_CMTOKEN(sesn_ptr);
  snprintf(json_payload, (sizeof(GCM_DRYRUN_REQUEST_JSON) + CONFIG_CM_TOKEN_SZ_MAX) - 1, GCM_DRYRUN_REQUEST_JSON,
           gcm_id);


  //	int result = HttpRequestGoogleGcm(http_ptr, APIURL_GOOGLE_GCM, json_payload);
  int result = HttpRequestGoogleFcm(http_ptr, json_payload);

  if (result == 0) goto return_gcm_error;

return_success:
  return true;

return_gcm_error:
  syslog(LOG_ERR, "%s (pid:'%lu', o:'%p', cid'%lu'): ERROR: COULD NOT POST json_payload '%s'. gcm_id:'%s'", __func__,
         pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), json_payload, gcm_id);
  goto return_error;

return_db_error:
  syslog(LOG_ERR, "%s (pid:'%lu', o:'%p', cid'%lu'): ERROR: COULD NOT GET GCM_ID FOR USER", __func__, pthread_self(),
         sesn_ptr, SESSION_ID(sesn_ptr));

return_error:
  return false;
}

/**
 * @param sesn_ptr: must be Session owner with full backend access context
 */
UFSRVResult *
UfsrvCommandMarshalCloudMessagingNotification(Session *sesn_ptr, HttpRequestContext *http_ptr, WireProtocolData *data)
{
  //#define GCM_REQUEST_JSON_ORIG	"{\"to\":\"%s\", \"data\": { \"notification\":\"yes\"} }"
#define GCM_REQUEST_JSON	"{ \"message\":{ \"token\":\"%s\", \"data\": { \"notification\":\"yes\"}} }"
  int   rescode                                                         = RESCODE_PROG_NULL_POINTER;
  char  json_payload[sizeof(GCM_REQUEST_JSON) + CONFIG_CM_TOKEN_SZ_MAX] = {0};
  char *gcm_id                                                          = NULL;

  if (IS_EMPTY(SESSION_CMTOKEN(sesn_ptr))) {
    ReloadCMToken(sesn_ptr, NULL);

    if (IS_EMPTY(SESSION_CMTOKEN(sesn_ptr))) goto return_db_error;
  }

  gcm_id = SESSION_CMTOKEN(sesn_ptr);
  snprintf(json_payload, (sizeof(GCM_REQUEST_JSON) + CONFIG_CM_TOKEN_SZ_MAX) - 1, GCM_REQUEST_JSON, gcm_id);

#if 0
  asprintf(&json_payload, "{\"to\":\"%s\", "
           //"\"notification\": {\"body\":\"body_hello\", \"title\":\"title_hello\"} }",  gcm_id);
           "\"data\": { \"notification\":\"yes\"} }", gcm_id); //
#endif
  //HttpRequestContext *http_ptr=GetHttpRequestContextUfsrvWorker(sesn_ptr);

  //	int result = HttpRequestGoogleGcm(http_ptr, APIURL_GOOGLE_GCM, json_payload);
  int result = HttpRequestGoogleFcm(http_ptr, json_payload);

  if (result == 0) goto return_gcm_error;

return_success: _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_SUCCESS, rescode)

return_gcm_error:
  syslog(LOG_ERR, "%s (pid:'%lu', o:'%p', cid'%lu'): ERROR: COULD NOT POST json_payload '%s'. gcm_id:'%s'", __func__,
         pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), json_payload, gcm_id);
  statsd_inc(THREAD_CONTEXT_INSTRUMENTATION_BACKEND, "gcm.delivery.failed", 1.0,
             UfsrvConfigInstrumentationBackendServerAsPtr());
  goto return_error;

return_db_error:
  syslog(LOG_ERR, "%s (pid:'%lu', o:'%p', cid'%lu'): ERROR: COULD NOT GET GCM_ID FOR USER", __func__, pthread_self(),
         sesn_ptr, SESSION_ID(sesn_ptr));

#if 0
  //curl --header "Authorization: key=xxx" --header "Content-Type:application/json" https://android.googleapis.com/gcm/send -d "{\"registration_ids\":[\"APA91bFaBGQMQQ8XzNlo1Zw86e8e8IpbiJF1xXbFlcQul2TtcTg9oymXImN3FBhpuocYGeFILssTxh6paB8WwEfZdi1kLmqepfguB7yXnUTMc2bdw5Tq2IWR71stpmtcpFtvyNp9tGBY\"] \"message\":\"hellow\"}"
  //curl --header "Authorization: key=xxx
  //" --header "Content-Type:application/json" https://android.googleapis.com/gcm/se
  //nd -d "{\"registration_ids\":[\"APA91bFaBGQMQQ8XzNlo1Zw86e8e8IpbiJF1xXbFlcQul2Tt
  //cTg9oymXImN3FBhpuocYGeFILssTxh6paB8WwEfZdi1kLmqepfguB7yXnUTMc2bdw5Tq2IWR71stpmtc
  //pFtvyNp9tGBY\"] \"message\":\"hellow\"}" --insecure

  //response
  //{"multicast_id":6126472261424557086,"success":1,"failure":0,"canonical_ids":1,"results":[{"registration_id":"APA91bEW6clA02RI2S_4caipD1k-SotCMjCbdrwrHWeNcxAPBG7Pra3ermvKN-gn9bi_rY4l6iTEZ3gPqYZyaW5V_hpZKv9JOn1IoPo5aHTobzaAb1lJHpST0PT3Y0Dx-iTnV3eFKKWm","message_id":"0:1478614820005773%3af43603f9fd7ecd"}]}
#endif

return_error: _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, rescode)

return_generic_error:
  return _ufsrv_result_generic_error;
}

#include <ufsrv_core/delegator_sessions/workers/ufsrvworker_pool_descriptor_type.h>
#include <ufsrv_core/delegator_sessions/workers/jobworkers_utils.h>
#include <ufsrv_core/delegator_sessions/base_thread_context_data_type.h>
#include <ufnetcorelib/ufnet_msgcore/msgqueue_backend/ufsrvmsgqueue.h>
#include "msgqueue_broadcast/ufsrvcmd_broadcast.h"
#include <ufsrv_core/cache_backend/persistence.h>
#include <ufsrv_core/cache_backend/redis.h>

static UFSRVResult *
sUfsrvWorkerPoolOneoffInitialiser(WorkerPoolDescriptor *pool_descriptor)
{
  return DefaultUfsrvWorkerPoolOneoffInitialiser(pool_descriptor);
}

/**
 * @brief static type provider for ufsrv worker pool. One per server instance.
 */
static WorkerPoolDescriptor *const sGetUfsrvWorkerPoolDescriptor(oneoff_initialiser on_created)
{
  static WorkerPoolDescriptor ufsrvworker_pool;

  ufsrvworker_pool.on_created = on_created;

  return &ufsrvworker_pool;
}

/**
 * @brief Callback initialiser for a UfsrvWorker thread, applicable to ufsrvwebsock and ufsrvapi class servers.
 * @note This must be called in the context of a running thread, because it initialises thread local ThreadContext object.
 * @param thread_base_ctx_data pre-allocated context
 */
static UFSRVResult *
sUfsrvWorkerThreadDataContextInitialiser(BaseThreadContext *thread_base_ctx_data)
{
  ThreadContext *          ufsrv_thread_context_ptr = thread_base_ctx_data->worker_thread_context;

  HopscotchHashtableConfigurable *locked_objects_store = &(thread_base_ctx_data->locked_objects_store);
  if (InitLockedObjectsStore(locked_objects_store) != 0) {
    syslog(LOG_ERR, "%s: ERROR: COULD NOT INITIALISE the locked-objects store for Ufsrv Worker thread: '%lu'...",
           __func__, pthread_self());
    _exit(-1);
  }

  ufsrv_thread_context_ptr->ht_ptr = locked_objects_store;
  ufsrv_thread_context.ht_ptr      = locked_objects_store; //TBD

  InitUfsrvScheduledJobsStore(&thread_base_ctx_data->scheduled_jobs_store, 0, NULL);

  ufsrv_thread_context_ptr->res_ptr = &(thread_base_ctx_data->ufsrv_result);
  ufsrv_thread_context.res_ptr      = &(thread_base_ctx_data->ufsrv_result); //TBD

  //>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

  if (IS_PRESENT(InitialiseHttpRequestContext(&(thread_base_ctx_data->http_request_context), 0))) {
    ufsrv_thread_context_ptr->http_request_context = &(thread_base_ctx_data->http_request_context);;
    ufsrv_thread_context.http_request_context      = &(thread_base_ctx_data->http_request_context); //TBD
  }
  else {
    syslog(LOG_ERR, "%s: ERROR: COULD NOT INITIALISE HttpRequestContext for Ufsrv Worker thread: '%lu'...", __func__,
           pthread_self());
    _exit(-1);
  }

  syslog(LOG_DEBUG, "%s: SUCCESS (http_ptr:'%p'): Initialised HttpRequestContext for Ufsrv Worker thread: '%lu'...",
         __func__, &(thread_base_ctx_data->http_request_context), pthread_self());

  //>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

  InstrumentationBackend *instr_ptr = InstrumentationBackendInit(NULL, NULL, NULL, 0); //no namespace
  if (instr_ptr) {
    ufsrv_thread_context_ptr->instrumentation_backend = instr_ptr;
    ufsrv_thread_context.instrumentation_backend      = instr_ptr; //TBD
  }
  else {
    syslog(LOG_NOTICE, "%s: ERROR: COULD NOT INITIALISE INSTRUMENTATION for Ufsrv Worker thread: '%lu'...", __func__,
           pthread_self());
  }

  syslog(
    LOG_INFO,
    "%s: SUCCESS (instr_ptr:'%p'): Initialised Instrumentation Backend for Ufsrv Worker thread: '%lu' (NOT IMPLEMENTED)...",
    __func__, instr_ptr, pthread_self());

  //>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

  /* Verify the fence claim script on the job worker's assigned connection. */
  UfsrvWorkerUserData *worker_data = ufsrv_thread_context.user_data;
  if (RedisVerifyScriptLoaded(worker_data->fence_cachebackend, REDIS_SCRIPT_SHA1_FENCE_CLAIM,
                             "FENCE RECORD CLAIM") != 0) {
    syslog(LOG_ERR, "%s: Fence record claim script is unavailable", __func__);
    _exit(-1);
  }

  THREAD_CONTEXT_RETURN_RESULT_SUCCESS(NULL, RECODE_NONE)
}

/**
 * @brief Initialise and launch the job workers subsystem (one per instance).
 * Each thread gets its own ThreadContext object, carved out of a slap allocator fixed for the size of thread pool workers
 * set-size.
 */
static void
sLaunchUfsrvWorkers()
{
  WorkerPoolDescriptor *const pool_descriptor = sGetUfsrvWorkerPoolDescriptor(sUfsrvWorkerPoolOneoffInitialiser);
  size_t                      pool_sz = UfsrvConfigUfsrvWorkersThreadPool();
  pool_descriptor->workers_pool_config_descriptor.up_status = POOL_STATE_UP;
  pool_descriptor->workers_pool_config_descriptor.pool_sz = pool_sz;
  //  pool_descriptor->sessions_delegator = sd_ptr;
  pool_descriptor->thread_handlers.on_instantiated = sUfsrvWorkerThreadDataContextInitialiser;
  RegisterJobWorkersConfigurationDescriptor(&pool_descriptor->workers_pool_config_descriptor);

  if (IS_PRESENT(pool_descriptor->on_created)) {
    pool_descriptor->on_created(pool_descriptor);
  }

  LaunchUfServerWorkerThreads(pool_descriptor, sizeof(ThreadContext));
}

#include "scheduled_job_fences.h"

/**
 * @brief Grouping of startup scheduled jobs
 */
static void
sInitialiseStartupScheduledJobs()
{
  InitialiseScheduledJobTypeForSessionsTimeouts();
  InitialiseScheduledJobTypeForOrphanedFences();
  InitialiseScheduledJobTypesForGpcAuthorization();
}

static int
TypePoolPutInitCallback_Fence(InstanceHolder *data_ptr, ContextData *context_data, unsigned long call_flags)
{
  //perhaps early in the initialisation we failed so there nothing to destruct
  if (IS_EMPTY(context_data)) return 0;

  Fence *                   f_ptr = FenceOffInstanceHolder(data_ptr);
  TypePoolContextDataFence *data  = context_data;

  if (call_flags & FENCE_CALLFLAG_WRITEBACK_TO_DBBACKEND) {
    struct json_object *jobj_fence = JsonFormatFenceForDbBackend(data->sesn_ptr, f_ptr, DIGESTMODE_FULL, 0/*lock*/);
    const char *        jstr_fence = json_object_to_json_string(jobj_fence);

    DbBackendInsertFenceRecord(f_ptr, jstr_fence);
    json_object_put(jobj_fence);
  }

  if (call_flags & FENCE_CALLFLAG_EVICT_FROM_CACHEBACKEND) {
    CacheBackendRemCacheRecordForFence(data->sesn_ptr, f_ptr);
    InterBroadcastFenceDestruct(data->sesn_ptr, f_ptr, NULL, 0);
  }

  DestructUserFence(data->sesn_ptr, &((InstanceContextForFence){
                      data->fence_data.instance_f_ptr, FenceOffInstanceHolder(data->fence_data.instance_f_ptr)
                    }), call_flags);

  //not relevant to check if lock owned already
  //note: if FENCE_CALLFLAG_LOCK was passed, _Destruct takes care of unlocking
  if (call_flags & FENCE_CALLFLAG_UNLOCK_FENCE) {
    FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, THREAD_CONTEXT_UFSRV_RESULT(THREAD_CONTEXT));
  }

  return 0; //success
}

UFSRVResult *
proto_websocket_protocol_init_callback(const Protocol *proto_ptr)
{
  syslog(LOG_INFO, "%s: Initialising protocol: '%s' ...", __func__, proto_ptr->protocol_name);

  RecyclerPoolHandle *pool = InitSessionRecyclerTypePool(
      GetSessionInstantiator(), UfsrvConfigMemorySpecsAllocationGroups(),
      UfsrvConfigMemorySpecsSessionsPerAllocationGroup());
  if (!ProtocolSetSessionPool(proto_ptr, pool)) {
    syslog(LOG_ERR, "%s: could not register the pool for '%s'", __func__, proto_ptr->protocol_name);
    return _ufsrv_result_generic_error;
  }
  return _ufsrv_result_generic_success;
}

bool
UfsrvWebsockStartServices(void)
{
  UfsrvDelegatorWorkersUserData worker_data = {0};
  if (!InitialiseUfsrvDelegatorWorkersUserData(&worker_data)) {
    syslog(LOG_ERR, "%s: Could not initialise delegator worker backends", __func__);
    return false;
  }

  SessionsDelegatorInitSpecs delegator_specs = {
      .ufsrv_delegator_userdata = &worker_data,
      .on_init_crypto = InitialiseUfsrvCrypto,
      .on_init_main_thread_backends = InitialiseUfsrvMainThreadBackends,
      .on_init_hashtable      = SessionDelegatorInitHashTables,
      .build_msgqueue_backend = BuildConnectionHandleForMessageQueueBackend,
      .msgqueue_executor = WorkerThreadMsgQueueParserExecutor,
  };

  SessionsDelegator *sd_ptr = InitialiseDelegator(&delegator_specs);
  if (!sd_ptr) {
    DestroyUfsrvDelegatorWorkersUserData(&worker_data);
    syslog(LOG_ERR, "%s: Could not initialise delegator", __func__);
    return false;
  }

  InitUFSRV(sd_ptr, &delegator_specs);
  LaunchSessionsDelegatorThread(sd_ptr);

  sLaunchUfsrvWorkers();
  LaunchTimerManagerThread(&sInitialiseStartupScheduledJobs);
  //ensure RegisterJobWorkersConfigurationDescriptor() is called beforehand
  LaunchFileloaderServiceThread(UfsrvConfigFileLoaderServiceAsPtr());

  InitFenceRecyclerTypePoolWithProvider(&(RecyclerPoolOps){.poolop_initput_callback = TypePoolPutInitCallback_Fence},
                                        UfsrvConfigMemorySpecsSessionsPerAllocationGroup());
  //_CONF_SESNMEMSPECS_ALLOC_GROUP_SZ(masterptr)
  InitFenceStateDescriptorRecyclerTypePool(UfsrvConfigMemorySpecsSessionsPerAllocationGroup());

  InitialiseMaserFenceRegistries();
  InitialiseMasterUserRegistry();

  RegisterFenceUserPreferencesSource();
  RegisterUserPreferencesSource();

  return true;
}

UFSRVResult *
proto_websocket_hanshake_callback(InstanceHolder *instance_sesn_ptr, SocketMessage *sock_msg_ptr, unsigned callflags,
                                  int **          comeback)
{
  Session *    sesn_ptr = SessionOffInstanceHolder(instance_sesn_ptr);
  UFSRVResult *res_ptr  = ProcessIncomingWsHandshake(sesn_ptr, sock_msg_ptr);
  switch (res_ptr->result_type) {
  case RESULT_TYPE_ERR:
    if (res_ptr->result_code == RESCODE_IO_WOULDBLOCK) {
    }
    else if (res_ptr->result_code == RESCODE_IO_CONNECTIONCLOSED) {
      syslog(LOG_DEBUG, "%s {pid:'%lu', th_ctx:'%p', o:'%p', cid:'%lu'}:  COULD NOT HANDSHAKE: connection closed",
             __func__, pthread_self(), THREAD_CONTEXT_PTR, sesn_ptr, SESSION_ID(sesn_ptr));
    }
    else if (res_ptr->result_code == RESCODE_PROTOCOL_WSHANDSHAKE) {
      syslog(LOG_DEBUG, "%s {pid:'%lu', th_ctx:'%p', o:'%p', cid:'%lu'}:  COULD NOT HANDSHAKE: parsing error", __func__,
             pthread_self(), THREAD_CONTEXT_PTR, sesn_ptr, SESSION_ID(sesn_ptr));
    }

    SuspendSession(instance_sesn_ptr, SOFT_SUSPENSE);

    _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_PROTOCOL_WSHANDSHAKE)
  }

  return HandlePostSuccessfulIncomingHandshake(instance_sesn_ptr, sock_msg_ptr);
  // _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_SUCCESS, RESCODE_PROTOCOL_WSHANDSHAKE)
}


/*
 * 	@brief: Session has just been successfully handshaked both ways and authenticated. This session could be brand new, migrated, unsuspended etc...
 */
UFSRVResult *
proto_websocket_post_hanshake_callback(InstanceHolder *instance_sesn_ptr, SocketMessage *sock_msg_ptr,
                                       unsigned        callflags)
{
  Session *sesn_ptr = SessionOffInstanceHolder(instance_sesn_ptr);

  //TODO: authenticated_user check for return
  //MarshalServiceCommandToClient(sesn_ptr_hashed, NULL, 1);//authenticated user
  DetermineUserLocationByServer(sesn_ptr, GetHttpRequestContext(sesn_ptr), 0); //broadcast event

  UfsrvCommandInvokeUserCommand(&(InstanceContextForSession){instance_sesn_ptr, AS_UFSRV_SESSION(sesn_ptr)}, NULL, NULL, NULL, NULL,
                                uSTATESYNC_V1_IDX);

  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_SUCCESS, RESCODE_PROG_NULL_POINTER)
}

static inline UFSRVResult *
_UfsrvCommandInvokeCommandCallback(InstanceContextForSession *ctx_ptr, WebSocketMessage *wsm_ptr, size_t cmdidx)
{
  Session *sesn_ptr = AS_SESSION(ctx_ptr->sesn_ptr);

  //type safety guaranteed
  if (cmdidx <= ufsrvcmd_maxidx) {
    const UfsrvCommand *p = NULL;
    UFSRVResult *       ufsrv_res;

    struct json_object *    jobj = NULL;
    enum json_tokener_error jerr;
    const char *            json_str;
    size_t                  json_str_len = 0;

    //we have a json payload included
    if ((json_str = wsm_ptr->request->verb)) {
      json_str_len = strlen(json_str);
    }

    if (json_str_len) {
      struct json_tokener *jtok;

#ifdef __UF_FULLDEBUG
      syslog(LOG_DEBUG, "%s (pid:'%lu', cid:'%lu'): TOKENISING JSON: '%s'", __func__, pthread_self(),
             SESSION_ID(sesn_ptr), json_str);
#endif
      jtok = json_tokener_new();

      do {
        jobj = json_tokener_parse_ex(jtok, json_str, json_str_len);
      }
      while ((jerr = json_tokener_get_error(jtok)) == json_tokener_continue);

      if (jerr != json_tokener_success) {
        syslog(LOG_NOTICE, "%s (pid:'%lu' cid:'%lu'): JSON tokeniser Error: '%s'...", __func__, pthread_self(),
               SESSION_ID(sesn_ptr), json_tokener_error_desc(jerr));

        jobj = NULL;
      }

      json_tokener_free(jtok);
    }

    if ((p = (ufsrvcmd_server_bound_callbacks_ptr + cmdidx))) {
      if ((json_str_len) && (jerr == json_tokener_success)) {
        //>>>>>>>>>>>>>>>>>>>>>>>>>>>>
        // ufsrv_res = (*p->callback)(ctx_ptr, wsm_ptr, jobj);
        ufsrv_res = _DecodeAndParseWebSocketWireMessage(ctx_ptr, wsm_ptr, p, jobj);
        //>>>>>>>>>>>>>>>>>>>>>>>>>>>

        json_object_put(jobj);
      }
      else {
        ufsrv_res = (*p->callback)(ctx_ptr, wsm_ptr, NULL);
      }

      return ufsrv_res;
    }
  }

  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_PROG_NULL_POINTER)
}

/**
 * '{"destination":"+61414821358","messages":[{"body":null,"content":"Mwi..3voB","destinationDeviceId":1,"destinationRegistrationId":0,"type":3}],"relay":null,"timestamp":1471608758401}'
 * @param wire_message
 */
static UFSRVResult *
_DecodeAndParseWebSocketWireMessage(InstanceContextForSession *ctx_ptr, WebSocketMessage * wsm_ptr,
                                    const UfsrvCommand *       ufsrv_cmd_ptr, json_object *jobj_msg)
{
  int          jobj_array_size     = 0;
  const char * destination         = json_object_get_string(json__get(jobj_msg, "destination"));
  time_t       msg_timestamp       = json_object_get_int64(json__get(jobj_msg, "timestamp"));
  json_object *jobj_messages_array = json__get(jobj_msg, "messages");

  Session *sesn_ptr = AS_SESSION(ctx_ptr->sesn_ptr);

  if (unlikely((IS_EMPTY(jobj_messages_array)))) {
    syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p'}: ERROR: No message found", __func__, pthread_self(), ctx_ptr->sesn_ptr);
    goto request_error;
  }
  if (unlikely((jobj_array_size = json_object_array_length(jobj_messages_array)) == 0)) {
    syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p'}: ERROR: zero message list found", __func__, pthread_self(), sesn_ptr);
    goto request_error;
  }

  int          i;
  json_object *jobj_entry = NULL;
  UFSRVResult *res        = _ufsrv_result_generic_error;

  for (i = 0; i < jobj_array_size; i++) {
    jobj_entry = json_object_array_get_idx(jobj_messages_array, i);
    if (IS_PRESENT(jobj_entry)) {
      int device_id = json_object_get_int(json__get(jobj_entry, "destinationDeviceId"));
      int rego_id   = json_object_get_int(json__get(jobj_entry, "destinationRegistrationId"));

      const char *msg_body_b64 = json_object_get_string(json__get(jobj_entry, "content"));

      if (!IS_STR_LOADED(msg_body_b64)) {
        syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', device_id:'%d', rego_id:'%d'}: ERROR: MSG BODY IS MISSING...",
               __func__, pthread_self(), sesn_ptr, device_id, rego_id);
        break;
      }

      int   rc_len              = 0;
      char *msg_content_decoded = (char*)base64_decode((unsigned char*)msg_body_b64, strlen(msg_body_b64), &rc_len);
      if (IS_EMPTY(msg_content_decoded)) {
        syslog(
          LOG_DEBUG, "%s {pid:'%lu', o:'%p', device_id:'%d', rego_id:'%d'}: ERROR: COULD NOT b64-DECODE BODY MSG...",
          __func__, pthread_self(), sesn_ptr, device_id, rego_id);
        break;
      }

      DataMessage *dm_ptr = data_message__unpack(NULL, rc_len, (unsigned char*)msg_content_decoded);
      if (unlikely(IS_EMPTY(dm_ptr))) {
        free(msg_content_decoded);
        syslog(
          LOG_DEBUG,
          "%s {pid:'%lu', o:'%p', device_id:'%d', rego_id:'%d'}: ERROR: COULD NOT PROTOBUF-UNPACK DataMessage...",
          __func__, pthread_self(), sesn_ptr, device_id, rego_id);
        break;
      }

      res = (*ufsrv_cmd_ptr->callback)(ctx_ptr, wsm_ptr, dm_ptr);
      //      FenceCommand *fcmd_ptr = dm_ptr->ufsrvcommand->fencecommand;
      //
      //      if (!(IS_EMPTY(fcmd_ptr))) {
      ////        UfsrvApiIntraBroadcastMessage (sesn_ptr, _WIRE_PROTOCOL_DATA(dm_ptr), MSGCMD_FENCE, INTRA_SEMANTICS, (const unsigned char *)msg_body_b64);
      //      } else {
      //        syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', device_id:'%d'}: ERROR: NO FENCE COMMAND WAS PRESENT", __func__, pthread_self(), sesn_ptr, device_id);
      //        free (msg_content_decoded);
      //        data_message__free_unpacked(dm_ptr, NULL);
      //        break;
      //      }

      free(msg_content_decoded);
      data_message__free_unpacked(dm_ptr, NULL);
    }
  }

  return res;

request_error:
  return _ufsrv_result_generic_error;
}

//TODO: we kind of mirroring what MarshalServiceCommandToClient is doing. We should consolidate at some stage

//TODO: frame_offset and len parameters are no longer applicable. frame_offset should be used as callflags by the calling environment
UFSRVResult *
proto_websocket_msg_callback(InstanceHolder *instance_sesn_ptr, SocketMessage *sock_msg_ptr, unsigned frame_offset,
                             size_t          len)
{
  ssize_t  read_result;
  Session *sesn_ptr = SessionOffInstanceHolder(instance_sesn_ptr);

  read_result = ReadFromSocket(instance_sesn_ptr, sock_msg_ptr, frame_offset);
  //frame_offset is 'callflags' passed by the calling environment

  if (read_result > 0) goto process_framed_decoded_msg;
  else if (read_result == 0) {
    //if we are reading a very large frame, the first fragment will be seen by decode_hybi, which will return 0
    //subsequent reads will detect missing size and will continue to report zero until  full frame is recieved up to 65k which is the max frame zize we allowe for Websocket
    syslog(
      LOG_DEBUG,
      "%s (pid:'%lu' cid:'%lu'): COULD NOT FIND A COMPLETE FRAME: NO MSG WILL BE PROCESSED: (frame_coundt='%lu' missing_msg_size: '%lu') RETURNING...",
      __func__, pthread_self(), SESSION_ID(sesn_ptr), sock_msg_ptr->frame_count, sock_msg_ptr->missing_msg_size);

    _RETURN_RESULT_SESN(sesn_ptr, sesn_ptr, RESULT_TYPE_SUCCESS, RESCODE_IO_FRAGMENTATION)
  }
  else {
read_error:
    switch (read_result) {
    case -1: _RETURN_RESULT_SESN(sesn_ptr, sesn_ptr, RESULT_TYPE_ERR, RESCODE_IO_CONNECTIONCLOSED) //suspended

    case -2: _RETURN_RESULT_SESN(sesn_ptr, sesn_ptr, RESULT_TYPE_ERR, RESCODE_LOGIC_CANTLOCK)

    case -3: //user sent termination in WS
      _RETURN_RESULT_SESN(sesn_ptr, sesn_ptr, RESULT_TYPE_ERR, RESCODE_IO_CONNECTIONCLOSED)

    case -4: _RETURN_RESULT_SESN(sesn_ptr, sesn_ptr, RESULT_TYPE_ERR, RESCODE_IO_DECODED)
    //this fatal couldent base64 decode: suspend

    case -5: _RETURN_RESULT_SESN(sesn_ptr, sesn_ptr, RESULT_TYPE_ERR, RESCODE_IO_MISSGINGFRAMEDATA)
    //benign error couldn't process frame because of incomplete frame data

    default: _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_PROG_NULL_POINTER)
    }
  }

process_framed_decoded_msg:

  //TODO: this may be redundant as read_result==0 condition above should indicate the same condition
  if (sock_msg_ptr->frame_count == 0) {
    syslog(
      LOG_DEBUG,
      "%s (pid:'%lu' o:'%p', frameindex_multiples:'%lu'): NO FRAME WAS FOUND: NO MSG WILL BE PROCESSED: (missing_msg_size: '%lu') RETURNING...",
      __func__, pthread_self(), sesn_ptr, sock_msg_ptr->frame_index_multiples, sock_msg_ptr->missing_msg_size);

    //TODO: FIX: this should be considered an error condition?
    _RETURN_RESULT_SESN(sesn_ptr, sesn_ptr, RESULT_TYPE_SUCCESS, RESCODE_IO_FRAGMENTATION)
  }

  //start framed_decoded_msg
  {
    unsigned     loop_counter = sock_msg_ptr->frame_count;
    size_t       len          = sock_msg_ptr->frame_index[0]; //length of the first frame payload
    int          frame_offset = 0;
    UFSRVResult *ufcmd_result;

    while (loop_counter--) {
      syslog(
        LOG_DEBUG,
        "%s {pid:'%lu', o:'%p',  cid:'%lu'}: PARSE ITERATION: '%d': PAYLOAD length: '%lu' READING OFFSET '%d'...",
        __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), loop_counter + 1, len, frame_offset);

      ufcmd_result = _ParseUfsrvCommandMessage(&(InstanceContextForSession){instance_sesn_ptr, AS_UFSRV_SESSION(sesn_ptr)}, sock_msg_ptr,
                                               frame_offset, len);
      switch (ufcmd_result->result_type) {
      case RESULT_TYPE_SUCCESS:
        if (ufcmd_result->result_code == RESCODE_IO_MSGPARSED) {
#ifdef __UF_FULLDEBUG
          syslog(LOG_DEBUG, "%s (pid:'%lu' cid:'%lu'): SUCCESSFULLY INVOKED MSG CALLBACK...", __func__, pthread_self(),
                 SESSION_ID(sesn_ptr));
#endif
        }
        break;

      case RESULT_TYPE_ERR:
        switch (ufcmd_result->result_code) {
        case RESCODE_IO_MSGPARSED:
          //processing or logical error
          syslog(LOG_NOTICE, "%s (pid:'%lu', o:'%p', cid:'%lu'):  COULD NOT parse message...", __func__, pthread_self(),
                 sesn_ptr, SESSION_ID(sesn_ptr));

          //TODO: implement trip threshold
          //ignore requestand send error status back to client
          break; //to loop_processing_body

        case RESCODE_LOGIC_NOCMND:
          break;

        default:
          //network error
          if (SESNSTATUS_IS_SET(sesn_ptr->ufsrv_session.stat, SESNSTATUS_SUSPENDED)) {
            syslog(
              LOG_DEBUG,
              "%s (pid:'%lu' o:'%p'): SESSION HAS BEEN SUSPENDED AMID ITERATION: PARSE ITERATION: '%d': READING OFFSET '%d': DISCONTINUING LOOP",
              __func__, pthread_self(), sesn_ptr, loop_counter + 1, frame_offset);

            //TODO: do we break? or save the frame_offset where we left?we need to retry last msg?
          }
        }      //inner switch
        break; //RESULT_TYPE_ERR
      }        //outer switch

loop_processing_body:
      //additional check just in case
      if (SESNSTATUS_IS_SET(sesn_ptr->ufsrv_session.stat, SESNSTATUS_SUSPENDED)) {
        syslog(
          LOG_DEBUG,
          "%s (pid:'%lu' o:'%p'): OUTSIDE CONDITIONAL: SESSION HAS BEEN SUSPENDED AMID ITERATION: PARSE ITERATION: '%d': READING OFFSET '%d'...",
          __func__, pthread_self(), sesn_ptr, loop_counter + 1, frame_offset);

        //TODO: do we break? or save the frame_offset where we left?we need to retry last msg?
        break;
      }

      if (loop_counter) {
        //only do it if loop_counter is >0 ie we have a subsequent run
        frame_offset += len; //remember last len

        if (frame_offset == sock_msg_ptr->processed_msg_size) {
          syslog(
            LOG_DEBUG,
            ">>>> %s (pid:'%lu' o:'%p'): MESG OFFSET EQUALS TOTAL MSG SIZE: loop_counter: '%u'. frame_offset='%d'. BREAKING LOOP",
            __func__, pthread_self(), sesn_ptr, loop_counter + 1, frame_offset);
          break;
        }

        //				len = sock_msg_ptr->frame_index[sock_msg_ptr->frame_count - loop_counter];//Increment offset to go past the '0'then read the length upto the next '0'
        len = *(sock_msg_ptr->frame_index + (sock_msg_ptr->frame_count - loop_counter));
      }
    } //while

    //reset buffer
cleanup_exit_block:
    free(sock_msg_ptr->_processed_msg);
    free(sock_msg_ptr->frame_index);
    sock_msg_ptr->_processed_msg        = 0;
    sock_msg_ptr->processed_msg_size    = 0;
    sock_msg_ptr->frame_count           = 0;
    sock_msg_ptr->frame_index_multiples = 0;

    _RETURN_RESULT_SESN(sesn_ptr, sesn_ptr, RESULT_TYPE_SUCCESS, RESCODE_IO_MSGPARSED)
  } //end framed_decoded_msg
}

UFSRVResult *
proto_websocket_decode_msg_callback(InstanceHolder *instance_sesn_ptr, SocketMessage *sm_ptr, unsigned frame_offset)
{
  unsigned int opcode = 0, left;

  Session *sesn_ptr = SessionOffInstanceHolder(instance_sesn_ptr);

  if (sm_ptr->raw_msg_size == 0) {
    syslog(LOG_DEBUG, "%s {pid:'%lu', th_ctx:'%p', o:'%p', cid:'%lu'}: Raw input buffer is empty: returning...",
           __func__, pthread_self(), THREAD_CONTEXT_PTR, sesn_ptr, SESSION_ID(sesn_ptr));

    _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_SUCCESS, RESCODE_IO_DECODED)
  }

  sm_ptr->_processed_msg = calloc(1, sm_ptr->raw_msg_size);

  //this will tokenise multiple frames on '\0'
  //>>>>>>>>>>>>>>>>>>>>>>>>
  sm_ptr->processed_msg_size = decode_hybi(sm_ptr, sm_ptr->_raw_msg, sm_ptr->raw_msg_size, sm_ptr->_processed_msg,
                                           sm_ptr->raw_msg_size, &opcode, &left);
  //>>>>>>>>>>>>>>>>>>>>>>>>

  if (opcode == 8) {
    //orderly shutdown by client. all allocated buffers are cleared
    sm_ptr->processed_msg_size = 1; //just to make sure buffer is correctly marked for destruction
    SuspendSession(instance_sesn_ptr, SOFT_SUSPENSE);

    _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_IO_PROTOCOL_SHUTDOWN)
  }

  if (sm_ptr->processed_msg_size < 0) {
decoding_error:
    syslog(
      LOG_ERR,
      "%s {pid:'%lu', o:'%p', cid:'%lu'}: ERROR: COULD NOT DECODE MESSAGE. SocketMessage will be destructed. Connection  will be suspended",
      __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr));

    //buffer data is now unbalanced: get rid of it now in case the client becomes unsuspended and uses the buffer again
    sm_ptr->processed_msg_size = 1; //just to make sure buffer is correctly marked for destruction
    DestructSocketMessage(sm_ptr);  //this will free sm_ptr->_processed_msg even if its corresponding size is -1

    SuspendSession(instance_sesn_ptr, SOFT_SUSPENSE);

    _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_IO_DECODED)
  }

  //we have incomplete frame. cur_pos point to the begining of the incomplete frame in _processed_msg
  if (sm_ptr->missing_msg_size) {
    syslog(
      LOG_DEBUG,
      "%s (pid:'%lu' o:'%p', cid:'%lu'): PARTIAL FRAME from client: amount left to be read: '%ld'. raw_msg_cur_pos='%ld'. holding_buffer_msg_size: '%ld' (This amount will be copied into holding_buffer). Raw buffer will be freed.",
      __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), sm_ptr->missing_msg_size, sm_ptr->raw_msg_cur_pos,
      sm_ptr->holding_buffer_msg_size);

    //how much we managed to collect so far
    sm_ptr->holding_buffer = (unsigned char*)strndup((char*)(sm_ptr->_raw_msg + sm_ptr->raw_msg_cur_pos),
                                                     sm_ptr->holding_buffer_msg_size);
  }
  else {
#ifdef __UF_FULLDEBUG
    syslog(
      LOG_DEBUG, "%s (pid:'%lu' cid:'%lu'): SUCESS: READ AND DECODED MESSAGE. size:  '%ld'. Raw buffer will be freed.",
      __func__, pthread_self(), SESSION_ID(sesn_ptr), sm_ptr->processed_msg_size);
#endif
  }

  //we are good. This buffer has been successfully utilised wholly or in part
  free(sm_ptr->_raw_msg);
  sm_ptr->_raw_msg     = NULL;
  sm_ptr->raw_msg_size = 0;

  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_SUCCESS, RESCODE_IO_DECODED)
}

UFSRVResult *
proto_websocket_encode_msg_callback(InstanceHolderForSession *instance_sesn_ptr, SocketMessage *sock_msg_ptr,
                                    unsigned                  frame_offset)
{
  Session *sesn_ptr = SessionOffInstanceHolder(instance_sesn_ptr);

  sesn_ptr->ufsrv_session.ssptr->socket_msg_out.processed_msg_size = encode_hybi(&(sesn_ptr->ufsrv_session.ssptr->socket_msg_out),
                                                                   sesn_ptr->ufsrv_session.ssptr->socket_msg_out._raw_msg,
                                                                   sesn_ptr->ufsrv_session.ssptr->socket_msg_out.raw_msg_size,
                                                                   sesn_ptr->ufsrv_session.ssptr->socket_msg_out._processed_msg,
                                                                   sesn_ptr->ufsrv_session.ssptr->socket_msg_out.processed_msg_size,
                                                                   2); //binary not text 1);

  /*
   * Two possibilities:
   * for straight-through both raw and processed are the same reference: so we can free processed and zero out
   * For WebSocket: raw and processed are both allocated so both must be freed
   * However, this block is concerned with Websocket condition only
   */
  if (sesn_ptr->ufsrv_session.ssptr->socket_msg_out.processed_msg_size < 0) {
    syslog(LOG_NOTICE, "%s: {pid:'%lu', th_ctx:'%p',  cid:'%lu'}: ERROR: COULD NOT ENCODE MSG...", __func__,
           pthread_self(), THREAD_CONTEXT_PTR, SESSION_ID(sesn_ptr));
    _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_IO_ENCODED)
  }

  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_SUCCESS, RESCODE_IO_ENCODED)
}

/**
 * @brief: read processed buffer based on a given offset. This should be a complete, correctly decoded Websocket frame of size len.
 *
 * @returns RESULT_TYPE_ERR, RESCODE_LOGIC_NOCMND: command directive was missing. It is not an error for which we'd
 * terminate, but it should be counted as a "trip" indicator and a threashld is applied before terminating
 *
 * @returns RESULT_TYPE_SUCCESS, RESCODE_IO_MSGPARSED: this a desired success case
 *
 * @return RESULT_TYPE_ERR, RESCODE_IO_PROTOUNPACKING: packaging error
 */
static inline UFSRVResult *
_ParseUfsrvCommandMessage(InstanceContextForSession *ctx_ptr, SocketMessage *sock_msg_ptr, unsigned frame_offset,
                          size_t                     len)
{
  WebSocketMessage *wsm_ptr;

  Session *sesn_ptr = AS_SESSION(ctx_ptr->sesn_ptr);

  if (*sock_msg_ptr->_processed_msg == '\0') {
    syslog(LOG_DEBUG, LOGSTR_PROTO_COMMAND_ENVELOPE_MISSING, __func__, pthread_self(), sesn_ptr,
           SESSION_USERNAME(sesn_ptr), frame_offset, LOGCODE_PROTO_COMMAND_ENVELOPE_MISSING);

    _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_LOGIC_NOCMND)
  }

  wsm_ptr = web_socket_message__unpack(NULL, len, sock_msg_ptr->_processed_msg + frame_offset);
  if (!IS_EMPTY(wsm_ptr)) {
    char *command = wsm_ptr->command;
    if (unlikely(!IS_STR_LOADED(command))) {
      syslog(LOG_DEBUG, LOGSTR_PROTO_COMMAND_MISSING, __func__, pthread_self(), sesn_ptr, SESSION_USERNAME(sesn_ptr),
             LOGCODE_PROTO_COMMAND_MISSING);

      _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_LOGIC_NOCMND)
    }

    switch (wsm_ptr->type) {
    case WEB_SOCKET_MESSAGE__TYPE__REQUEST: ;
      //keep this empty statement for the declaration below to compile, or enclose with brackets
#ifdef __UF_FULLDEBUG
      syslog(
        LOG_DEBUG, "%s (pid:'%lu', o:'%p', uname:'%s', command:'%s', id:'%lu'): Received WebSocket Request Command...",
        __func__, pthread_self(), sesn_ptr, SESSION_USERNAME(sesn_ptr), command, wsm_ptr->request->id);
#endif
      //>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
      int cmdidx = UfsrvCommandIndexGet(command);
      //>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

      _UfsrvCommandInvokeCommandCallback(ctx_ptr, wsm_ptr, cmdidx);

      break;

    //TODO: this shuld be merged with request. It is separated for debugging and semantics testing only
    case WEB_SOCKET_MESSAGE__TYPE__RESPONSE:
#ifdef __UF_TESTING
      syslog(
        LOG_NOTICE,
        "%s: (pid:'%lu', o:'%p', cid:'%lu'): >> UNSUPPORTED WEB_SOCKET_MESSAGE__TYPE__RESPONSE MESSAGE '%s'", __func__,
        pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), wsm_ptr->response->message);
#endif
      break;

    default:
      syslog(LOG_DEBUG, LOGSTR_PROTO_COMMAND_TYPE_ERROR, __func__, pthread_self(), sesn_ptr, SESSION_USERNAME(sesn_ptr),
             command, wsm_ptr->type, LOGCODE_PROTO_COMMAND_TYPE_ERROR);
    }

    _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_SUCCESS, RESCODE_IO_MSGPARSED)
  }
  else {
    syslog(LOG_NOTICE, LOGSTR_PROTO_COMMAND_UNPACK_ERROR, __func__, pthread_self(), sesn_ptr,
           SESSION_USERNAME(sesn_ptr), frame_offset, len, LOGCODE_PROTO_COMMAND_UNPACK_ERROR);

    _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_IO_PROTOUNPACKING)
  }
}

/**
 * @brief Lifecycle callback for when a new session object is memory allocated. This is the pure data object, no instanceHolder
 *
 * @param call_flag: signifies whether the callback is being invoked from the context of the recycler. if set indicates the object is being initialised from a recycler GET request; ie recurring initiallisation. Otherwise indicates
 * object is being initialised at memory allocation time; ie once-off
 * setup
 *
 */
UFSRVResult *
proto_websocket_init_session_callback(ClientContextData *ctx_data_ptr, unsigned call_flags)
{
  Session *sesn_ptr = (Session*)ctx_data_ptr;

  if (call_flags == 0) {
    SESSION_PROTOCOL_SESSION_DATA(sesn_ptr) = calloc(1, sizeof(WebSocketSession));
  }
  else {
    SESSION_PROTOCOL_SESSION_DATA(sesn_ptr) = calloc(1, sizeof(WebSocketSession));
  }

  //TODO: at the moment the session object needs to be recreated regardless of recycler origin. Future optimisation

common_init: _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_SUCCESS, RESCODE_PROG_NULL_POINTER)
}

/**
 *
 * @param instance_sesn_ptr
 * @param call_flags signifies whether the callback is being invoked from the context of the recycler. If set indicates the object is being initialised from a recycler GET request; ie recurring initiallisation. Otherwise indicates
 * object is being initialised at memory allocation time; ie once-off
 * @return
 */
UFSRVResult *
proto_websocket_reset_callback(InstanceHolder *instance_sesn_ptr, unsigned call_flags)
{
  Session *sesn_ptr = SessionOffInstanceHolder(instance_sesn_ptr);

  WebSocketSession *ws_ptr = (WebSocketSession*)SESSION_PROTOCOL_SESSION_DATA(sesn_ptr);

  if (call_flags == 0) {
    if (IS_PRESENT(ws_ptr)) {
      syslog(
        LOG_DEBUG, "%s {pid:'%lu', th_ctx:'%p', o:'%p', cid: '%lu'}: WEBSOCKET SESSION: DESTRUCTING OBJECT INSTANCE...",
        __func__, pthread_self(), THREAD_CONTEXT_PTR, sesn_ptr, SESSION_ID(sesn_ptr));
    }
  }
  else {
    if (IS_PRESENT(ws_ptr)) {
      //when invoked from within the recyrler SessionReset will have been invoked, clearing protocol session
      syslog(LOG_DEBUG, "%s {pid:'%lu', th_ctx:'%p', o:'%p', cid: '%lu'}: WEBSOCKET SESSION: PUSHING TO RECYCLER...",
             __func__, pthread_self(), THREAD_CONTEXT_PTR, sesn_ptr, SESSION_ID(sesn_ptr));
    }
  }

  ResetWebSocketProtocolSession(ws_ptr, true);
  LOAD_NULL(SESSION_PROTOCOL_SESSION_DATA(sesn_ptr));

  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_NOOP, RESCODE_PROG_NULL_POINTER)
}

#define BASELINE_REFCOUNT	3

/**
 * 	@brief: invoked everytime service timeout check is performed. This is invoked outside the normal Session worky context, hence
 * 	why ephemeral mode is loaded for the session.
 *
 * 	@param sens_ptr: this is a locally hashed session, but not necessarily connected. It will have ephemeral mode loaded.
 *
 * 	@locked sesn_ptr: locked by the caller unless the flag SESN_WONTLOCK is passed
 *
 * 	@worker: UfsrvWorker
 * 	@access_context sesn_ptr: loaded in ephemeral mode
 *
 * 	@returns on returning error, session will be suspended by the caller
 */
UFSRVResult *
proto_websocket_service_timeout_callback(InstanceHolder *instance_sesn_ptr, time_t now, unsigned long call_flags)
{
  bool     recycle_flag   = false;
  bool     suspended_flag = false;
  Session *sesn_ptr       = SessionOffInstanceHolder(instance_sesn_ptr);

  //local session with no handshake bit: perhaps connected then failed to complete the handshake
  if (!SESNSTATUS_IS_SET(sesn_ptr->ufsrv_session.stat, SESNSTATUS_REMOTE) && (!
    SESNSTATUS_IS_SET(sesn_ptr->ufsrv_session.stat, SESNSTATUS_HANDSHAKED))) {
    // || !SESNSTATUS_IS_SET(sesn_ptr->ufsrv_session.stat, SESNSTATUS_CONNECTED)))
#ifdef __UF_FULLDEBUG
    syslog(
      LOG_DEBUG,
      "%s {pid:'%lu', o:'%p', cid:'%lu'}: CHECKING for Dangling Session with incomplete handshake (timeout='%u')...",
      __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), SessionsDelegatorPtr()->user_timeouts.unauthenticated);
#endif
    //incomplete handshake
    if ((SessionsDelegatorPtr()->user_timeouts.unauthenticated > 0) && (now - sesn_ptr->ufsrv_session.when_serviced_end) >
      SessionsDelegatorPtr()->user_timeouts.unauthenticated) {
      //even with dangling conditions are satisfied, we could still have lingering references, for example in the delegator-worker pipe, where a recycle request could have been raised before the session was subsequently suspended due to io error
      size_t session_refcount = SessionGetReferenceCount(instance_sesn_ptr);
      if (session_refcount > BASELINE_REFCOUNT) {
        //3 is the baseline state, as CheckSessionIdleTime() increases refcount. IMPORTANT: DECREASE TO '1' if CheckSessionIdleTime() removes the increase
        syslog(
          LOG_DEBUG,
          "%s {pid:'%lu', o:'%p', cid:'%lu', refcount:'%lu'}: FOUND REFERENCE-COUNTED Dangling Session WITH: Returning...",
          __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), session_refcount);
        goto _return_noop;
      }

      syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', cid:'%lu', refcount:'%lu'}: FOUND Dangling Session: RECYCLING...",
             __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), session_refcount);
      if (SuspendSession(instance_sesn_ptr, HARD_SUSPENSE)) recycle_flag = true;
    }
  }
  else if (SESNSTATUS_IS_SET(sesn_ptr->ufsrv_session.stat, SESNSTATUS_HANDSHAKED) && SESNSTATUS_IS_SET(
    sesn_ptr->ufsrv_session.stat, SESNSTATUS_CONNECTED)) {
#ifdef __UF_FULLDEBUG
    syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p', cid:'%lu'): CHECKING for Connected-Idling Session (timeout='%u')...",
           __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), SessionsDelegatorPtr()->user_timeouts.connected,
           SESSION_ID(sesn_ptr));
#endif
    if ((SessionsDelegatorPtr()->user_timeouts.connected > 0) && (now - sesn_ptr->ufsrv_session.when_serviced_end) >
      SessionsDelegatorPtr()->user_timeouts.connected) {
      syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p', cid:'%lu'): FOUND Connected Idling Session: SUSPENDING...", __func__,
             pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr));

      if (SuspendSession(instance_sesn_ptr, SOFT_SUSPENSE)) suspended_flag = true;
    }
    else if (!SESNSTATUS_IS_SET(sesn_ptr->ufsrv_session.stat, SESNSTATUS_LOCATED)) {
#ifdef __UF_FULLDEBUG
      syslog(LOG_DEBUG, "%s (pid:'%lu'): CHECKING for LOCATION-LESS Session (timeout='%u'): 'cid='%lu'...", __func__,
             pthread_self(), SessionsDelegatorPtr()->user_timeouts.locationless, sesn_ptr->ufsrv_session.session_id);
#endif
      if ((SessionsDelegatorPtr()->user_timeouts.locationless > 0) && (now - sesn_ptr->ufsrv_session.when_serviced_end) >
        SessionsDelegatorPtr()->user_timeouts.locationless) {
        syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p', cid:'%lu'): FOUND Connected-Location-less Session: SUSPENDING...",
               __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr));

        if (SuspendSession(instance_sesn_ptr, SOFT_SUSPENSE)) suspended_flag = true;
      }
    }
    /*else
    {
      syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p', cid:'%lu'): UNABLE TO ASCERTAIN THE STATE OF A CONNECTED SESSION: Forcibly suspending...",
          __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr));

      SuspendThisSession (NULL, sesn_ptr, 0); suspended_flag=true;
    }*/
  }
  else if (SESNSTATUS_IS_SET(sesn_ptr->ufsrv_session.stat, SESNSTATUS_SUSPENDED) && (!SESNSTATUS_IS_SET(
    sesn_ptr->ufsrv_session.stat, SESNSTATUS_REMOTE))) {
#ifdef __UF_FULLDEBUG
    syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p', cid:'%lu'): CHECKING for Suspended Session (timeout:'%u'): 'cid='%lu'...",
           __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), SessionsDelegatorPtr()->user_timeouts.suspended);
#endif
    if ((SessionsDelegatorPtr()->user_timeouts.suspended > 0) && (now - sesn_ptr->ufsrv_session.when_serviced_end) >
      SessionsDelegatorPtr()->user_timeouts.suspended) {
      //even with above conditions are satisfied, we could still have lingering references, for example in the delegator-worker pipe, where a recycle request could have been raised before the session was subsequently suspended due to io error
      size_t session_refcount = SessionGetReferenceCount(instance_sesn_ptr);
      if (session_refcount > BASELINE_REFCOUNT) {
        //3 is the baseline state, as CheckSessionIdleTime() increases refcount. IMPORTANT: DECREASE TO '1' if CheckSessionIdleTime() removes the increase
        syslog(
          LOG_DEBUG,
          "%s {pid:'%lu', th_ctx:'%p', o:'%p', cid:'%lu', refcount:'%lu'}: FOUND REFERENCE-COUNTED Suspended Idling Session: Returning...",
          __func__, pthread_self(), THREAD_CONTEXT_PTR, sesn_ptr, SESSION_ID(sesn_ptr), session_refcount);
        goto _return_noop;
      }

      syslog(
        LOG_DEBUG, "%s (pid:'%lu', o:'%p', cid:'%lu', refcount:'%lu'): FOUND Suspended Idling Session: RECYCLING: ...",
        __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), session_refcount);

      if (SuspendSession(instance_sesn_ptr, HARD_SUSPENSE)) recycle_flag = true;
    }
  }
  else if (SESNSTATUS_IS_SET(sesn_ptr->ufsrv_session.stat, SESNSTATUS_REMOTE)) {
    if (SESNSTATUS_IS_SET(sesn_ptr->ufsrv_session.stat, SESNSTATUS_REMOTE_CONNECTED)) {
      syslog(LOG_DEBUG, "%s (pid:'%lu', th_ctx:'%p', o:'%p', cid:'%lu'): FOUND REMOTE CONNECTED Session: IGNORING...",
             __func__, pthread_self(), THREAD_CONTEXT_PTR, sesn_ptr, SESSION_ID(sesn_ptr));
    }
    else {
#if 0
      //TODO: to be enabled at future date
      syslog(
        LOG_DEBUG, "%s (pid:'%lu', o:'%p', cid:'%lu'): FOUND REMOTE _NON_ CONNECTED &&& SUSPENDED Session: CLEARING...",
        __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr)); ClearLocalSessionCache(
        NULL, sesn_ptr, CALL_FLAG_DONT_BROADCAST_FENCE_EVENT); //we dont unlock
#endif
    }
  }
  else {
    syslog(
      LOG_DEBUG,
      "%s {pid:'%lu', th_ctx:'%p', o:'%p', cid:'%lu'}: UNABLE TO ASCERTAIN THE STATE OF ORPHAN SESSION: Forcibly suspending...",
      __func__, pthread_self(), THREAD_CONTEXT_PTR, sesn_ptr, SESSION_ID(sesn_ptr));

    if (SuspendSession(instance_sesn_ptr, SOFT_SUSPENSE)) suspended_flag = true;
  }

  if (recycle_flag) _RETURN_RESULT_SESN(sesn_ptr, sesn_ptr, RESULT_TYPE_SUCCESS, RESCODE_SESN_HARDSPENDED)

  if (suspended_flag) _RETURN_RESULT_SESN(sesn_ptr, sesn_ptr, RESULT_TYPE_SUCCESS, RESCODE_SESN_SOFTSPENDED)

_return_noop: _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_NOOP, RESCODE_PROG_NULL_POINTER)
}

UFSRVResult *
proto_websocket_error_callback(InstanceHolder *instance_sesn_ptr, unsigned call_flags)
{
  Session *sesn_ptr = SessionOffInstanceHolder(instance_sesn_ptr);

_return_noop: _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_NOOP, RESCODE_PROG_NULL_POINTER)
}

UFSRVResult *
proto_websocket_recycler_error_callback(InstanceHolder *instance_sesn_ptr, unsigned call_flags)
{
  Session *sesn_ptr = SessionOffInstanceHolder(instance_sesn_ptr);

_return_noop: _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_NOOP, RESCODE_PROG_NULL_POINTER)
}

UFSRVResult *
proto_websocket_close_callback(InstanceHolder *instance_sesn_ptr)
{
  Session *sesn_ptr = SessionOffInstanceHolder(instance_sesn_ptr);

_return_noop: _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_NOOP, RESCODE_PROG_NULL_POINTER)
}


UFSRVResult *
proto_websocket_msgqueue_topics_callback(UFSRVResult *res_ptr)
{
  static char *               topic_names[UFSRV_MAX_BROADCAST_ID];
  static CollectionDescriptor collection_topics;

  for (size_t i = 0; i < UFSRV_MAX_BROADCAST_ID; i++) {
    UfsrvCommandBroadcast *broadcast_ptr = GetBroadcastDescriptorByTopicId((enum enumUfsrvBroadcastTopicIds)i);
    topic_names[i]                       = broadcast_ptr->topic_name;
  }

  collection_topics.collection_sz = UFSRV_MAX_BROADCAST_ID;
  collection_topics.collection    = (collection_t**)topic_names;

  _RETURN_RESULT_RES(res_ptr, &collection_topics, RESULT_TYPE_SUCCESS, RESCODE_PROTOCOL_DATA)
}

UFSRVResult *
proto_websocket_generate_session_id_callback(UFSRVResult *res_ptr, ClientContextData *context_data)
{
  unsigned long session_id = GenerateSessionIdGlobally();
  if (session_id == 0) {
    _RETURN_RESULT_RES(res_ptr, NULL, RESULT_TYPE_ERR, RESCODE_PROTOCOL_DATA)
  }
  else {
    _RETURN_RESULT_RES(res_ptr, (uintptr_t)(unsigned long)session_id, RESULT_TYPE_SUCCESS, RESCODE_PROTOCOL_DATA)
  }
}

UFSRVResult *
proto_websocket_on_ratelimited_callback(InstanceHolder *   instance_holder, unsigned stage,
                                        ClientContextData *context_data)
{
  static const char body[] = "<h1>Rate limit exceeded</h1>";
  UFSRVResult *     result_ptr;
  BufferDescriptor *bd;

  (void)stage;
  (void)context_data;

  bd = calloc(1, sizeof(*bd));
  if (IS_EMPTY(bd)) return NULL;
  BufferDescriptorInit(bd, 256);

  if (IS_EMPTY(instance_holder)) {
    result_ptr = calloc(1, sizeof(*result_ptr));
  }
  else {
    Session *sesn_ptr = SessionOffInstanceHolder(instance_holder);
    result_ptr        = IS_PRESENT(sesn_ptr) ? &(sesn_ptr->ufsrv_session.result) : NULL;
  }

  if (IS_EMPTY(result_ptr)) {
    BufferDescriptorRelease(bd);
    free(bd);
    return NULL;
  }

  if (IS_EMPTY(bd->data) || BufferDescriptorAppendFormatted(
    bd, "HTTP/1.1 429 Too Many Requests\r\n" "Content-Type: text/html; charset=utf-8\r\n" "Content-Length: %zu\r\n"
    "Connection: close\r\n" "\r\n" "%s", sizeof(body) - 1, body) != BUFFER_DESCRIPTOR_OK) {
    BufferDescriptorRelease(bd);
    free(bd);
    if (IS_EMPTY(instance_holder)) free(result_ptr);
    return NULL;
  }

  result_ptr->result_user_data = bd;
  result_ptr->result_code      = RESCODE_USER_RATELIMIT_EXCEEDED;
  result_ptr->result_type      = RESULT_TYPE_ERR;

  return result_ptr;
}
