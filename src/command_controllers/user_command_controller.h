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

#ifndef UFSRV_USER_COMMAND_CONTROLLER_H
#define UFSRV_USER_COMMAND_CONTROLLER_H

#include "command_controllers/command_controllers.h"
#include <command_base_context_type.h>
#include <ufnetcorelib/ufnet_msgcore/fence/fence_type.h>
#include <ufnetcorelib/ufnet_core/session/session_type.h>

//DO NOT INCLUDE ANY OF xxx_command_controller.h files here

#include <ufnetcorelib/ufnet_msgcore/UnfacdService.pb-c.h>
#include <ufnetcorelib/ufnet_msgcore/WebSocketMessage.pb-c.h>

UFSRVResult *MarshalUserPrefProfile(InstanceContextForSession *ctx_ptr, ClientContextData *ctx_data_ptr, WebSocketMessage *, DataMessage *data_msg_ptr_received, UfsrvEvent *event_ptr);
UFSRVResult *MarshalUserPrefProfileForFence(InstanceContextForSession *, ClientContextData *ctx_ptr, WebSocketMessage *, DataMessage *data_msg_ptr_received, UfsrvEvent *event_ptr);
UFSRVResult *MarshalUserPrefNetstate(InstanceContextForSession *ctx_ptr, ClientContextData *ctx_data_ptr, WebSocketMessage *, DataMessage *data_msg_ptr_received, unsigned long call_flags, UfsrvEvent *fence_event_ptr);
UFSRVResult *MarshalUserPrefGroupRoaming(InstanceContextForSession *, ClientContextData *ctx_ptr, WebSocketMessage *, DataMessage *data_msg_ptr_received, unsigned long call_flags, UfsrvEvent *fence_event_ptr);

UFSRVResult *MarshalFenceUserPrefProfileSharing(InstanceContextForSession *, ClientContextData *ctx_ptr, DataMessage *data_msg_ptr_recieved, WebSocketMessage *, UfsrvEvent *fence_event_ptr);
UFSRVResult *MarshalUserPrefShareList(InstanceContextForSession *ctx_ptr, ClientContextData *ctx_data_ptr, DataMessage *data_msg_ptr_recieved, WebSocketMessage *wsm_ptr_received, UfsrvEvent *event_ptr);
UFSRVResult *MarshalUserPrefForBlockedFence(InstanceContextForSession *ctx_ptr, ClientContextData *ctx_data_ptr, DataMessage *data_msg_ptr_received,  WebSocketMessage *wsm_ptr_received, UfsrvEvent *event_ptr);
UFSRVResult *MarshalUserInvalidated(InstanceContextForSession *ctx_ptr, ClientContextData *ctx_data_ptr);

UFSRVResult *IsUserAllowedToShareProfile(InstanceContextForSession *ctx_ptr, WebSocketMessage *, DataMessage *data_msg_received, UfsrvEvent *event_ptr, unsigned long sesn_call_flags);
UFSRVResult *IsUserAllowedToShareNetstate(InstanceContextForSession *ctx_ptr, WebSocketMessage *, DataMessage *data_msg_received, UfsrvEvent *event_ptr, unsigned long sesn_call_flags);
UFSRVResult *IsUserAllowedToShareReadReceipt(InstanceContextForSession *ctx_ptr, DataMessage *data_msg_received, UfsrvEvent *event_ptr, unsigned long sesn_call_flags);
UFSRVResult *IsUserAllowedToShareBlocked(InstanceContextForSession *ctx_ptr, DataMessage *data_msg_received, WebSocketMessage *wsm_ptr_received, UfsrvEvent *event_ptr, CallbackCommandMarshaller command_marshaller, unsigned long sesn_call_flags);
UFSRVResult *IsUserAllowedToShareBlockedFence(InstanceContextForSession *ctx_ptr, DataMessage *data_msg_received, WebSocketMessage *wsm_ptr_received, UfsrvEvent *event_ptr, CallbackCommandMarshaller command_marshaller, unsigned long sesn_call_flags);
UFSRVResult *IsUserAllowedToShareContacts(InstanceContextForSession *ctx_ptr, DataMessage *data_msg_received, WebSocketMessage *, UfsrvEvent *event_ptr, CallbackCommandMarshaller command_marshaller, unsigned long sesn_call_flags);
UFSRVResult *IsUserAllowedToChangeUnsolicitedContactAction(InstanceContextForSession *ctx_ptr, UserPreference *pref_ptr, DataMessage *data_msg_ptr_received, WebSocketMessage *wsm_ptr_received, UfsrvEvent *event_ptr, CallbackCommandMarshaller command_marshaller, unsigned long sesn_call_flags);
UFSRVResult *IsUserAllowedToChangeUserPrefGroupRoaming(InstanceContextForSession *ctx_ptr, UserPreference *sesn_msg_pref_ptr, WebSocketMessage *, DataMessage *data_msg_ptr_received, UfsrvEvent *event_ptr, unsigned long sesn_call_flags);
UFSRVResult *IsUserAllowedToChangeFenceUserPrefStickyGeoGroup(Session *sesn_ptr, FenceUserPreference *sesn_msg_pref_ptr, DataMessage *data_msg_ptr_received, unsigned long sesn_call_flags);

#endif //UFSRV_USER_COMMAND_CONTROLLER_H
