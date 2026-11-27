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

#ifndef SRC_INCLUDE_COMMAND_CONTROLLERS_H_
#define SRC_INCLUDE_COMMAND_CONTROLLERS_H_

#include <command_base_context_type.h>
#include <ufsrv_core/ufsrvresult_type.h>
#include <ufnetcorelib/ufnet_core/session/session_type.h>

//DO NOT INCLUDE ANY OF xxx_command_controller.h files here

#include <ufnetcorelib/ufnet_msgcore/UnfacdService.pb-c.h>
#include <ufnetcorelib/ufnet_msgcore/WebSocketMessage.pb-c.h>


//todo: this style is to be phased out in favour of one defined below
typedef UFSRVResult * (*CallbackCommandMarshaller)(InstanceContextForSession *ctx_ptr, ClientContextData *ctx_data_ptr, DataMessage *data_msg_ptr_received, WebSocketMessage *wsm_ptr_received, UfsrvEvent *event_ptr);
#define _INVOKE_COMMAND_MARSHALLER(command_marshaller, ctx_ptr, ctx_data_ptr, data_msg_ptr_received,  wsm_ptr_received, event_ptr) return (*command_marshaller)(ctx_ptr, CLIENT_CTX_DATA((ctx_data_ptr)), data_msg_ptr_received, wsm_ptr_received, event_ptr)

typedef UFSRVResult * (*CommandMarshallerCallback)(CommandBaseContext *);
#define INVOKE_COMMAND_MARSHALLER(command_marshaller, command_context) (*command_marshaller)((command_context))

UFSRVResult *CommandCallbackControllerCallCommand(InstanceContextForSession *ctx_ptr, WebSocketMessage *wsm_ptr_orig, DataMessage *data_msg_ptr);
UFSRVResult *CommandCallbackControllerUserCommand(InstanceHolderForSession *instance_sesn_ptr_local_user, WebSocketMessage *wsm_ptr_orig, DataMessage *data_msg_ptr);

UFSRVResult *CommandCallbackControllerReceiptCommand(InstanceContextForSession *ctx_ptr_local_user, WebSocketMessage *wsm_ptr_received, DataMessage *data_msg_ptr);

UFSRVResult *IsUserAllowedToChangeFenceUserPrefProfileSharing(InstanceContextForSession *, FenceUserPreference *sesn_msg_pref_ptr, DataMessage *data_msg_ptr_received, UfsrvEvent *event_ptr, CallbackCommandMarshaller );

#endif /* SRC_INCLUDE_COMMAND_CONTROLLERS_H_ */
