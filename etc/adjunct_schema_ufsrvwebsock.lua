-- adjunct_schema_ufsrvwebsock.lua — ufsrvwebsock adjunct namespace schema
--
-- Application-owned adjunct schema (the framework owns core_schema_ufsrv.lua
-- and core_schema_validator.lua).  Defines the `ufsrvwebsock` scope; the
-- validator merges this with the core schema before validating the
-- ufsrvwebsock config file.
--
-- IMPORTANT: when you make changes to attribute names, make sure they match
-- literals defined in:
-- include/ufsrvcorelib/ufsrv_core/configfile_processor/ufsrv_config_literals_defs.h
-- and the cold catalogue
-- dev/scripts/ufsrv_config_generator/ufsrvwebsock/ufsrv_config_generator_fields.c.input
--
-- Copyright (C) 2015-2026 unfacd works
-- SPDX-License-Identifier: AGPL-3.0-or-later

local schema = {

    -- ══════════════════════════════════════════════════════════════════
    -- ufsrvwebsock scope — WebSocket-server-specific
    -- ══════════════════════════════════════════════════════════════════

    ufsrvwebsock = {
        -- ── WebSocket protocol ───────────────────────────────────────
        websocket = {
               max_frame_size              = { type = "integer", required = false, default = 65536,   min = 256 },
               max_message_size            = { type = "integer", required = false, default = 1048576, min = 256 },
               heartbeat_interval_secs     = { type = "integer", required = false, default = 30,      min = 5 },
               close_timeout_secs          = { type = "integer", required = false, default = 10,      min = 1 },
               max_connections_per_session = { type = "integer", required = false, default = 1,       min = 1 },
       },

        -- ── Google Cloud service accounts ────────────────────────────
        -- Each value is an enc:v1 envelope of the service-account document,
        -- decrypted at the point of use with the instance secret key; the
        -- document supplies both the signing key and the key id it is filed
        -- under at Google.
        cloud_service_accounts = {
            gpc_firebase_messaging = { type = "string", required = false, default = "" },
            gpc_integrity          = { type = "string", required = false, default = "" }
        }
    }
}

return schema
