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


#ifndef UFSRV_COMMAND_BROADCAST_STATE_H
#define UFSRV_COMMAND_BROADCAST_STATE_H

#include <ufnetcorelib/ufnet_msgcore/fence/fence_type.h>
#include <ufnetcorelib/ufnet_msgcore/fence/fence_event_type.h>
#include <ufnetcorelib/ufnet_core/session/session_type.h>
#include <ufsrv_core/protocol/protocol_type.h>

#include <ufnetcorelib/ufnet_msgcore/UnfacdService.pb-c.h>
#include <ufnet_msgcore/msgqueue_backend/UfsrvMessageQueue.pb-c.h>

UFSRVResult *InterBroadcastState(Session *sesn_ptr, ClientContextData *context_ptr, FenceEvent *event_ptr, enum _CommandArgs command_arg);
int HandleInterBroadcastForState(MessageQueueMessage 		*mqm_ptr, UFSRVResult *res_ptr, unsigned long call_flags);
int HandleIntraBroadcastForState(MessageQueueMessage *, UFSRVResult *res_ptr, unsigned long call_flags);
int VerifyStateCommandFromUser(WireProtocolData *data_ptr);

#endif //UFSRV_COMMAND_BROADCAST_STATE_H
