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

#ifndef UFSRV_INTER_BROADCAST_USER_H
#define UFSRV_INTER_BROADCAST_USER_H


#include <ufnetcorelib/ufnet_msgcore/fence/fence_event_type.h>
#include <ufnetcorelib/ufnet_core/session/session_type.h>
#include <ufnetcorelib/ufnet_msgcore/UnfacdService.pb-c.h>
#include <ufnet_msgcore/msgqueue_backend/UfsrvMessageQueue.pb-c.h>

int HandleInterBroadcastForUser(MessageQueueMessage 		*mqm_ptr, UFSRVResult *res_ptr, unsigned long);
UFSRVResult *InterBroadcastUserMessageUserPrefsBoolean(Session *sesn_ptr, ClientContextData *context_ptr, UfsrvEvent *event_ptr, enum _CommandArgs command_arg);
UFSRVResult *InterBroadcastUserMessageUserPrefsInteger(Session *sesn_ptr, ClientContextData *context_ptr, UfsrvEvent *event_ptr, enum _CommandArgs command_arg);
UFSRVResult *InterBroadcastUserNicknameMessage(Session *sesn_ptr, ClientContextData *context_ptr, UfsrvEvent *event_ptr, enum _CommandArgs command_arg);
UFSRVResult *InterBroadcastUserAvatarMessage(Session *sesn_ptr, ClientContextData *context_ptr, UfsrvEvent *event_ptr, enum _CommandArgs command_arg);
UFSRVResult *InterBroadcastUserShareListMessage(Session *sesn_ptr, ClientContextData *context_ptr, UfsrvEvent *event_ptr, enum _CommandArgs command_arg);
UFSRVResult *InterBroadcastUserMessageFenceUserPrefs(Session *sesn_ptr, ClientContextData *context_ptr, UfsrvEvent *event_ptr, enum _CommandArgs command_arg);

#endif //UFSRV_INTER_BROADCAST_USER_H
