/**
* @file ufsrv_config_host.c
 * @brief host application specialisation of ufsrv_config module.
 *
 * The consumer allocates two UfsrvConfigValue arrays and registers them via
 * the Init functions.  The library holds private pointers and mediates every
 * access through Instance(idx), which provides the panic-before-init guard
 * via an internal function-pointer swap (panic → direct).
 *
 * Generated accessors in ufsrv_config_accessor_defs_generated.c route through
 * UfsrvConfigConfFileValuesInstance(idx) / UfsrvConfigRuntimeValuesInstance(idx)
 * and therefore carry the guard automatically.
 *
 * Copyright (C) 2015-2026 unfacd works
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifdef HAVE_CONFIG_UFSRVWEBSOCK_H
#include <config_ufsrvwebsock.h>
#endif

#include <ufsrvcorelib/ufsrv_core/ufsrv_config/ufsrv_config.h>
#include <ufsrvcorelib/ufsrv_core/ufsrv_config/ufsrv_config_pretty_printers.h>
#include "ufsrv_core/ufsrv_config/ufsrv_config_host.h"

#include <uflib/buffer_descriptor/buffer_descriptor.h>

#include "generated/data_ufsrv_config_field_tables_generated.c"

void
BootstrapRuntimeConfigStore()
{
 UfsrvConfigRuntimeStoreInit(ufsrv_runtime_field_table, ufsrv_runtime_field_defaults_table);
 UfsrvConfigInflateRuntimeStore();
}

void
BootstrapConfigurationFileConfiStore()
{
 UfsrvConfigConfFileStoreInit(ufsrv_config_field_table, ufsrv_config_field_defaults_table);
 UfsrvConfigInflateConfigStore();
}

void
LoadConfigurationFileConfiStoreDefaults(const bool is_forced_reload)
{
 UfsrvConfigConfFileStorePopulateDefaults(ufsrv_config_field_defaults_table, is_forced_reload);
}

void
LoadRuntimeConfigStoreDefaults(const bool is_forced_reload)
{
 UfsrvConfigRuntimeStorePopulateDefaults(ufsrv_runtime_field_defaults_table, is_forced_reload);
}

int
JsonFormatConfigurationFileConfigStore(BufferDescriptor *buffer_descriptor_provided)
{
 int rc = JsonFormatConfigStore(ufsrv_config_field_table, UFSRV_CONFIG_FIELD_COUNT, PRETTY_PRINT_SCOPE_DIGEST, buffer_descriptor_provided);

 return rc;
}

int
JsonFormatRuntimeConfigStore(BufferDescriptor *buffer_descriptor_provided)
{
 int rc = JsonFormatConfigStore(ufsrv_runtime_field_table, UFSRV_RUNTIME_FIELD_COUNT, PRETTY_PRINT_SCOPE_DIGEST, buffer_descriptor_provided);

 return rc;

}