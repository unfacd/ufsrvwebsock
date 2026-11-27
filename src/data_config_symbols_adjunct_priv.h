/**
 * @file data_config_symbols_adjunct_priv.h
 * @brief ufsrvwebsock adjunct symbol table — WebSocket-server-specific scope.
 *
 * Maps the keys of the "ufsrvwebsock" Lua scope to their expected value types
 * and holds a zero-initialised value slot populated at runtime by
 * ResolveSymbols or the generated ufsrv_config bridge.
 *
 * Default values, validation callbacks, and storage targets live in the
 * ufsrv_config generated field tables and cold catalogue — they are not
 * carried on the symbol-table entry.
 *
 * Copyright (C) 2015-2026 unfacd works
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef UFSRVWEBSOCK_DATA_CONFIG_SYMBOLS_ADJUNCT_PRIV_H
#define UFSRVWEBSOCK_DATA_CONFIG_SYMBOLS_ADJUNCT_PRIV_H

#include <ufsrvcorelib/ufsrv_core/configfile_processor/configfile_processor_symbol_table_type.h>
#include <ufsrvcorelib/ufsrv_core/ufsrv_config/generated/ufsrv_config_field_enums_generated.h>

/*
 *  This file defines the static map that ties the leaf Lua schema
 *  (ufsrvcorelib/etc/adjunct_schema_ufsrvwebsock.lua, scope "ufsrvwebsock")
 *  with its runtime bindings.
 *
 *  IMPORTANT: YOU MUST ENSURE NAMING ALIGNMENT BETWEEN:
 *  1) this symbol table (scope + dotted key),
 *  2) literals in 'ufsrv_config_literals_defs.h' (CONF_FILE_LITERAL_UFSRVWEBSOCK_WEBSOCKET_*),
 *  3) lua schema file 'adjunct_schema_ufsrvwebsock.lua',
 *  4) cold catalogue 'ufsrv_config_generator_fields.c.input',
 *  5) generated ufsrv_config field tables.
 *
 *  Keys are dot-paths within the scope, so "websocket.max_frame_size" maps to
 *  ufsrvwebsock = { websocket = { max_frame_size = ... } }.
 */

static ConfFileSymbolTableEntry g_app_config_symbols[] = {

  // ── WebSocket protocol ──────────────────────────────────────────────────
  { .scope = "ufsrvwebsock", .key = "websocket.max_frame_size",              .type = UFSRV_CONFIG_VALUE_INT, .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "Maximum WebSocket frame payload (bytes)" },
  { .scope = "ufsrvwebsock", .key = "websocket.max_message_size",            .type = UFSRV_CONFIG_VALUE_INT, .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "Maximum reassembled message size (bytes)" },
  { .scope = "ufsrvwebsock", .key = "websocket.heartbeat_interval_secs",     .type = UFSRV_CONFIG_VALUE_INT, .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "Server-to-client ping interval (seconds)" },
  { .scope = "ufsrvwebsock", .key = "websocket.close_timeout_secs",          .type = UFSRV_CONFIG_VALUE_INT, .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "Graceful close wait (seconds)" },
  { .scope = "ufsrvwebsock", .key = "websocket.max_connections_per_session", .type = UFSRV_CONFIG_VALUE_INT, .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "Max concurrent connections per session" },

  // Google Cloud service accounts
  {
    .scope = "ufsrvwebsock", .key = CONF_FILE_LITERAL_CLOUD_SERVICE_ACCOUNTS_GPC_FIREBASE_MESSAGING, .type = UFSRV_CONFIG_VALUE_STR, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "firebase-messaging service-account document (enc:v1 envelope, decrypted at use)"
  },
  {
    .scope = "ufsrvwebsock", .key = CONF_FILE_LITERAL_CLOUD_SERVICE_ACCOUNTS_GPC_INTEGRITY, .type = UFSRV_CONFIG_VALUE_STR, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "integrity-api service-account document (enc:v1 envelope, decrypted at use)"
  },

  /* Sentinel */
  {.scope = NULL, .key = NULL, .type = UFSRV_CONFIG_VALUE_TYPE_UNSPECIFIED, .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = NULL}
};

#endif /* UFSRVWEBSOCK_DATA_CONFIG_SYMBOLS_ADJUNCT_PRIV_H */
