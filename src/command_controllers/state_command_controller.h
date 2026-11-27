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


#ifndef UFSRV_STATE_COMMAND_CONTROLLER_H
#define UFSRV_STATE_COMMAND_CONTROLLER_H

#include <stddef.h>
#include <ufsrv_core/ufsrvresult_type.h>
#include <uflib/recycler/instance_type.h>
#include <ufnetcorelib/ufnet_core/session/session_type.h>
#include <command_base_context_type.h>

#include <ufnetcorelib/ufnet_msgcore/WebSocketMessage.pb-c.h>
#include <ufnetcorelib/ufnet_msgcore/UnfacdService.pb-c.h>

typedef struct StateCommandContext {
  CommandBaseContext command_base_context;
  long fid;
  struct {
    const unsigned long *uids;    ///< target snapshot the marshaller stages, set by the permission gate
    size_t               uids_sz; ///< number of targets in @c uids
  } recipients;
  struct {
    struct {
      InstanceContextForSession *ctx_sesn_ptr;
      bool lock_state;
    } sesn_target;
  } marshaller_context;
}	StateCommandContext;

#define CMDCTX_STATE_FID(x) ((x)->fid)
#define CMDCTX_STATE_LOCK_STATE(x) ((x)->marshaller_context.sesn_target.lock_state)
#define CMDCTX_STATE_TARGET_SESN_CTX(x) ((x)->marshaller_context.sesn_target.ctx_sesn_ptr)
#define CMDCTX_STATE_RECIPIENT_UIDS(x) ((x)->recipients.uids)
#define CMDCTX_STATE_RECIPIENT_COUNT(x) ((x)->recipients.uids_sz)

UFSRVResult *
CommandCallbackControllerStateCommand (InstanceContextForSession *ctx_ptr_local_user, WebSocketMessage *wsm_ptr_orig, DataMessage *data_msg_ptr);

#endif //UFSRV_STATE_COMMAND_CONTROLLER_H
