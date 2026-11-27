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

#ifndef UFSRV_MESSAGE_COMMAND_UTILS_H
#define UFSRV_MESSAGE_COMMAND_UTILS_H

#include <stdbool.h>

#include "command_base_context_type.h"
#include "message_command_context_type.h"
#include <ufnetcorelib/ufnet_core/session/session_type.h>
#include <ufnetcorelib/ufnet_msgcore/fence/fence.h>
#include <ufnetcorelib/ufnet_msgcore/UnfacdService.pb-c.h>
#include "command_delivery/message_command_delivery.h"

struct MarshalMessageEnvelopeForUser
{
  UfsrvCommandWire *ufsrv_command_wire;
  Envelope *        envelope;
  MessageCommand *  message_command;
  CommandHeader *   header;
  FenceRecord *     fence_record;
  FenceRecord **    fence_records;
  UserRecord *      user_record;
  UserRecord **     user_records;
  UserRecord *      originator;
};

typedef struct MarshalMessageEnvelopeForUser MarshalMessageEnvelopeForUser;

bool MessageCommandCollectFenceTargetUids(unsigned long fid, unsigned long originator_uid,
                                          CollectionResult *out);

void PrepareMarshalMessageForUserMessage(MarshalMessageEnvelopeForUser *envelope_ptr, Session *sesn_ptr, Fence *f_ptr,
                                         UfsrvEvent *event_ptr, DataMessage *data_msg_ptr_orig,
                                         enum _MessageCommand__CommandTypes command_type,
                                         enum _CommandArgs command_arg);
Envelope__Type GetProtocolType(MessageCommand *msg_ptr, void (^on_ready)(Envelope__Type));
UFSRVResult *MarshalMessageToFence(InstanceContextForSession *ctx_sesn_ptr, WebSocketMessage *wsm_ptr_orig,
                                   DataMessage *data_msg_ptr_received, InstanceContextForFence *ctx_f_ptr,
                                   FenceEvent *fence_event_ptr);
UFSRVResult *MarshalMessageToFenceTargets(InstanceContextForSession *ctx_sesn_ptr, WebSocketMessage *wsm_ptr_orig,
                                          DataMessage *data_msg_ptr_received, unsigned long fid,
                                          FenceEvent *fence_event_ptr, const CollectionDescriptor *targets_ptr,
                                          const CollectionDescriptor *target_uids_ptr,
                                          CollectionDescriptor *delivery_results_ptr);
UFSRVResult *MarshalFenceMessageToUser(InstanceContextForSession *ctx_sesn_ptr,
                                       InstanceHolderForSession * instance_sesn_ptr_target,
                                       WebSocketMessage *         wsm_ptr_orig, unsigned long fid,
                                       Envelope *                 command_envelope_ptr);
UFSRVResult *MarshalCommandToUser(InstanceHolderForSession *instance_sesn_ptr,
                                  InstanceHolderForSession *instance_sesn_ptr_target, WebSocketMessage *wsm_ptr_orig,
                                  Envelope *                command_envelope_ptr, unsigned              req_cmd_idx);
bool IsContactSharing(Session *sesn_ptr, UserContactRecord **contact_records_ptr);

#endif //UFSRV_MESSAGE_COMMAND_UTILS_H
