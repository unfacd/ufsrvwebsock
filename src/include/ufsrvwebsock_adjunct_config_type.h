/**
 * @file ufsrvwebsock_adjunct_config_type.h
 * @brief Adjunct config struct for ufsrvwebsock.
 *
 * Declares the C struct that the adjunct symbol table populates during
 * config resolution.  The application allocates a zero-filled instance,
 * passes it to ConfFileSetAdjunct(), then resolves the adjunct symbol
 * table.  After resolution, fields are accessed via ConfFileAdjunct().
 *
 * Copyright (C) 2015-2026 unfacd works
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef UFSRVWEBSOCK_ADJUNCT_CONFIG_TYPE_H
#define UFSRVWEBSOCK_ADJUNCT_CONFIG_TYPE_H

#include <stddef.h>

/*!
 * WebSocket-server-specific adjunct configuration.
 *
 * Populated by the ufsrvwebsock adjunct symbol table's on_val_available
 * callbacks.  Access via:
 *
 * @code{.c}
 * UfsrvWebsockAdjunctConfig *ac = ConfFileAdjunct();
 * printf("max_frame_size = %d\n", ac->max_frame_size);
 * @endcode
 */
typedef struct UfsrvWebsockAdjunctConfig {
  int  max_frame_size;               ///< Maximum WebSocket frame payload (bytes)
  int  max_message_size;             ///< Maximum reassembled message size (bytes)
  int  heartbeat_interval_secs;      ///< Server→client ping interval (seconds)
  int  close_timeout_secs;           ///< Graceful close wait (seconds)
  int  max_connections_per_session;  ///< Max concurrent connections per session
} UfsrvWebsockAdjunctConfig;

#endif /* UFSRVWEBSOCK_ADJUNCT_CONFIG_TYPE_H */
