/**
 * @file data_config_symbols_core_priv.h
 * @brief Core symbol table for the ufsrv namespace — ships with ufsrvcorelib.
 *
 * Every ufsrv-class server inherits this table.  Each entry maps a Lua config
 * key in the "ufsrv" scope to its expected value type and holds a zero-initialised
 * value slot that is populated at runtime by ResolveSymbols or the generated
 * ufsrv_config bridge.
 *
 * Default values, validation callbacks, and storage targets have been moved
 * to the ufsrv_config generated field tables and cold catalogue — they are
 * no longer carried on the symbol-table entry.
 *
 * Copyright (C) 2015-2026 unfacd works
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef UFSRV_CORE_CONFIGFILE_PROCESSOR_DATA_CONFIG_SYMBOLS_CORE_PRIV_H
#define UFSRV_CORE_CONFIGFILE_PROCESSOR_DATA_CONFIG_SYMBOLS_CORE_PRIV_H

#include "ufsrv_core/configfile_processor/configfile_processor_symbol_table_type.h"
#include "ufsrv_core/configfile_processor/ufsrv_config_literals_defs.h"
#include "ufsrv_core/ufsrv_config/generated/ufsrv_config_field_enums_generated.h"

/*
 * This file defines a static map that ties lua schema file with the c-language processing and runtime bindings, enabling
 * values contained in those files to be used at runtime using unified accessors. For example: CONF_FILE_LITERAL_SERVER_INSTANCE_ID -> 'int UfsrvConfigServerInstanceId()'
 * In order for this this scheme to work an number of conventions + definition files must be satisfied, including alignment requirements outlined below:
 *
 *  IMPORTANT: YOU MUST ENSURE NAMING ALIGNMENT BETWEEN:
 *  1) static array entries in this very file 'data_config_symbols_core_priv.h',
 *  2) literals in this file 'ufsrv_config_literals_defs.h',
 *  3) lua schema file 'core_schema_ufsrv.lua',
 *  4) input file 'ufsrv_config_generator_fields.c.input' (generated field tables),
 *  5) generated ufsrv_config field tables and cold catalogue (where defaults, validators, and storage wiring now live).
 *
 */

