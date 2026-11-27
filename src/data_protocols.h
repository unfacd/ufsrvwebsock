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

#ifndef DATA_PROTOCOLS_H_
#define DATA_PROTOCOLS_H_

#include <ufsrv_core/protocol/protocol_type.h>
#include <ufnetcorelib/ufnet_core/session/session_type.h>
#include "protocol_websocket.h"//function definitions for this protocol
#include <ufsrv_core/delegator_sessions/workers/worker_session_io_thread.h>

#include "protocol_index_enum.h" //WEBSOCKET_PROTOCOL_INDEX et al

static Protocol ProtocolsRegistry[] = {
  [WEBSOCKET_PROTOCOL_INDEX] = {
    .protocol_name = "WebSockets", .protocol_id = WEBSOCKET_PROTOCOL_INDEX, .protocol_key = "websocket",
    .protocol_threadx = ThreadWebSockets,
    .protocol_callbacks = {
      .protocol_init_callback = proto_websocket_protocol_init_callback,
      .session_init_callback = proto_websocket_init_session_callback,
      .session_reset_callback = proto_websocket_reset_callback, .handshake_callback = proto_websocket_hanshake_callback,
      .post_handshake_callback = proto_websocket_post_hanshake_callback, .msg_callback = proto_websocket_msg_callback,
      .msg_decode_callback = proto_websocket_decode_msg_callback,
      .msg_encode_callback = proto_websocket_encode_msg_callback,
      .service_timeout_callback = proto_websocket_service_timeout_callback,
      .error_callback = proto_websocket_error_callback,
      .recycler_error_callback = proto_websocket_recycler_error_callback,
      .close_callback = proto_websocket_close_callback,
      .msgqueue_topics_callback = proto_websocket_msgqueue_topics_callback,
      .generate_session_id_callback = proto_websocket_generate_session_id_callback,
      .on_ratelimited_callback = proto_websocket_on_ratelimited_callback,
    },
    .protocol_controls = {
      .is_enabled          = 1, .read_blocked_session = 1, .read_inservice_session = 1, .retain_session_on_error = 1,
      .abort_on_ioerror    = 1, .client_of_recycler   = 1, .cloudmsg_on_ioderror   = 1, .pub_session_transitions = 1,
      .msgqueue_subscriber = 1, .msgqueue_publisher   = 1,
    },
  },
  {0}
};

#endif /* DATA_PROTOCOLS_H_ */
