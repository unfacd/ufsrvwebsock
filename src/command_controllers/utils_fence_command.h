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

#ifndef UFSRV_UTILS_FENCE_COMMAND_H
#define UFSRV_UTILS_FENCE_COMMAND_H

#include <uflib/main_types.h>
#include <uflib/recycler/recycler_type.h>
#include <ufsrv_core/ufsrvresult_type.h>
#include <ufnetcorelib/ufnet_msgcore/fence/fence_enums_type.h>
#include <ufnetcorelib/ufnet_msgcore/UnfacdService.pb-c.h>//proto
#include <ufnetcorelib/ufnet_msgcore/WebSocketMessage.pb-c.h>
#include <ufnetcorelib/ufnet_core/session/session_type.h>
#include <ufnetcorelib/ufnet_msgcore/fence/fence_context_descriptor_type.h>
#include <ufnetcorelib/ufnet_msgcore/fence/typepool_context_data_fence_type.h>
#include <ufnetcorelib/ufnet_msgcore/fence/fence_state_descriptor_type.h>
#include <ufnetcorelib/ufnet_msgcore/fence/fence_event_type.h>

#include <ufnetcorelib/ufnet_msgcore/zkgroup_utils/zkgroup_masterkey_type.h>

UFSRVResult *HandleJoinFence(InstanceContextForSession *, InstanceHolderForFenceStateDescriptor *, WebSocketMessage *, DataMessage *data_msg_ptr, EnumFenceJoinType join_type, UFSRVResult *res_ptr);
UFSRVResult *IsUserAllowedToChangeFenceMessageExpiry(Session *sesn_ptr, FenceStateDescriptor *fence_state_ptr, time_t msg_expiry_in_seconds, unsigned long fence_call_flags, FenceEvent *fence_event_ptr_out);
UFSRVResult *IsUserAllowedToChangeFenceName(Session *sesn_ptr, FenceStateDescriptor *fence_state_ptr, const char *fname_new, unsigned long fence_call_flags, FenceEvent *fence_event_ptr_out);
UFSRVResult *IsUserAllowedToChangeFenceDescription(Session *sesn_ptr, FenceStateDescriptor *fence_state_ptr, const char *description_new, unsigned long fence_call_flags, FenceEvent *fence_event_ptr_out);
UFSRVResult *IsUserAllowedToJoinGeoFence(InstanceHolderForSession *, LocationDescription *);
UFSRVResult *IsUserAllowedGeoFenceInvite(InstanceHolderForSession *instance_sesn_ptr, LocationDescription *location_description_ptr, Session *sesn_ptr_inviter, unsigned long fence_call_flags);
UFSRVResult *IsUserAllowedToChangeFenceMaxMembers(Session *sesn_ptr, FenceStateDescriptor *fence_state_ptr, int32_t maxmembers, unsigned long fence_call_flags, FenceEvent *fence_event_ptr_out);
UFSRVResult *IsUserAllowedToChangeFenceDeliveryMode(Session *sesn_ptr, FenceStateDescriptor *fence_state_ptr, int delivery_mode, unsigned long fence_call_flags, FenceEvent *fence_event_ptr_out);

UFSRVResult *IsUserAllowedToJoinFenceById(InstanceHolderForSession *, const unsigned long, const char *linkjoin_nonce, unsigned long, bool *fence_lock_state);
UFSRVResult *_ProcessUserIsAllowedToJoinFence(InstanceHolderForSession *instance_sesn_ptr, InstanceHolderForFence *instance_f_ptr, const char *linkjoin_nonce, unsigned long call_flags_fence);
UFSRVResult *IsUserAllowedToJoinFenceByCanonicalName(InstanceHolderForSession *, const char *, unsigned long);
UFSRVResult *IsUserAllowedToMakeUserFence(InstanceHolderForSession *instance_sesn_ptr, const char *fence_banner, const char *baseloc_prefix, FenceContextDescriptor *, bool *fence_lock_already_owned, unsigned long call_flags);
UFSRVResult *IsUserAllowedToChangeFence(Session *sesn_ptr, unsigned long fid, const char *cname, bool *fence_lock_state, unsigned long fence_call_flags);
UFSRVResult *IsUserAllowedToRejectFence(Session *sesn_ptr, unsigned long fid, const char *cname, bool *fence_lock_state, unsigned long fence_call_flags);
UFSRVResult *IsUserAllowedToRemoveInvitedUser(Session *sesn_ptr, unsigned long fid, InstanceContextForFence *, ContextData *ctx_data, unsigned long fence_call_flags);

