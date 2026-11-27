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

#ifndef UFSRV_MESSAGE_COMMAND_CONTEXT_TYPE_H
#define UFSRV_MESSAGE_COMMAND_CONTEXT_TYPE_H

#include "command_base_context_type.h"
#include <ufnetcorelib/ufnet_core/session/session_type.h>
#include <ufnetcorelib/ufnet_msgcore/fence/fence_type.h>
#include <ufnetcorelib/ufnet_core/incoming_message_descriptor_type.h>

typedef struct MessageCommandContext {
    CommandBaseContext command_base_context;
    long fid;
    ParsedMessageDescriptor *msg_descriptor_ptr;
    struct {
        struct {
            InstanceContextForSession *ctx_sesn_ptr;
            bool lock_state;
        } sesn_target;
        InstanceContextForFence *ctx_f_ptr;
    } marshaller_context;
}	MessageCommandContext;

#endif //UFSRV_MESSAGE_COMMAND_CONTEXT_TYPE_H
