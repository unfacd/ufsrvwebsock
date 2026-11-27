/**
 * @file ufsrv_config.c
 * @brief Singleton lifecycle — store init, Instance(idx) access, hot store
 *        pointers, teardown.
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

#ifdef HAVE_CONFIG_UFSRVCORELIB_H
#include <config_ufsrvcorelib.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <stdbool.h>
#include <stddef.h>
#include <ufsrvcorelib/ufsrv_core/ufsrv_config/ufsrv_config.h>
#include <ufsrvcorelib/ufsrv_core/ufsrv_config/ufsrv_config_value_type.h>

/**
 * IMPORTANT NOTICE this file is not included in normal build target. Typically, it get incorporated as part
 * of 'composition' at a host server, such as ufsrvswebsock.
 */

/*! The Config store array pointer injected by UfsrvConfigConfFileStoreInit(). */
static UfsrvConfigValue *      ufsrv_config_conf_file_values_store;
const static UfsrvConfigValue *ufsrv_config_conf_file_default_values_store;
static UfsrvConfigFieldEntry * ufsrv_config_conf_file_store;
/*! The Runtime store array pointer injected by UfsrvConfigRuntimeStoreInit(). */
static UfsrvConfigValue *      ufsrv_config_runtime_values_store;
const static UfsrvConfigValue *ufsrv_config_runtime_default_values_store;
static UfsrvConfigFieldEntry * ufsrv_config_runtime_store;

// Panic guards — abort if accessed before Init

/*!
 * Pre-init panic guard for Config store.
 * Prints a FATAL message to stderr and aborts the process.
 */
static UfsrvConfigValue *
sUfsrvConfigConfFileValuesPanic(enum UfsrvConfigField idx)
{
  fprintf(stderr, "FATAL: UfsrvConfig ConfFile store accessed before UfsrvConfigConfFileStoreInit()\n");
  abort();
  return NULL; /* unreachable */
}

static UfsrvConfigFieldEntry *
sUfsrvConfigConfFileStorePanic(enum UfsrvConfigField idx)
{
  fprintf(stderr, "FATAL: UfsrvConfig ConfFile store accessed before UfsrvConfigConfFileStoreInitx()\n");
  abort();
  return NULL; /* unreachable */
}

/*!
 * Post-init direct path for Config store.
 * Branch-free, lock-free — the function pointer target is predictable
 * after the first call.
 */
static UfsrvConfigValue *
sUfsrvConfigConfFileValuesDirect(enum UfsrvConfigField idx)
{
  /* ufsrv_config_conf_file_values_store was set to &store[0].config in
   * StoreInit.  Recover the parent UfsrvConfigFieldEntry by subtracting
   * offsetof(UfsrvConfigFieldEntry, config), then index with the full entry
   * stride (sizeof(UfsrvConfigFieldEntry)) rather than sizeof(UfsrvConfigValue).
   * config is no longer the first member — field_name now precedes it — so a
   * plain cast would land offsetof(config) bytes too far into each entry. */
  UfsrvConfigFieldEntry *base = (UfsrvConfigFieldEntry *)((char *)ufsrv_config_conf_file_values_store
                                                          - offsetof(UfsrvConfigFieldEntry, config));
  return &base[idx].config;
}

static const UfsrvConfigValue *
sUfsrvConfigConfFileDefaultValue(enum UfsrvConfigField idx)
{
  return &ufsrv_config_conf_file_default_values_store[idx];
}

static UfsrvConfigFieldEntry *
sUfsrvConfigConfFileStoreDirect(enum UfsrvConfigField idx)
{
  return &ufsrv_config_conf_file_store[idx];
}

/*!
 * Pre-init panic guard for Runtime store.
 */
static UfsrvConfigValue *
sUfsrvConfigRuntimeValuesPanic(enum UfsrvRuntimeField idx)
{
  (void)idx;
  fprintf(stderr, "FATAL: UfsrvConfig Runtime store accessed before UfsrvConfigRuntimeStoreInit()\n");
  abort();
  return NULL; /* unreachable */
}

static UfsrvConfigFieldEntry *
sUfsrvConfigRuntimeStorePanic(enum UfsrvRuntimeField idx)
{
  (void)idx;
  fprintf(stderr, "FATAL: UfsrvConfig Runtime store accessed before UfsrvConfigRuntimeStoreInitx()\n");
  abort();
  return NULL; /* unreachable */
}