FenceEvent *UpdateBackendFenceData(Session *sesn_ptr_target, Fence *f_ptr, void *user_data, unsigned event_type, FenceEvent *);
FenceEvent *UpdateBackendFenceInvitedData(Session *sesn_ptr_inviter, Session *sesn_ptr_invited, FenceStateDescriptor *fstate_ptr, unsigned event_type, FenceEvent *fe_ptr_out);
void UpdateFenceAssignments(Session *sesn_ptr, Fence *f_ptr_processed, ClientContextData *context_ptr);
UFSRVResult *UpdateFenceAvatarAssignment(Session *sesn_ptr, Fence *f_ptr, AttachmentRecord *record_ptr, unsigned long fence_call_flags);
bool UpdateFenceNameAssignment(Session *, InstanceHolderForFence *instance_f_ptr, const char *fname_new, char *cname_new, bool flag_build_cname, unsigned long);
bool UpdateFenceDescriptionAssignment(Session *sesn_ptr, InstanceHolderForFence *instance_f_ptr, const char *description_new, unsigned long fence_call_flags);
UFSRVResult *UpdateFenceKeyAssignment(Fence *f_ptr, ZKGroupMasterKey *group_key, unsigned long fence_call_flags);
UFSRVResult *UpdateFenceTypeAssignment(Session *sesn_ptr, Fence *f_ptr, FenceRecord__FenceType fence_type, unsigned long fence_call_flags);
UFSRVResult *UpdateFenceMaxUsersAssignment(Session *sesn_ptr, Fence *f_ptr, int maxusers, unsigned long fence_call_flags);
UFSRVResult *UpdateFencePrivacyModeAssignment(Session *sesn_ptr, Fence *f_ptr, FenceRecord__PrivacyMode privacy_mode, unsigned long fence_call_flags);
UFSRVResult *UpdateFenceDeliveryModeAssignment(Session *sesn_ptr, Fence *f_ptr, FenceRecord__DeliveryMode delivery_mode, unsigned long fence_call_flags);
UFSRVResult *IsUserAllowedToChangeFenceAvatar(Session *sesn_ptr, FenceStateDescriptor *fence_state_ptr, DataMessage *data_msg_ptr, unsigned long fence_call_flags, FenceEvent *fence_event_ptr_out);
UFSRVResult *UpdateFenceJoinModeAssignment(Session *sesn_ptr, Fence *f_ptr, FenceRecord__JoinMode join_mode, unsigned long fence_call_flags);

UFSRVResult *RemoveUserFromFencePermissions(InstanceHolderForSession *instance_sesn_ptr, Fence *f_ptr, FencePermission *ht_ptr_permission, unsigned long fence_call_flags, FenceEvent *fence_event_ptr_out);
UFSRVResult *AddUserToFencePermissions(InstanceHolderForSession *instance_sesn_ptr, Fence *f_ptr, FencePermission *ht_ptr_permission, unsigned long fence_call_flags, FenceEvent *fence_event_ptr_out);
InstanceHolderForFenceStateDescriptor *CreateUserFenceAndLinkToUser(InstanceHolderForSession *instance_sesn_ptr, const char *fence_banner, char *userfence_canonical_name_in, FenceContextDescriptor *, unsigned long call_flags);
UFSRVResult *AddMemberToInvitedFenceList(InstanceHolderForSession *instance_sesn_ptr_invited, InstanceHolderForFence *instance_f_ptr, Session *sesn_ptr_inviter, unsigned long);

CollectionDescriptor *AddToFenceInvitedListFromProtoRecord(InstanceHolderForSession *instance_sesn_ptr, InstanceHolderForFence *instance_f_ptr, UserRecord **user_records, size_t invited_members_sz, CollectionDescriptorPair *, bool flag_exclude_self);

unsigned RemoveUserFromAllFences(InstanceHolderForSession *, unsigned long);
unsigned long RemoveUserFromMembersListForFence(InstanceHolderForSession *instance_sesn_ptr_target, Fence *f_ptr_in, unsigned long call_flags);
int RemoveUserFromInvitedList(InstanceHolderForSession *instance_sesn_ptr, FenceStateDescriptor *fence_state_invited, FenceEvent *fe_ptr_out, unsigned long fence_call_flags);
#endif //UFSRV_UTILS_FENCE_COMMAND_H
