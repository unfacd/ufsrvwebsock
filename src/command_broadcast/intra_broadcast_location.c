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
#include <uflib/utils_time.h>
#include <ufnetcorelib/ufnet_msgcore/session.h>
#endif


#include "main_protocols.h"
#include "protocol_websocket.h"

#include "command_controllers/location_command_controller.h"
#include "intra_broadcast_location.h"
#include "nportredird.h"
#include <ufnetcorelib/ufnet_msgcore/broadcast/location_broadcast.h>
#include <ufsrv_core/delegator_sessions/sessions_delegator_type.h>
#include <ufnet_msgcore/msgqueue_backend/UfsrvMessageQueue.pb-c.h>
#include <uflib/ufsrvuid.h>
#include <ufsrv_core/delegator_sessions/delegator_session_worker_thread.h>
#include <ufnetcorelib/ufnet_msgcore/type_providers/session_provider.h>

#include "ufsrv_core/ufsrv_config/generated/ufsrv_config_accessor_decls_generated.h"

extern __thread ThreadContext ufsrv_thread_context;

/**
 * 	@brief: Main interface method for broadcasting backend data model state change for Fence display name attribute.
 * 	@
 */

struct BroadcastLocationEnvelopeForMessage {
    MessageQueueMessage 			*msgqueue_msg;
    LocationCommand 					*location_command;
    LocationRecord						*location_record;
    CommandHeader 						*header;
};

typedef struct BroadcastLocationEnvelopeForMessage BroadcastLocationEnvelopeForMessage;

///// INTRA	\\\\\\

int
HandleIntraBroadcastForLocation(MessageQueueMessage *mqm_ptr, UFSRVResult *res_ptr, unsigned long call_flags)
{
  int				rc					= 0;
  long long timer_start	=	GetTimeNowInMicros();
  long long timer_end;

  if ((rc = VerifyLocationCommandForIntra(_WIRE_PROTOCOL_DATA(mqm_ptr->wire_data->ufsrvcommand->locationcommand))) < 0)	goto return_final;

  unsigned long userid = UfsrvUidGetSequenceId((const UfsrvUid *)(mqm_ptr->ufsrvuid.data));

  InstanceHolderForSession  *instance_sesn_ptr_carrier			=	InstantiateCarrierSession (NULL, WORKERTYPE_UFSRVWORKER, SESSION_CALLFLAGS_EMPTY, UfsrvWebsockProtocol());
  if (IS_EMPTY(instance_sesn_ptr_carrier))	{
    rc = -4;
    goto return_final;
  }

  Session *sesn_ptr_carrier = SessionOffInstanceHolder(instance_sesn_ptr_carrier);

  unsigned long sesn_call_flags				=	(	CALL_FLAG_LOCK_SESSION|CALL_FLAG_LOCK_SESSION_BLOCKING|
                                             CALL_FLAG_HASH_SESSION_LOCALLY|CALL_FLAG_HASH_UID_LOCALLY| CALL_FLAG_HASH_USERNAME_LOCALLY|
                                             CALL_FLAG_ATTACH_FENCE_LIST_TO_SESSION|CALL_FLAG_REMOTE_SESSION);

  bool lock_already_owned = false;
  GetSessionForThisUserByUserId(sesn_ptr_carrier, userid, &lock_already_owned, sesn_call_flags);
  InstanceHolderForSession *instance_sesn_ptr_local_user = (InstanceHolderForSession *)THREAD_CONTEXT_UFSRV_RESULT_USERDATA;

  if (unlikely(IS_EMPTY(instance_sesn_ptr_local_user)))	goto return_error_unknown_uname;

  Session *sesn_ptr_local_user = SessionOffInstanceHolder(instance_sesn_ptr_local_user);

  //>>> sesn_ptr_local_user IS NOW LOCKED

  SESSION_WHEN_SERVICE_STARTED(sesn_ptr_local_user) = time(NULL);

  //////////////////////////
  SessionLoadEphemeralMode(sesn_ptr_local_user);

  CommandCallbackControllerLocationCommand(&(InstanceContextForSession){instance_sesn_ptr_local_user, AS_UFSRV_SESSION(sesn_ptr_local_user)}, NULL, mqm_ptr->wire_data);

  SESSION_WHEN_SERVICED(sesn_ptr_local_user) = time(NULL);
  SessionUnLoadEphemeralMode(sesn_ptr_local_user);
  if (!lock_already_owned)	SessionUnLockCtx(THREAD_CONTEXT_PTR, AS_UFSRV_SESSION(sesn_ptr_local_user), __func__);
  //TODO: Error checking from command controller
  /////////////////////////

  return_success:
  goto return_deallocate_carrier;

  return_error_unknown_uname:
  syslog(LOG_DEBUG, "%s {pid:'%lu', userid:'%lu'}: ERROR: COULD NOT RETRIEVE SESSION FOR USER", __func__, pthread_self(), userid);
  rc = -7;
  goto return_deallocate_carrier;

  return_deallocate_carrier:
  SessionReturnToRecycler(instance_sesn_ptr_carrier, (ContextData *)NULL, 0);

  return_final:
  timer_end = GetTimeNowInMicros();
  statsd_timing(THREAD_CONTEXT_INSTRUMENTATION_BACKEND, "delegator.ufsrv.job.command.msg.elapsed_time", timer_end - timer_start, UfsrvConfigInstrumentationBackendServerAsPtr());
  return rc;

}

/**
 * 	@brief: Verify the fitness of the FenceCommand message in the context of on INTRA broadcast
 */
int
VerifyLocationCommandForIntra(WireProtocolData *data_ptr)
{
  int rc = 1;
  LocationCommand *cmd_ptr = (LocationCommand *)data_ptr;

  if (unlikely(IS_EMPTY((cmd_ptr))))				goto return_error_ufsrvcommand_missing;
  if (unlikely(IS_EMPTY(cmd_ptr->header)))	goto return_error_commandheader_missing;

  return_success:
  goto return_final;

  return_error_ufsrvcommand_missing:
  syslog(LOG_DEBUG, "%s {pid:'%lu'}: ERROR: COULD NOT FIND UFSRV COMMAND IN UNPACKED MESAGEQUEUE", __func__, pthread_self());
  rc = -3;
  goto return_free;

  return_error_commandheader_missing:
  syslog(LOG_DEBUG, "%s {pid:'%lu'}: ERROR: COULD NOT FIND COMMAND HEADER", __func__, pthread_self());
  rc = -8;
  goto return_free;

  return_free:
  return_final:
  return rc;

}

//////////////////