/*!
 * Post-init direct path for Runtime store.
 */
static UfsrvConfigValue *
sUfsrvConfigRuntimeValuesDirect(enum UfsrvRuntimeField idx)
{
  // Recover the UfsrvConfigFieldEntry base by subtracting offsetof(config);
  // field_name now precedes config, so a plain cast mis-indexes by that offset.
  UfsrvConfigFieldEntry *base = (UfsrvConfigFieldEntry *)((char *)ufsrv_config_runtime_values_store
                                                          - offsetof(UfsrvConfigFieldEntry, config));
  return &base[idx].config;
}

/**
 * Retrieve a config field entry indexed at a pre-registered \p idx
 * @param idx Pre-registered config
 * @return An individual config field entry
 */
static UfsrvConfigFieldEntry *
sUfsrvConfigRuntimeStoreDirect(enum UfsrvRuntimeField idx)
{
  return &ufsrv_config_runtime_store[idx];
}

static const UfsrvConfigValue *
sUfsrvConfigRuntimeDefaultValue(enum UfsrvRuntimeField idx)
{
  return &ufsrv_config_runtime_default_values_store[idx];
}

// Active adapters — start as panic, swapped to direct by Init

static UfsrvConfigFieldEntry *(*ufsrv_config_conf_file_store_self)(enum UfsrvConfigField) = sUfsrvConfigConfFileStorePanic;
static UfsrvConfigValue *(*ufsrv_config_conf_file_values_self)(enum UfsrvConfigField) = sUfsrvConfigConfFileValuesPanic;

static UfsrvConfigFieldEntry *(*ufsrv_config_runtime_store_self)(enum UfsrvRuntimeField) = sUfsrvConfigRuntimeStorePanic;
static UfsrvConfigValue *(*ufsrv_config_runtime_values_self)(enum UfsrvRuntimeField) = sUfsrvConfigRuntimeValuesPanic;

// Lifecycle — Init

PUBLIC_API void
UfsrvConfigConfFileValuesInit(UfsrvConfigValue *store, const UfsrvConfigValue *store_defaults)
{
  ufsrv_config_conf_file_values_store         = store;
  ufsrv_config_conf_file_default_values_store = store_defaults;
  ufsrv_config_conf_file_values_self          = sUfsrvConfigConfFileValuesDirect; //dispatch function
}

PUBLIC_API void
UfsrvConfigConfFileStoreInit(UfsrvConfigFieldEntry *store, const UfsrvConfigValue *store_defaults)
{
  ufsrv_config_conf_file_store      = store;
  ufsrv_config_conf_file_store_self = sUfsrvConfigConfFileStoreDirect; //dispatch function
  UfsrvConfigConfFileValuesInit(&store->config, store_defaults);
}

PUBLIC_API void
UfsrvConfigRuntimeValuesInit(UfsrvConfigValue *store, const UfsrvConfigValue *store_defaults)
{
  ufsrv_config_runtime_values_store         = store;
  ufsrv_config_runtime_default_values_store = store_defaults;
  ufsrv_config_runtime_values_self          = sUfsrvConfigRuntimeValuesDirect;
}

/**
 *
 * @param store Main runtime store array, holding config field items.
 * @param store_defaults
 */
PUBLIC_API void
UfsrvConfigRuntimeStoreInit(UfsrvConfigFieldEntry *store, const UfsrvConfigValue *store_defaults)
{
  ufsrv_config_runtime_store      = store;
  ufsrv_config_runtime_store_self = sUfsrvConfigRuntimeStoreDirect;
  UfsrvConfigRuntimeValuesInit(&store->config, store_defaults);
}

// Lifecycle — Instance(idx)  (public API)

PUBLIC_API UfsrvConfigFieldEntry *
UfsrvConfigConfFileStoreInstance(enum UfsrvConfigField idx)
{
  return ufsrv_config_conf_file_store_self(idx);
}

PUBLIC_API UfsrvConfigValue *
UfsrvConfigConfFileValuesInstance(enum UfsrvConfigField idx)
{
  return ufsrv_config_conf_file_values_self(idx);
}

PUBLIC_API UfsrvConfigFieldEntry *
UfsrvConfigRuntimeStoreInstance(enum UfsrvRuntimeField idx)
{
  return ufsrv_config_runtime_store_self(idx);
}

