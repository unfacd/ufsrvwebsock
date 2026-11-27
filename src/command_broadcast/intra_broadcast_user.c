//
// Created by devops on 6/7/26.
//

#ifdef HAVE_CONFIG_UFSRVWEBSOCK_H
#include "config_ufsrvwebsock.h"
#endif

#include "protocol_websocket.h"

#include "main_protocols.h"
#include <uflib/utils_time.h>
#include <ufnetcorelib/ufnet_msgcore/session.h>
#include "command_broadcast/intra_broadcast_user.h"
#include "msgqueue_broadcast/ufsrvcmd_broadcast.h"
#include "command_controllers/command_controllers.h"
#include <ufnetcorelib/ufnet_core/user_prefs/user_prefs_fence.h>
#include "nportredird.h"
#include <uflib/ufsrvuid.h>
#include <ufsrv_core/delegator_sessions/delegator_session_worker_thread.h>
#include <ufnetcorelib/ufnet_msgcore/type_providers/session_provider.h>

#include "ufsrv_core/ufsrv_config/generated/ufsrv_config_accessor_decls_generated.h"

extern __thread ThreadContext ufsrv_thread_context;

static int _VetrifyUserCommandForIntra(WireProtocolData *);

/**
 * 	@brief: Main controller for handling INTRA broadcasts for UserComands.
 */
int
HandleIntraBroadcastForUser(MessageQueueMessage *mqm_ptr, UFSRVResult *res_ptr, unsigned long call_flags)
{
  int 			rc					=	0;
  long long timer_start	=	GetTimeNowInMicros(),
          timer_end;

  if (unlikely(mqm_ptr->has_ufsrvuid == 0)) goto return_error_undefined_ufsrvuid;

  if ((rc = _VetrifyUserCommandForIntra(_WIRE_PROTOCOL_DATA(mqm_ptr->wire_data->ufsrvcommand->usercommand))) < 0)	goto return_final;

  unsigned long userid = UfsrvUidGetSequenceId((const UfsrvUid *)(mqm_ptr->ufsrvuid.data));

  InstanceHolderForSession  *instance_sesn_ptr_carrier	=	InstantiateCarrierSession(NULL, WORKERTYPE_UFSRVWORKER, SESSION_CALLFLAGS_EMPTY, UfsrvWebsockProtocol());
  if (IS_EMPTY(instance_sesn_ptr_carrier))	{
    rc = -4;
    goto return_final;
  }

  unsigned long sesn_call_flags				=	(	CALL_FLAG_LOCK_SESSION|CALL_FLAG_LOCK_SESSION_BLOCKING|
                                             CALL_FLAG_HASH_SESSION_LOCALLY|CALL_FLAG_HASH_UID_LOCALLY| CALL_FLAG_HASH_USERNAME_LOCALLY|
                                             CALL_FLAG_ATTACH_FENCE_LIST_TO_SESSION|CALL_FLAG_REMOTE_SESSION);
  bool lock_already_owned = false;
  Session *sesn_ptr_carrier = SessionOffInstanceHolder(instance_sesn_ptr_carrier);

  GetSessionForThisUserByUserId(sesn_ptr_carrier, userid, &lock_already_owned,    sesn_call_flags);
  InstanceHolderForSession *instance_sesn_ptr_local_user = (InstanceHolderForSession *)THREAD_CONTEXT_UFSRV_RESULT_USERDATA;

  if (unlikely(IS_EMPTY(instance_sesn_ptr_local_user)))	goto return_error_unknown_uname;

  //>>> sesn_ptr_local_user IS NOW LOCKED
  Session	*sesn_ptr_local_user = SessionOffInstanceHolder(instance_sesn_ptr_local_user);

  SESSION_WHEN_SERVICE_STARTED(sesn_ptr_local_user) = time(NULL);

#ifdef __UF_TESTING
  UserCommand 				*ucmd_ptr	= mqm_ptr->wire_data->ufsrvcommand->usercommand;
  syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', cid:'%lu', nprefs:'%lu',  userid:'%lu'}: FULLY CONSTRUCTED USER COMMAND.", __func__, pthread_self(),sesn_ptr_local_user, SESSION_ID(sesn_ptr_local_user),  ucmd_ptr->n_prefs, userid);
#endif

  SessionLoadEphemeralMode(sesn_ptr_local_user);
  //>>>>>>>>><<<<<<<<<<
  CommandCallbackControllerUserCommand(instance_sesn_ptr_local_user, NULL, mqm_ptr->wire_data);
  //>>>>>>>>><<<<<<<<<<

  SESSION_WHEN_SERVICED(sesn_ptr_local_user) = time(NULL);
  SessionUnLoadEphemeralMode(sesn_ptr_local_user);
  if (!lock_already_owned)	SessionUnLockCtx(THREAD_CONTEXT_PTR, AS_UFSRV_SESSION(sesn_ptr_local_user), __func__);

  return_success:
  goto return_deallocate_carrier;

  return_error_undefined_ufsrvuid:
  syslog(LOG_DEBUG, "%s {pid:'%lu'}: ERROR: COULD NOT FIND UFSRVUID", __func__, pthread_self());
  rc = -7;
  goto return_final;

  return_error_unknown_uname:
  syslog(LOG_DEBUG, "%s {pid:'%lu', userid:'%lu'}: ERROR: COULD NOT RETRIEVE SESSION FOR USER", __func__, pthread_self(), userid);
  rc = -7;
  goto return_deallocate_carrier;

  return_deallocate_carrier:
  SessionReturnToRecycler(instance_sesn_ptr_carrier, (ContextData *)NULL, 0);

  return_final:
  timer_end = GetTimeNowInMicros();
  statsd_timing(THREAD_CONTEXT_INSTRUMENTATION_BACKEND, "delegator.ufsrv.job.command.user.elapsed_time", (timer_end-timer_start), UfsrvConfigInstrumentationBackendServerAsPtr());
  return rc;

}