static ConfFileSymbolTableEntry g_ufsrv_config_symbols[] = {

  // Server identity
  {
    .scope       = "ufsrv", .key = CONF_FILE_LITERAL_SERVER_INSTANCE_ID, .type = UFSRV_CONFIG_VALUE_INT, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "server instance id (CLI -i)"
  },
  {
    .scope       = "ufsrv", .key = CONF_FILE_LITERAL_SERVER_RUN_MODE, .type = UFSRV_CONFIG_VALUE_STR, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "run mode: normal|shadow"
  },
  {
    .scope       = "ufsrv", .key = CONF_FILE_LITERAL_SERVER_CPU_AFFINITY, .type = UFSRV_CONFIG_VALUE_STR, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "CPU affinity policy"
  },
  {
    .scope       = "ufsrv", .key = CONF_FILE_LITERAL_UFSRV_GEOGROUP, .type = UFSRV_CONFIG_VALUE_INT, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "geographic routing group"
  },

  // Network listeners
  {
    .scope = "ufsrv", .key = CONF_FILE_LITERAL_LISTENERS, .type = UFSRV_CONFIG_VALUE_COLLECTION, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "configured TCP listeners"
  },
  {
    .scope       = "ufsrv", .key = CONF_FILE_LITERAL_COMMAND_CONSOLE_PORT, .type = UFSRV_CONFIG_VALUE_INT, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "TCP port for command console"
  },
  {
    .scope = "ufsrv", .key     = CONF_FILE_LITERAL_COMMAND_CONSOLE_BIND_ADDRESS, .type = UFSRV_CONFIG_VALUE_STR,
    .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT, .description                = "console bind address"
  },

  // Thread pools
  {
    .scope = "ufsrv", .key     = CONF_FILE_LITERAL_SESSION_WORKERS_THREAD_POOL, .type = UFSRV_CONFIG_VALUE_INT,
    .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT, .description               = "session worker count"
  },
  {
    .scope = "ufsrv", .key = CONF_FILE_LITERAL_UFSRV_WORKERS_THREAD_POOL, .type = UFSRV_CONFIG_VALUE_INT, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "ufsrv worker count"
  },
  {
    .scope = "ufsrv", .key     = CONF_FILE_LITERAL_LISTENER_WORKERS_THREAD_POOL, .type = UFSRV_CONFIG_VALUE_INT,
    .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT, .description                = "listener worker count"
  },

  // SSL COMMAND CONSOLE
  {
    .scope = "ufsrv", .key     = CONF_FILE_LITERAL_SSL_COMMAND_CONSOLE_IS_ENABLED, .type = UFSRV_CONFIG_VALUE_BOOL,
    .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "SSL Command Console is enabled"
  },
  {
    .scope = "ufsrv", .key     = CONF_FILE_LITERAL_SSL_COMMAND_CONSOLE_CERTIFICATE_FILE, .type = UFSRV_CONFIG_VALUE_STR,
    .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "SSL Command Console cert file"
  },
  {
    .scope = "ufsrv", .key     = CONF_FILE_LITERAL_SSL_COMMAND_CONSOLE_KEY_FILE, .type = UFSRV_CONFIG_VALUE_STR,
    .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT, .description                = "SSL Command Console key file"
  },
  {
    .scope = "ufsrv", .key     = CONF_FILE_LITERAL_SSL_COMMAND_CONSOLE_LOCATION, .type = UFSRV_CONFIG_VALUE_STR,
    .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "SSL cert Command Console directory"
  },
  {
    .scope = "ufsrv", .key     = CONF_FILE_LITERAL_SSL_COMMAND_CONSOLE_ALLOW_LIST_GOD, .type = UFSRV_CONFIG_VALUE_ARRAY,
    .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "SSL fingerprints for super admin access"
  },
  {
    .scope = "ufsrv", .key = CONF_FILE_LITERAL_SSL_COMMAND_CONSOLE_ALLOW_LIST_BROWSER, .type = UFSRV_CONFIG_VALUE_ARRAY,
    .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "SSL fingerprints for read access"
  },
  {
    .scope = "ufsrv", .key     = CONF_FILE_LITERAL_SSL_COMMAND_CONSOLE_DENY_LIST, .type = UFSRV_CONFIG_VALUE_ARRAY,
    .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "SSL fingerprints for deined access"
  },

  // SSL COMMAND CONSOLE CLIENT
  {
    .scope = "ufsrv", .key = CONF_FILE_LITERAL_SSL_COMMAND_CONSOLE_CLIENT_IS_ENABLED, .type = UFSRV_CONFIG_VALUE_BOOL,
    .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "SSL Command Console client is enabled"
  },
  {
    .scope       = "ufsrv", .key = CONF_FILE_LITERAL_SSL_COMMAND_CONSOLE_IS_CLIENT_CERTIFICATE_REQUIRED,
    .type        = UFSRV_CONFIG_VALUE_BOOL, .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT,
    .description = "SSL Command Console client certificate required"
  },
  {
    .scope       = "ufsrv", .key                  = CONF_FILE_LITERAL_SSL_COMMAND_CONSOLE_CLIENT_CERTIFICATE_FILE,
    .type        = UFSRV_CONFIG_VALUE_STR, .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT,
    .description = "SSL cert Command Console client  cert file"
  },
  {
    .scope = "ufsrv", .key     = CONF_FILE_LITERAL_SSL_COMMAND_CONSOLE_CLIENT_KEY_FILE, .type = UFSRV_CONFIG_VALUE_STR,
    .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "SSL key  Command Console client key file"
  },
  {
    .scope = "ufsrv", .key     = CONF_FILE_LITERAL_SSL_COMMAND_CONSOLE_CLIENT_LOCATION, .type = UFSRV_CONFIG_VALUE_STR,
    .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "SSL cert Command Console client  directory"
  },

  // StatsD instrumentation
  {
    .scope = "ufsrv", .key = CONF_FILE_LITERAL_STATS_BACKEND_ADDRESS, .type = UFSRV_CONFIG_VALUE_STR, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "StatsD server address"
  },
  {
    .scope       = "ufsrv", .key = CONF_FILE_LITERAL_STATS_BACKEND_PORT, .type = UFSRV_CONFIG_VALUE_INT, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "StatsD server port"
  },

  // User session timeouts
  {
    .scope = "ufsrv", .key = CONF_FILE_LITERAL_USER_TIMEOUTS_UNAUTHENTICATED_TIMEOUT, .type = UFSRV_CONFIG_VALUE_INT,
    .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "unauthenticated timeout (s)"
  },
  {
    .scope = "ufsrv", .key     = CONF_FILE_LITERAL_USER_TIMEOUTS_CONNECTED_TIMEOUT, .type = UFSRV_CONFIG_VALUE_INT,
    .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT, .description                   = "connected timeout (s)"
  },
  {
    .scope = "ufsrv", .key     = CONF_FILE_LITERAL_USER_TIMEOUTS_SUSPENDED_TIMEOUT, .type = UFSRV_CONFIG_VALUE_INT,
    .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT, .description                   = "suspended timeout (s)"
  },
  //todo this should be in ufsrvwebsock
  {
    .scope = "ufsrv", .key     = CONF_FILE_LITERAL_USER_TIMEOUTS_LOCATIONLESS_TIMEOUT, .type = UFSRV_CONFIG_VALUE_INT,
    .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "locationless timeout (s)"
  },

  // I/O buffer sizes
  {
    .scope = "ufsrv", .key     = CONF_FILE_LITERAL_BUFFER_SIZES_INCOMING_BUFFER_SIZE, .type = UFSRV_CONFIG_VALUE_INT,
    .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "incoming I/O buffer (bytes)"
  },
  {
    .scope = "ufsrv", .key     = CONF_FILE_LITERAL_BUFFER_SIZES_INCOMING_BUFFER_MAXSIZE, .type = UFSRV_CONFIG_VALUE_INT,
    .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "max incoming I/O buffer (bytes)"
  },
  {
    .scope = "ufsrv", .key     = CONF_FILE_LITERAL_BUFFER_SIZES_OUTGOING_BUFFER_SIZE, .type = UFSRV_CONFIG_VALUE_INT,
    .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "outgoing I/O buffer (bytes)"
  },
  {
    .scope = "ufsrv", .key     = CONF_FILE_LITERAL_BUFFER_SIZES_HOLDING_BUFFER_SIZE, .type = UFSRV_CONFIG_VALUE_INT,
    .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT, .description                    = "holding buffer (bytes)"
  },

  // Session memory-pool specs
  {
    .scope = "ufsrv", .key = CONF_FILE_LITERAL_MEMORY_SPECS_SESSIONS_PER_ALLOC_GROUP, .type = UFSRV_CONFIG_VALUE_INT,
    .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "sessions per allocation group"
  },
  {
    .scope = "ufsrv", .key     = CONF_FILE_LITERAL_MEMORY_SPECS_ALLOCATION_GROUPS, .type = UFSRV_CONFIG_VALUE_INT,
    .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT, .description                  = "max allocation groups"
  },
  {
    .scope = "ufsrv", .key     = CONF_FILE_LITERAL_MEMORY_SPECS_ALLOCATION_THRESHOLD, .type = UFSRV_CONFIG_VALUE_STR,
    .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "allocation threshold trigger (% string)"
  },

  // Runtime privilege drop
  {
    .scope = "ufsrv", .key = CONF_FILE_LITERAL_SYS_RUNTIME_RUN_AS_USER, .type = UFSRV_CONFIG_VALUE_STR, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "user to drop privileges to"
  },
  {
    .scope       = "ufsrv", .key = CONF_FILE_LITERAL_SYS_RUNTIME_CHROOT, .type = UFSRV_CONFIG_VALUE_STR, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "chroot path"
  },

  // File loader
  {
    .scope       = "ufsrv", .key = CONF_FILE_LITERAL_FILELOADER_ENABLED, .type = UFSRV_CONFIG_VALUE_BOOL, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "enable concurrent file loader"
  },
  {
    .scope = "ufsrv", .key = CONF_FILE_LITERAL_FILELOADER_REGISTRY_SIZE, .type = UFSRV_CONFIG_VALUE_INT, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "max files tracked (1..1048576)"
  },
  {
    .scope = "ufsrv", .key = CONF_FILE_LITERAL_FILELOADER_REGISTRY_HOT_LOADED_SIZE, .type = UFSRV_CONFIG_VALUE_INT,
    .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT,
    .description = "max files mmap'd simultaneously (1..registry_size)"
  },
  {
    .scope = "ufsrv", .key = CONF_FILE_LITERAL_FILELOADER_BLACK_LISTED_FILES_REGISTRY, .type = UFSRV_CONFIG_VALUE_ARRAY,
    .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "paths never to serve"
  },
  {
    .scope = "ufsrv", .key = CONF_FILE_LITERAL_FILELOADER_FILES_REGISTRY, .type = UFSRV_CONFIG_VALUE_ARRAY,
    .value = {0}, .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "paths to preload into the loader registry"
  },

  // Logger
  {
    .scope       = "ufsrv", .key = CONF_FILE_LITERAL_LOGGER_DRIVER, .type = UFSRV_CONFIG_VALUE_STR, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "logger destination: syslog|stderr|stdout|file"
  },
  {
    .scope       = "ufsrv", .key = CONF_FILE_LITERAL_LOGGER_FORMAT, .type = UFSRV_CONFIG_VALUE_STR, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "log pattern (zlog format)"
  },
  {
    .scope       = "ufsrv", .key = CONF_FILE_LITERAL_LOGGER_PREFIX, .type = UFSRV_CONFIG_VALUE_STR, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "category carried by log records"
  },
  {
    .scope       = "ufsrv", .key = CONF_FILE_LITERAL_RATELIMIT_IS_ENABLED, .type = UFSRV_CONFIG_VALUE_BOOL, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "enable request rate limiting"
  },
  {
    .scope       = "ufsrv", .key = CONF_FILE_LITERAL_RATELIMIT_IS_BYPASS_PRIVATE_IP, .type = UFSRV_CONFIG_VALUE_BOOL, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "exempt private-range peers from rate limiting"
  },

  // Outbound e-mail provider (Mailgun-compatible HTTP API)
  {
    .scope       = "ufsrv", .key = CONF_FILE_LITERAL_EMAIL_API_KEY, .type = UFSRV_CONFIG_VALUE_STR, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "provider API key (enc:v1 envelope, decrypted at use)"
  },
  {
    .scope       = "ufsrv", .key = CONF_FILE_LITERAL_EMAIL_API_URL, .type = UFSRV_CONFIG_VALUE_STR, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "provider API endpoint URL"
  },

  // GeoIP backend
  {
    .scope       = "ufnet", .key = "geoip_service.address", .type = UFSRV_CONFIG_VALUE_STR, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "GeoIP server address"
  },
  {
    .scope       = "ufnet", .key = "geoip_service.port", .type = UFSRV_CONFIG_VALUE_INT, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "GeoIP server port"
  },

  // Message queue
  {
    .scope       = "ufnet", .key = "msgqueue.is_enabled", .type = UFSRV_CONFIG_VALUE_BOOL, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "enable inter/intra-instance messaging"
  },
  {
    .scope       = "ufnet", .key = "msgqueue.address", .type = UFSRV_CONFIG_VALUE_STR, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "message queue address"
  },
  {
    .scope       = "ufnet", .key = "msgqueue.port", .type = UFSRV_CONFIG_VALUE_INT, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "message queue port"
  },

  // Database backend (MariaDB)
  {
    .scope       = "ufnet", .key = "db_backend.port", .type = UFSRV_CONFIG_VALUE_INT, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "MariaDB port"
  },
  {
    .scope       = "ufnet", .key = "db_backend.address", .type = UFSRV_CONFIG_VALUE_STR, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "MariaDB server address"
  },
  {
    .scope       = "ufnet", .key = "db_backend.username", .type = UFSRV_CONFIG_VALUE_STR, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "MariaDB username"
  },
  {
    .scope       = "ufnet", .key = "db_backend.password", .type = UFSRV_CONFIG_VALUE_STR, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "MariaDB password (sensitive)"
  },

  // Persistence backend (Redis)
  {
    .scope       = "ufnet", .key = "persistence_backend.address", .type = UFSRV_CONFIG_VALUE_STR, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "Redis persistence address"
  },
  {
    .scope       = "ufnet", .key = "persistence_backend.port", .type = UFSRV_CONFIG_VALUE_INT, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "Redis persistence port"
  },

  // User-message cache backend (Redis)
  {
    .scope       = "ufnet", .key = "cache_backend_usrmsg.address", .type = UFSRV_CONFIG_VALUE_STR, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "usrmsg cache address"
  },
  {
    .scope       = "ufnet", .key = "cache_backend_usrmsg.port", .type = UFSRV_CONFIG_VALUE_INT, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "usrmsg cache port"
  },

  // Fence cache backend (Redis)
  {
    .scope       = "ufnet", .key = "cache_backend_fence.address", .type = UFSRV_CONFIG_VALUE_STR, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "fence cache address"
  },
  {
    .scope       = "ufnet", .key = "cache_backend_fence.port", .type = UFSRV_CONFIG_VALUE_INT, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = "fence cache port"
  },
  /* Sentinel */
  {
    .scope       = NULL, .key = NULL, .type = UFSRV_CONFIG_VALUE_TYPE_UNSPECIFIED, .value = {0},
    .field_index = UFSRV_CONFIG_FIELD_COUNT, .description = NULL
  }
};

#endif /* UFSRV_CORE_CONFIGFILE_PROCESSOR_DATA_CONFIG_SYMBOLS_CORE_PRIV_H */