PUBLIC_API UfsrvConfigValue *
UfsrvConfigRuntimeValuesInstance(enum UfsrvRuntimeField idx)
{
  return ufsrv_config_runtime_values_self(idx);
}

PUBLIC_API void
UfsrvConfigInflateConfigStore(void)
{
  for (int idx = UFSRV_CONFIG_FIELD_COUNT - 1; idx >= 0; idx--) {
    // note if refactored to use size_t: invert the loop to start with idx = 0, as size_t will not satisfy the condition of < 0 and roll over to max value for type
    UfsrvConfigFieldEntry *config_field_entry = UfsrvConfigConfFileStoreInstance(idx);
    config_field_entry->config.type_allocation_sz =  sUfsrvConfigConfFileDefaultValue(idx)->type_allocation_sz;
    if (IS_PRESENT(config_field_entry->lifecycle.on_inflated)) {
      config_field_entry->lifecycle.on_inflated(idx, &config_field_entry->config, false);
    }
  }
}

PUBLIC_API void
UfsrvConfigInflateRuntimeStore(void)
{
  for (int idx = UFSRV_RUNTIME_FIELD_COUNT - 1; idx >= 0; idx--) {
    // note if refactored to use size_t: invert the loop to start with idx = 0, as size_t will not satisfy the condition of < 0 and roll over to max value for type
    UfsrvConfigFieldEntry *config_field_entry = UfsrvConfigRuntimeStoreInstance(idx);
    config_field_entry->config.type_allocation_sz = sUfsrvConfigRuntimeDefaultValue(idx)->type_allocation_sz;
    if (IS_PRESENT(config_field_entry->lifecycle.on_inflated)) {
      config_field_entry->lifecycle.on_inflated(idx, &config_field_entry->config, false);
    }
  }
}

PUBLIC_API void
UfsrvConfigRuntimeTeardown(void)
{
  for (int i = UFSRV_RUNTIME_FIELD_COUNT - 1; i >= 0; i--) {
    UfsrvConfigFieldEntry *entry = UfsrvConfigRuntimeStoreInstance(i);
    if (entry->lifecycle.on_teardown) {
      UfsrvConfigValue *slot = ufsrv_config_runtime_values_self(i);
      if (slot->stored_value.ptr_val) {
        entry->lifecycle.on_teardown(i, slot);
        memset(slot, 0, sizeof(UfsrvConfigValue));
      }
    }
  }
}

PUBLIC_API void
UfsrvConfigConfFileStorePopulateDefaults(const UfsrvConfigValue *defaults_table, bool is_forced_reset)
{
  syslog(LOG_INFO, "%s: entry, operating_state=%d", __func__, UfsrvConfigConfFileStoreOperatingState());

  if (is_forced_reset) {
    syslog(LOG_INFO, "%s: NOT IMPLEMENTED forced zeroing %d CONFIG value slots", __func__, UFSRV_CONFIG_FIELD_COUNT);
    //fixme this will wipeout preallocted memory references. todo: implement individual reset functions
    /*for (size_t idx = 0; idx < UFSRV_CONFIG_FIELD_COUNT; idx++) {
      memset(UfsrvConfigConfFileValuesInstance(idx), 0, sizeof(UfsrvConfigValue));
      UfsrvConfigConfFileValuesInstance(idx)->type_allocation_sz = sUfsrvConfigConfFileDefaultValue(idx)->type_allocation_sz;
    }*/
  }

  for (size_t idx = 0; idx < UFSRV_CONFIG_FIELD_COUNT; idx++) {
    UfsrvConfigFieldEntry *entry = UfsrvConfigConfFileStoreInstance(idx);
    if (IS_PRESENT(entry->lifecycle.on_value_available)) {
      entry->lifecycle.on_value_available(idx, &defaults_table[idx], UFSRV_CONFIG_SOURCE_DEFAULTS);
      entry->config.source = UFSRV_CONFIG_SOURCE_DEFAULTS;
    }
  }

  UfsrvConfigRuntimeValuesInstance(UFSRV_RUNTIME_FIELD_CONF_FILE_STORE_OPERATING_STATE)->stored_value.int_val = UFSRV_CONFIG_STORE_OPERATING_DEFAULTS_LOADED;

  syslog(LOG_INFO, "UfsrvConfigConfFileStorePopulateDefaults: complete, %d fields loaded", UFSRV_CONFIG_FIELD_COUNT);
}