/**
 * 	@brief: Verify the fitness of the UserCommand message in the context of on INTRA broadcast
 */
inline static int
_VetrifyUserCommandForIntra(WireProtocolData *data_ptr)
{
  int rc = 1;
  UserCommand *user_cmd_ptr = (UserCommand *)data_ptr;

  if (unlikely(IS_EMPTY((data_ptr))))				goto return_error_usercommand_missing;
  if (unlikely(IS_EMPTY(user_cmd_ptr->header)))	goto return_error_commandheader_missing;
  if (user_cmd_ptr->header->command == USER_COMMAND__COMMAND_TYPES__PREFERENCE) {
    if (unlikely((user_cmd_ptr->n_prefs < 1) || IS_EMPTY(user_cmd_ptr->prefs))) goto return_error_missing_prefs_definition;
  }

  return_success:
  goto return_final;

  return_error_missing_payload:
  syslog(LOG_DEBUG, "%s {pid:'%lu'}: ERROR: DATA PAYLOAD MISSING FROM MessageQueue Message", __func__, pthread_self());
  rc = -2;
  goto return_free;

  return_error_ufsrvcommand_missing:
  syslog(LOG_DEBUG, "%s {pid:'%lu'}: ERROR: COULD NOT FIND UFSRV COMMAND IN UNPACKED MESAGEQUEUE", __func__, pthread_self());
  rc = -3;
  goto return_free;

  return_error_commandheader_missing:
  syslog(LOG_DEBUG, "%s {pid:'%lu'}: ERROR: COULD NOT FIND COMMAND HEADER", __func__, pthread_self());
  rc = -8;
  goto return_free;

  return_error_usercommand_missing:
  syslog(LOG_DEBUG, "%s {pid:'%lu'}: ERROR: COULD NOT FIND USER COMMAND IN UNPACKED MESAGEQUEUE", __func__, pthread_self());
  rc = -4;
  goto return_free;

  return_error_missing_prefs_definition:
  syslog(LOG_DEBUG, "%s (pid:'%lu): ERROR: USER COMMAND DID NOT INCLUDE VALID PREFS DEFINITION", __func__, pthread_self());
  rc = -6;
  goto	return_free;

  return_free:
  return_final:
  return rc;

}
