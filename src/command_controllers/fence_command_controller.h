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

#ifndef UFSRV_FENCE_COMMAND_CONTROLLER_H
#define UFSRV_FENCE_COMMAND_CONTROLLER_H

#include "command_controllers/command_controllers.h"
#include <ufnetcorelib/ufnet_msgcore/UnfacdService.pb-c.h>
#include <ufnetcorelib/ufnet_msgcore/fence/fence_type.h>
#include <ufnetcorelib/ufnet_msgcore/fence/fence_state_descriptor_type.h>
#include <ufnetcorelib/ufnet_msgcore/WebSocketMessage.pb-c.h>

typedef void (*CallbackRestoreFenceValue)(FenceStateDescriptor *fence_state_ptr, FenceRecord *fence_record_ptr);

UFSRVResult *CommandCallbackControllerFenceCommand(InstanceHolderForSession *instance_sesn_ptr, WebSocketMessage *wsm_ptr_orig, DataMessage *data_msg_ptr_received);
UFSRVResult *HandleFenceCommandError(InstanceContextForSession *ctx_ptr, FenceStateDescriptor *fence_state_ptr, WebSocketMessage *wsm_ptr_received, DataMessage *data_msg_ptr, int rescode, FenceCommand__CommandTypes command_type, CallbackRestoreFenceValue restore_callback);
/**
 * @brief Marshal one already-prepared fence command envelope to a single session, immediately.
 *
 * This is the synchronous path: a @c NULL @p ctx_ptr_target addresses the session that issued the
 * command, which is how the originator's own acknowledgement (and every error response) is
 * delivered. Durable fan-out to the rest of the fence goes through
 * @ref FenceCommandStageForFenceMembers() and is replayed by the fence command drain.
 */
UFSRVResult *WireMarshalFenceCommandToUser(InstanceContextForSession *ctx_ptr, InstanceContextForSession *ctx_ptr_target, Fence *f_ptr, WebSocketMessage *,Envelope *command_envelope_ptr, unsigned req_cmd_idx);
UFSRVResult *MarshalFenceJoinToUser(InstanceContextForSession *, FenceStateDescriptor *fence_state_ptr, WebSocketMessage *wsm_ptr_orig, DataMessage *data_msg_ptr);
UFSRVResult *MarshalFenceJoinInvitedToUser(InstanceContextForSession *, FenceStateDescriptor *fence_state_ptr, WebSocketMessage *wsm_ptr_orig, DataMessage *data_msg_ptr);
UFSRVResult *MarshalFenceStateSync(InstanceContextForSession *ctx_ptr, FenceStateDescriptor *fstate_ptr, WebSocketMessage *, DataMessage *data_msg_ptr, unsigned long call_flags);
UFSRVResult *MarshalGeoFenceJoinToUser(InstanceContextForSession *, InstanceContextForSession *, FenceStateDescriptor *fence_state_ptr, WebSocketMessage *wsm_ptr, unsigned call_flags);
UFSRVResult *MarshalFenceStateSyncForLeave(InstanceContextForSession *, InstanceContextForSession *ctx_ptr_newly_left, InstanceContextForFence *, DataMessage *data_msg_ptr, unsigned call_flags);
UFSRVResult *MarshalFenceStateSyncForJoin(InstanceContextForSession *, Session *sesn_ptr_newly_joined, InstanceHolderForFence *, unsigned call_flags);
UFSRVResult *MarshalFenceInvitation(InstanceContextForSession *, InstanceHolderForFence *, WebSocketMessage *wsm_ptr_orig, DataMessage *data_msg_ptr, CollectionDescriptor *invited_eids_collection_ptr, CollectionDescriptor *unchanged_collection_ptr, unsigned call_flags);
UFSRVResult *MarshalFenceInvitationRejected(InstanceContextForSession *ctx_ptr, FenceStateDescriptor *fstate_ptr, InstanceContextForFence *fence_context,  UfsrvUid *uid_invited_by, FenceEvent *event_ptr, WebSocketMessage *wsm_ptr_received, DataMessage *data_msg_ptr, unsigned call_flags);
UFSRVResult *MarshalFenceInvitationDeleted(InstanceContextForSession *ctx_ptr, InstanceContextForSession *instance_sesn_ctx_uninvited, FenceStateDescriptor *fstate_ptr, InstanceContextForFence *fence_context, UfsrvUid *uid_invited_by, FenceEvent *event_ptr, WebSocketMessage *wsm_ptr_received, DataMessage *data_msg_ptr, unsigned call_flags);
UFSRVResult *MarshalFenceInvitationBlocked(InstanceContextForSession *ctx_ptr, FenceStateDescriptor *fstate_ptr, InstanceContextForFence *fence_context, UfsrvUid *uid_invited_by, FenceEvent *event_ptr, WebSocketMessage *wsm_ptr_received, DataMessage *data_msg_ptr, unsigned call_flags);
UFSRVResult *MarshalFenceUnInvitedToUser(InstanceContextForSession *ctx_ptr, InstanceContextForSession *ctx_ptr_uninvited, Fence *f_ptr, FenceEvent *fence_event_ptr, unsigned call_flags);
size_t NetworkRemoveUsersFromInviteList(InstanceContextForSession *ctx_ptr_carrier, InstanceHolderForFence *);

#endif //UFSRV_FENCE_COMMAND_CONTROLLER_H