PUBLIC_API void
UfsrvConfigRuntimeStorePopulateDefaults(const UfsrvConfigValue *defaults_table, bool is_forced_reset)
{
  syslog(LOG_INFO, "UfsrvConfigRuntimeStorePopulateDefaults: entry, operating_state=%d", UfsrvConfigRuntimeStoreOperatingState());

  if (is_forced_reset) {
    syslog(LOG_INFO, "%s: NOT IMPLEMENTED forced zeroing %d RUNTIME value slots", __func__, UFSRV_RUNTIME_FIELD_COUNT);
    //fixme this will wipeout preallocted memory references. todo: implement individual reset functions
    /*for (size_t idx = 0; idx < UFSRV_RUNTIME_FIELD_COUNT; idx++) {
      memset(UfsrvConfigRuntimeValuesInstance(idx), 0, sizeof(UfsrvConfigValue));
      UfsrvConfigConfFileValuesInstance(idx)->type_allocation_sz = sUfsrvConfigRuntimeDefaultValue(idx)->type_allocation_sz;
    }*/
  }

  for (size_t idx = 0; idx < UFSRV_RUNTIME_FIELD_COUNT; idx++) {
    // Skip operating-state metadata fields — they are set post‑loop
    if (idx == UFSRV_RUNTIME_FIELD_CONF_FILE_STORE_OPERATING_STATE || idx == UFSRV_RUNTIME_FIELD_RUNTIME_STORE_OPERATING_STATE) continue;

    UfsrvConfigFieldEntry *entry = UfsrvConfigRuntimeStoreInstance(idx);
    if (IS_PRESENT(entry->lifecycle.on_value_available)) {
      entry->lifecycle.on_value_available(idx, &defaults_table[idx], UFSRV_CONFIG_SOURCE_DEFAULTS);
      entry->config.source = UFSRV_CONFIG_SOURCE_DEFAULTS;
    }
  }

  UfsrvConfigRuntimeValuesInstance(UFSRV_RUNTIME_FIELD_RUNTIME_STORE_OPERATING_STATE)->stored_value.int_val = UFSRV_CONFIG_STORE_OPERATING_DEFAULTS_LOADED;

  syslog(LOG_INFO, "UfsrvConfigRuntimeStorePopulateDefaults: complete, %d non‑metadata fields loaded", UFSRV_RUNTIME_FIELD_COUNT - 2);
}

PUBLIC_API void
UfsrvConfigPopulateDefaults(const UfsrvConfigValue *config_defaults_table, const UfsrvConfigValue *runtime_defaults_table, bool is_forced_reset)
{
  UfsrvConfigConfFileStorePopulateDefaults(config_defaults_table, is_forced_reset);
  UfsrvConfigRuntimeStorePopulateDefaults(runtime_defaults_table, is_forced_reset);
}

PUBLIC_API const char *
UfsrvConfigSourceNameLiteral(const UfsrvConfigSource src)
{
  switch (src) {
    case UFSRV_CONFIG_SOURCE_DEFAULTS:    return "defaults";
    case UFSRV_CONFIG_SOURCE_CONFIG_FILE: return "config_file";
    case UFSRV_CONFIG_SOURCE_CLI:         return "cli";
    case UFSRV_CONFIG_SOURCE_RUNTIME:     return "runtime";
    case UFSRV_CONFIG_SOURCE_UNASSIGNED:
    default:                              return "unassigned";
  }
}

PUBLIC_API const char *
UfsrvConfigTypeLiteral(const UfsrvConfigValueType type)
{
  switch (type) {
    case UFSRV_CONFIG_VALUE_TYPE_UNSPECIFIED: return "unspecified";
    case UFSRV_CONFIG_VALUE_INT:              return "int";
    case UFSRV_CONFIG_VALUE_STR:              return "string";
    case UFSRV_CONFIG_VALUE_PTR:              return "pointer";
    case UFSRV_CONFIG_VALUE_STRUCT:           return "struct";
    case UFSRV_CONFIG_VALUE_CONST_PTR:        return "const_pointer";
    case UFSRV_CONFIG_VALUE_ENUM:             return "enum";
    case UFSRV_CONFIG_VALUE_ARRAY:            return "array";
    case UFSRV_CONFIG_VALUE_COLLECTION:       return "collection";
    case UFSRV_CONFIG_VALUE_BOOL:             return "bool";
    case UFSRV_CONFIG_VALUE_DOUBLE:           return "double";
    default:                                  return "unknown";
  }
}