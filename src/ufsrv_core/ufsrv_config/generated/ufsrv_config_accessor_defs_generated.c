/* JOINED BY ufsrv_config compose from:
 *   /home/devops/repos/1.ufsrv/ufsrvwebsock/dev/scripts/ufsrv_config_generator/ufsrvcorelib/generated/ufsrv_config_accessor_defs_generated.c
 *   /home/devops/repos/1.ufsrv/ufsrvwebsock/dev/scripts/ufsrv_config_generator/ufnetcorelib/generated/ufsrv_config_accessor_defs_generated.c
 *   /home/devops/repos/1.ufsrv/ufsrvwebsock/dev/scripts/ufsrv_config_generator/ufsrvwebsock/generated/ufsrv_config_accessor_defs_generated.c
 */
/*
 * ufsrv_config_getters_setters_generated.c — GENERATED, DO NOT EDIT
 *
 * Version:    67
 * Generated:  Sun Nov 22 22:33:53 2026
 * Generator:  aa42f0acec8015b8cfd0669182be202dd836212c
 * Catalogue:  d4bb12e727d8ec36dff666a85d4d064353052fb3
 *   Config fields:   50
 *   Runtime fields:  31
 */

#include <stdlib.h>  /* malloc, free */
#include <string.h>  /* memcpy, strncpy */
#include <ufsrvcorelib/ufsrv_core/ufsrv_config/ufsrv_config.h>
#include "ufsrv_core/ufsrv_config/generated/ufsrv_config_field_enums_generated.h"
#include "ufsrv_core/ufsrv_config/generated/ufsrv_config_accessor_decls_generated.h"
#include "ufsrv_config_generic_callbacks_priv.h"
#include "ufsrv_config_stubs_as_string_priv.h"
#include <netinet/in.h>
#include <lua.h>
#include <ufsrvcorelib/ufsrv_core/ufsrv_config/ufsrv_config_field_descriptor.h>
#include <ufsrvcorelib/ufsrv_core/ufsrv_config/ufsrv_config_store_operating_state_enum.h>
#include <ufsrvcorelib/ufsrv_core/ufsrv_config/ufsrv_config_types.h>
#include <ufsrvcorelib/ufsrv_core/system/server_run_mode_enum.h>
#include <ufsrvcorelib/ufsrv_core/system/server_process_mode_enum.h>
#include <ufsrvcorelib/ufsrv_core/system/ufsrv_thread_cpu_affinity_descriptor_type.h>
#include <uflib/collection_descriptor_type.h>
#include <ufsrvcorelib/ufsrv_core/service/file_loader/file_loader_service_descriptor_type.h>
#include <ufnetcorelib/ufnet_msgcore/cloud_authorization_token_type.h>
#include <ufnetcorelib/ufnet_msgcore/msgqueue_backend/ufsrvmsgqueue_type.h>
#include <ufnetcorelib/ufnet_core/cache_backend/persistence_type.h>
#include <ufnetcorelib/ufnet_msgcore/utils_curve.h>
#include <ufnetcorelib/ufnet_core/ratelimit_v2/ratelimit_application_policy_type.h>
#include <limits.h>

// Config accessors

int
UfsrvConfigServerInstanceId(void)
{
    return (int)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_SERVER_INSTANCE_ID)->stored_value.int_val;
}

void
UfsrvConfigSetServerInstanceId(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_SERVER_INSTANCE_ID);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_SERVER_INSTANCE_ID, &item->value, config_source);
}

int
UfsrvConfigUfsrvGeogroup(void)
{
    return (int)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_UFSRV_GEOGROUP)->stored_value.int_val;
}

void
UfsrvConfigSetUfsrvGeogroup(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_UFSRV_GEOGROUP);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_UFSRV_GEOGROUP, &item->value, config_source);
}

enum ServerRunMode
UfsrvConfigServerRunMode(void)
{
    return (enum ServerRunMode)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_SERVER_RUN_MODE)->stored_value.int_val;
}

void
UfsrvConfigSetServerRunMode(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_SERVER_RUN_MODE);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_SERVER_RUN_MODE, &item->value, config_source);
}

int
UfsrvConfigCommandConsolePort(void)
{
    return (int)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_COMMAND_CONSOLE_PORT)->stored_value.int_val;
}

int *
UfsrvConfigCommandConsolePortAsPtr(void)
{
    return &UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_COMMAND_CONSOLE_PORT)->stored_value.int_val;
}

void
UfsrvConfigSetCommandConsolePort(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_COMMAND_CONSOLE_PORT);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_COMMAND_CONSOLE_PORT, &item->value, config_source);
}

char *
UfsrvConfigCommandConsoleAddress(void)
{
    return UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_COMMAND_CONSOLE_ADDRESS)->stored_value.str_val;
}

void
UfsrvConfigSetCommandConsoleAddress(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_COMMAND_CONSOLE_ADDRESS);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_COMMAND_CONSOLE_ADDRESS, &item->value, config_source);
}

enum CpuAffinityPolicy
UfsrvConfigServerCpuAffinity(void)
{
    return (enum CpuAffinityPolicy)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_SERVER_CPU_AFFINITY)->stored_value.int_val;
}

void
UfsrvConfigSetServerCpuAffinity(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_SERVER_CPU_AFFINITY);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_SERVER_CPU_AFFINITY, &item->value, config_source);
}

char *
UfsrvConfigSslCommandConsoleKeyFile(void)
{
    return UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_SSL_COMMAND_CONSOLE_KEY_FILE)->stored_value.str_val;
}

void
UfsrvConfigSetSslCommandConsoleKeyFile(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_SSL_COMMAND_CONSOLE_KEY_FILE);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_SSL_COMMAND_CONSOLE_KEY_FILE, &item->value, config_source);
}

char *
UfsrvConfigSslCommandConsoleCertificateFile(void)
{
    return UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_SSL_COMMAND_CONSOLE_CERTIFICATE_FILE)->stored_value.str_val;
}

void
UfsrvConfigSetSslCommandConsoleCertificateFile(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_SSL_COMMAND_CONSOLE_CERTIFICATE_FILE);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_SSL_COMMAND_CONSOLE_CERTIFICATE_FILE, &item->value, config_source);
}

bool
UfsrvConfigSslCommandConsoleIsEnabled(void)
{
    return (bool)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_SSL_COMMAND_CONSOLE_IS_ENABLED)->stored_value.int_val;
}

void
UfsrvConfigSetSslCommandConsoleIsEnabled(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_SSL_COMMAND_CONSOLE_IS_ENABLED);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_SSL_COMMAND_CONSOLE_IS_ENABLED, &item->value, config_source);
}

DistinctArray
UfsrvConfigSslCommandConsoleAllowListGod(void)
{
    return *(const DistinctArray *)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_SSL_COMMAND_CONSOLE_ALLOW_LIST_GOD)->stored_value.blob_val;
}

DistinctArray *
UfsrvConfigSslCommandConsoleAllowListGodAsPtr(void)
{
    return (DistinctArray *)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_SSL_COMMAND_CONSOLE_ALLOW_LIST_GOD)->stored_value.blob_val;
}

void
UfsrvConfigSslCommandConsoleAllowListGodSetter(int field_index, const UfsrvConfigValue *value_ptr, UfsrvConfigSource source)
{
    (void)field_index;
    UfsrvConfigItem item = { .layer = source, .value = *value_ptr };
    UfsrvConfigSetSslCommandConsoleAllowListGod(&item, source);
}

DistinctArray
UfsrvConfigSslCommandConsoleAllowListBrowser(void)
{
    return *(const DistinctArray *)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_SSL_COMMAND_CONSOLE_ALLOW_LIST_BROWSER)->stored_value.blob_val;
}

DistinctArray *
UfsrvConfigSslCommandConsoleAllowListBrowserAsPtr(void)
{
    return (DistinctArray *)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_SSL_COMMAND_CONSOLE_ALLOW_LIST_BROWSER)->stored_value.blob_val;
}

void
UfsrvConfigSslCommandConsoleAllowListBrowserSetter(int field_index, const UfsrvConfigValue *value_ptr, UfsrvConfigSource source)
{
    (void)field_index;
    UfsrvConfigItem item = { .layer = source, .value = *value_ptr };
    UfsrvConfigSetSslCommandConsoleAllowListBrowser(&item, source);
}

DistinctArray
UfsrvConfigSslCommandConsoleDenyList(void)
{
    return *(const DistinctArray *)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_SSL_COMMAND_CONSOLE_DENY_LIST)->stored_value.blob_val;
}

DistinctArray *
UfsrvConfigSslCommandConsoleDenyListAsPtr(void)
{
    return (DistinctArray *)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_SSL_COMMAND_CONSOLE_DENY_LIST)->stored_value.blob_val;
}

void
UfsrvConfigSslCommandConsoleDenyListSetter(int field_index, const UfsrvConfigValue *value_ptr, UfsrvConfigSource source)
{
    (void)field_index;
    UfsrvConfigItem item = { .layer = source, .value = *value_ptr };
    UfsrvConfigSetSslCommandConsoleDenyList(&item, source);
}

char *
UfsrvConfigSslCommandConsoleClientKeyFile(void)
{
    return UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_SSL_COMMAND_CONSOLE_CLIENT_KEY_FILE)->stored_value.str_val;
}

void
UfsrvConfigSetSslCommandConsoleClientKeyFile(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_SSL_COMMAND_CONSOLE_CLIENT_KEY_FILE);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_SSL_COMMAND_CONSOLE_CLIENT_KEY_FILE, &item->value, config_source);
}

char *
UfsrvConfigSslCommandConsoleClientCertificateFile(void)
{
    return UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_SSL_COMMAND_CONSOLE_CLIENT_CERTIFICATE_FILE)->stored_value.str_val;
}

void
UfsrvConfigSetSslCommandConsoleClientCertificateFile(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_SSL_COMMAND_CONSOLE_CLIENT_CERTIFICATE_FILE);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_SSL_COMMAND_CONSOLE_CLIENT_CERTIFICATE_FILE, &item->value, config_source);
}

bool
UfsrvConfigSslCommandConsoleClientIsEnabled(void)
{
    return (bool)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_SSL_COMMAND_CONSOLE_CLIENT_IS_ENABLED)->stored_value.int_val;
}

void
UfsrvConfigSetSslCommandConsoleClientIsEnabled(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_SSL_COMMAND_CONSOLE_CLIENT_IS_ENABLED);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_SSL_COMMAND_CONSOLE_CLIENT_IS_ENABLED, &item->value, config_source);
}

bool
UfsrvConfigSslConsoleIsClientCertificateRequired(void)
{
    return (bool)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_SSL_CONSOLE_IS_CLIENT_CERTIFICATE_REQUIRED)->stored_value.int_val;
}

void
UfsrvConfigSetSslConsoleIsClientCertificateRequired(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_SSL_CONSOLE_IS_CLIENT_CERTIFICATE_REQUIRED);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_SSL_CONSOLE_IS_CLIENT_CERTIFICATE_REQUIRED, &item->value, config_source);
}

int
UfsrvConfigFileloaderEnabled(void)
{
    return (int)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_FILELOADER_ENABLED)->stored_value.int_val;
}

void
UfsrvConfigSetFileloaderEnabled(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_FILELOADER_ENABLED);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_FILELOADER_ENABLED, &item->value, config_source);
}

size_t
UfsrvConfigFileloaderRegistrySize(void)
{
    return (size_t)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_FILELOADER_REGISTRY_SIZE)->stored_value.int_val;
}

void
UfsrvConfigSetFileloaderRegistrySize(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_FILELOADER_REGISTRY_SIZE);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_FILELOADER_REGISTRY_SIZE, &item->value, config_source);
}

size_t
UfsrvConfigFileloaderRegistryHotLoadedSize(void)
{
    return (size_t)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_FILELOADER_REGISTRY_HOT_LOADED_SIZE)->stored_value.int_val;
}

void
UfsrvConfigSetFileloaderRegistryHotLoadedSize(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_FILELOADER_REGISTRY_HOT_LOADED_SIZE);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_FILELOADER_REGISTRY_HOT_LOADED_SIZE, &item->value, config_source);
}

CollectionDescriptor
UfsrvConfigFileloaderBlackListedFilesRegistry(void)
{
    return *(const CollectionDescriptor *)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_FILELOADER_BLACK_LISTED_FILES_REGISTRY)->stored_value.blob_val;
}

CollectionDescriptor *
UfsrvConfigFileloaderBlackListedFilesRegistryAsPtr(void)
{
    return (CollectionDescriptor *)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_FILELOADER_BLACK_LISTED_FILES_REGISTRY)->stored_value.blob_val;
}

void
UfsrvConfigFileloaderBlackListedFilesRegistrySetter(int field_index, const UfsrvConfigValue *value_ptr, UfsrvConfigSource source)
{
    (void)field_index;
    UfsrvConfigItem item = { .layer = source, .value = *value_ptr };
    UfsrvConfigSetFileloaderBlackListedFilesRegistry(&item, source);
}

CollectionDescriptor
UfsrvConfigFileloaderFilesRegistry(void)
{
    return *(const CollectionDescriptor *)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_FILELOADER_FILES_REGISTRY)->stored_value.blob_val;
}

CollectionDescriptor *
UfsrvConfigFileloaderFilesRegistryAsPtr(void)
{
    return (CollectionDescriptor *)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_FILELOADER_FILES_REGISTRY)->stored_value.blob_val;
}

void
UfsrvConfigFileloaderFilesRegistrySetter(int field_index, const UfsrvConfigValue *value_ptr, UfsrvConfigSource source)
{
    (void)field_index;
    UfsrvConfigItem item = { .layer = source, .value = *value_ptr };
    UfsrvConfigSetFileloaderFilesRegistry(&item, source);
}

size_t
UfsrvConfigListenerWorkersThreadPool(void)
{
    return (size_t)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_LISTENER_WORKERS_THREAD_POOL)->stored_value.int_val;
}

void
UfsrvConfigSetListenerWorkersThreadPool(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_LISTENER_WORKERS_THREAD_POOL);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_LISTENER_WORKERS_THREAD_POOL, &item->value, config_source);
}

size_t
UfsrvConfigUfsrvWorkersThreadPool(void)
{
    return (size_t)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_UFSRV_WORKERS_THREAD_POOL)->stored_value.int_val;
}

void
UfsrvConfigSetUfsrvWorkersThreadPool(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_UFSRV_WORKERS_THREAD_POOL);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_UFSRV_WORKERS_THREAD_POOL, &item->value, config_source);
}

size_t
UfsrvConfigSessionWorkersThreadPool(void)
{
    return (size_t)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_SESSION_WORKERS_THREAD_POOL)->stored_value.int_val;
}

void
UfsrvConfigSetSessionWorkersThreadPool(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_SESSION_WORKERS_THREAD_POOL);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_SESSION_WORKERS_THREAD_POOL, &item->value, config_source);
}

size_t
UfsrvConfigMemorySpecsSessionsPerAllocationGroup(void)
{
    return (size_t)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_MEMORY_SPECS_SESSIONS_PER_ALLOCATION_GROUP)->stored_value.int_val;
}

void
UfsrvConfigSetMemorySpecsSessionsPerAllocationGroup(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_MEMORY_SPECS_SESSIONS_PER_ALLOCATION_GROUP);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_MEMORY_SPECS_SESSIONS_PER_ALLOCATION_GROUP, &item->value, config_source);
}

size_t
UfsrvConfigMemorySpecsAllocationGroups(void)
{
    return (size_t)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_MEMORY_SPECS_ALLOCATION_GROUPS)->stored_value.int_val;
}

void
UfsrvConfigSetMemorySpecsAllocationGroups(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_MEMORY_SPECS_ALLOCATION_GROUPS);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_MEMORY_SPECS_ALLOCATION_GROUPS, &item->value, config_source);
}

char *
UfsrvConfigMemorySpecsAllocationThresholdTrigger(void)
{
    return UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_MEMORY_SPECS_ALLOCATION_THRESHOLD_TRIGGER)->stored_value.str_val;
}

void
UfsrvConfigSetMemorySpecsAllocationThresholdTrigger(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_MEMORY_SPECS_ALLOCATION_THRESHOLD_TRIGGER);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_MEMORY_SPECS_ALLOCATION_THRESHOLD_TRIGGER, &item->value, config_source);
}

char *
UfsrvConfigStatsBackendAddress(void)
{
    return UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_STATS_BACKEND_ADDRESS)->stored_value.str_val;
}

void
UfsrvConfigSetStatsBackendAddress(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_STATS_BACKEND_ADDRESS);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_STATS_BACKEND_ADDRESS, &item->value, config_source);
}

unsigned
UfsrvConfigStatsBackendPort(void)
{
    return (unsigned)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_STATS_BACKEND_PORT)->stored_value.int_val;
}

void
UfsrvConfigSetStatsBackendPort(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_STATS_BACKEND_PORT);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_STATS_BACKEND_PORT, &item->value, config_source);
}

size_t
UfsrvConfigUserTimeoutsUnauthenticatedTimeout(void)
{
    return (size_t)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_USER_TIMEOUTS_UNAUTHENTICATED_TIMEOUT)->stored_value.int_val;
}

void
UfsrvConfigSetUserTimeoutsUnauthenticatedTimeout(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_USER_TIMEOUTS_UNAUTHENTICATED_TIMEOUT);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_USER_TIMEOUTS_UNAUTHENTICATED_TIMEOUT, &item->value, config_source);
}

size_t
UfsrvConfigUserTimeoutsConnectedTimeout(void)
{
    return (size_t)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_USER_TIMEOUTS_CONNECTED_TIMEOUT)->stored_value.int_val;
}

void
UfsrvConfigSetUserTimeoutsConnectedTimeout(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_USER_TIMEOUTS_CONNECTED_TIMEOUT);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_USER_TIMEOUTS_CONNECTED_TIMEOUT, &item->value, config_source);
}

size_t
UfsrvConfigUserTimeoutsSuspendedTimeout(void)
{
    return (size_t)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_USER_TIMEOUTS_SUSPENDED_TIMEOUT)->stored_value.int_val;
}

void
UfsrvConfigSetUserTimeoutsSuspendedTimeout(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_USER_TIMEOUTS_SUSPENDED_TIMEOUT);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_USER_TIMEOUTS_SUSPENDED_TIMEOUT, &item->value, config_source);
}

size_t
UfsrvConfigBufferSizesIncomingBufferSize(void)
{
    return (size_t)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_BUFFER_SIZES_INCOMING_BUFFER_SIZE)->stored_value.int_val;
}

void
UfsrvConfigSetBufferSizesIncomingBufferSize(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_BUFFER_SIZES_INCOMING_BUFFER_SIZE);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_BUFFER_SIZES_INCOMING_BUFFER_SIZE, &item->value, config_source);
}

size_t
UfsrvConfigBufferSizesIncomingBufferMaxsize(void)
{
    return (size_t)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_BUFFER_SIZES_INCOMING_BUFFER_MAXSIZE)->stored_value.int_val;
}

void
UfsrvConfigSetBufferSizesIncomingBufferMaxsize(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_BUFFER_SIZES_INCOMING_BUFFER_MAXSIZE);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_BUFFER_SIZES_INCOMING_BUFFER_MAXSIZE, &item->value, config_source);
}

size_t
UfsrvConfigBufferSizesOutgoingBufferSize(void)
{
    return (size_t)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_BUFFER_SIZES_OUTGOING_BUFFER_SIZE)->stored_value.int_val;
}

void
UfsrvConfigSetBufferSizesOutgoingBufferSize(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_BUFFER_SIZES_OUTGOING_BUFFER_SIZE);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_BUFFER_SIZES_OUTGOING_BUFFER_SIZE, &item->value, config_source);
}

size_t
UfsrvConfigBufferSizesHoldingBufferSize(void)
{
    return (size_t)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_BUFFER_SIZES_HOLDING_BUFFER_SIZE)->stored_value.int_val;
}

void
UfsrvConfigSetBufferSizesHoldingBufferSize(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_BUFFER_SIZES_HOLDING_BUFFER_SIZE);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_BUFFER_SIZES_HOLDING_BUFFER_SIZE, &item->value, config_source);
}

char *
UfsrvConfigSysRuntimeRunAsUser(void)
{
    return UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_SYS_RUNTIME_RUN_AS_USER)->stored_value.str_val;
}

void
UfsrvConfigSetSysRuntimeRunAsUser(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_SYS_RUNTIME_RUN_AS_USER);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_SYS_RUNTIME_RUN_AS_USER, &item->value, config_source);
}

char *
UfsrvConfigSysRuntimeChroot(void)
{
    return UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_SYS_RUNTIME_CHROOT)->stored_value.str_val;
}

void
UfsrvConfigSetSysRuntimeChroot(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_SYS_RUNTIME_CHROOT);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_SYS_RUNTIME_CHROOT, &item->value, config_source);
}

char *
UfsrvConfigLoggerDriver(void)
{
    return UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_LOGGER_DRIVER)->stored_value.str_val;
}

void
UfsrvConfigSetLoggerDriver(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_LOGGER_DRIVER);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_LOGGER_DRIVER, &item->value, config_source);
}

char *
UfsrvConfigLoggerFormat(void)
{
    return UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_LOGGER_FORMAT)->stored_value.str_val;
}

void
UfsrvConfigSetLoggerFormat(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_LOGGER_FORMAT);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_LOGGER_FORMAT, &item->value, config_source);
}

char *
UfsrvConfigLoggerPrefix(void)
{
    return UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_LOGGER_PREFIX)->stored_value.str_val;
}

void
UfsrvConfigSetLoggerPrefix(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_LOGGER_PREFIX);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_LOGGER_PREFIX, &item->value, config_source);
}

bool
UfsrvConfigRatelimitIsEnabled(void)
{
    return (bool)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_RATELIMIT_IS_ENABLED)->stored_value.int_val;
}

void
UfsrvConfigSetRatelimitIsEnabled(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_RATELIMIT_IS_ENABLED);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_RATELIMIT_IS_ENABLED, &item->value, config_source);
}

bool
UfsrvConfigRatelimitIsBypassPrivateIp(void)
{
    return (bool)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_RATELIMIT_IS_BYPASS_PRIVATE_IP)->stored_value.int_val;
}

void
UfsrvConfigSetRatelimitIsBypassPrivateIp(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_RATELIMIT_IS_BYPASS_PRIVATE_IP);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_RATELIMIT_IS_BYPASS_PRIVATE_IP, &item->value, config_source);
}

char *
UfsrvConfigOauthAccessTokenFormat(void)
{
    return UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_OAUTH_ACCESS_TOKEN_FORMAT)->stored_value.str_val;
}

void
UfsrvConfigSetOauthAccessTokenFormat(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_OAUTH_ACCESS_TOKEN_FORMAT);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_OAUTH_ACCESS_TOKEN_FORMAT, &item->value, config_source);
}

char *
UfsrvConfigOauthJwtIssuer(void)
{
    return UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_OAUTH_JWT_ISSUER)->stored_value.str_val;
}

void
UfsrvConfigSetOauthJwtIssuer(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_OAUTH_JWT_ISSUER);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_OAUTH_JWT_ISSUER, &item->value, config_source);
}

char *
UfsrvConfigOauthJwtSigningKey(void)
{
    return UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_OAUTH_JWT_SIGNING_KEY)->stored_value.str_val;
}

void
UfsrvConfigSetOauthJwtSigningKey(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_OAUTH_JWT_SIGNING_KEY);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_OAUTH_JWT_SIGNING_KEY, &item->value, config_source);
}

char *
UfsrvConfigOauthJwtActiveKid(void)
{
    return UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_OAUTH_JWT_ACTIVE_KID)->stored_value.str_val;
}

void
UfsrvConfigSetOauthJwtActiveKid(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_OAUTH_JWT_ACTIVE_KID);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_OAUTH_JWT_ACTIVE_KID, &item->value, config_source);
}

char *
UfsrvConfigEmailApiKey(void)
{
    return UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_EMAIL_API_KEY)->stored_value.str_val;
}

void
UfsrvConfigSetEmailApiKey(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_EMAIL_API_KEY);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_EMAIL_API_KEY, &item->value, config_source);
}

char *
UfsrvConfigEmailApiUrl(void)
{
    return UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_EMAIL_API_URL)->stored_value.str_val;
}

void
UfsrvConfigSetEmailApiUrl(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_EMAIL_API_URL);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_EMAIL_API_URL, &item->value, config_source);
}

CollectionDescriptor
UfsrvConfigListeners(void)
{
    return *(const CollectionDescriptor *)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_LISTENERS)->stored_value.blob_val;
}

CollectionDescriptor *
UfsrvConfigListenersAsPtr(void)
{
    return (CollectionDescriptor *)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_LISTENERS)->stored_value.blob_val;
}

void
UfsrvConfigListenersSetter(int field_index, const UfsrvConfigValue *value_ptr, UfsrvConfigSource source)
{
    (void)field_index;
    UfsrvConfigItem item = { .layer = source, .value = *value_ptr };
    UfsrvConfigSetListeners(&item, source);
}

// Runtime accessors

enum UfsrvConfigStoreOperatingState
UfsrvConfigConfFileStoreOperatingState(void)
{
    return (enum UfsrvConfigStoreOperatingState)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_CONF_FILE_STORE_OPERATING_STATE)->stored_value.int_val;
}

void
UfsrvConfigSetConfFileStoreOperatingState(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_CONF_FILE_STORE_OPERATING_STATE);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_CONF_FILE_STORE_OPERATING_STATE, &item->value, config_source);
}

enum UfsrvConfigStoreOperatingState
UfsrvConfigRuntimeStoreOperatingState(void)
{
    return (enum UfsrvConfigStoreOperatingState)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_RUNTIME_STORE_OPERATING_STATE)->stored_value.int_val;
}

void
UfsrvConfigSetRuntimeStoreOperatingState(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_RUNTIME_STORE_OPERATING_STATE);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_RUNTIME_STORE_OPERATING_STATE, &item->value, config_source);
}

unsigned char
UfsrvConfigUfsrvSecretKey(void)
{
    return *(const unsigned char *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_UFSRV_SECRET_KEY)->stored_value.blob_val;
}

unsigned char *
UfsrvConfigUfsrvSecretKeyAsPtr(void)
{
    return (unsigned char *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_UFSRV_SECRET_KEY)->stored_value.blob_val;
}

void
UfsrvConfigSetUfsrvSecretKey(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_UFSRV_SECRET_KEY);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_UFSRV_SECRET_KEY, &item->value, config_source);
}

char *
UfsrvConfigUfsrvSecretKeyStore(void)
{
    return UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_UFSRV_SECRET_KEY_STORE)->stored_value.str_val;
}

void
UfsrvConfigSetUfsrvSecretKeyStore(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_UFSRV_SECRET_KEY_STORE);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_UFSRV_SECRET_KEY_STORE, &item->value, config_source);
}

unsigned
UfsrvConfigServeridRuntimeRandom(void)
{
    return (unsigned)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_SERVERID_RUNTIME_RANDOM)->stored_value.int_val;
}

void
UfsrvConfigSetServeridRuntimeRandom(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_SERVERID_RUNTIME_RANDOM);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_SERVERID_RUNTIME_RANDOM, &item->value, config_source);
}

pid_t
UfsrvConfigServerpid(void)
{
    return (pid_t)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_SERVERPID)->stored_value.int_val;
}

void
UfsrvConfigSetServerpid(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_SERVERPID);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_SERVERPID, &item->value, config_source);
}

char *
UfsrvConfigServerClass(void)
{
    return UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_SERVER_CLASS)->stored_value.str_val;
}

void
UfsrvConfigSetServerClass(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_SERVER_CLASS);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_SERVER_CLASS, &item->value, config_source);
}

char *
UfsrvConfigServerDescriptiveName(void)
{
    return UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_SERVER_DESCRIPTIVE_NAME)->stored_value.str_val;
}

void
UfsrvConfigSetServerDescriptiveName(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_SERVER_DESCRIPTIVE_NAME);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_SERVER_DESCRIPTIVE_NAME, &item->value, config_source);
}

time_t
UfsrvConfigWhen(void)
{
    return (time_t)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_WHEN)->stored_value.int_val;
}

void
UfsrvConfigSetWhen(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_WHEN);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_WHEN, &item->value, config_source);
}

struct timeval
UfsrvConfigUfsrvTime(void)
{
    return *(const struct timeval *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_UFSRV_TIME)->stored_value.blob_val;
}

struct timeval *
UfsrvConfigUfsrvTimeAsPtr(void)
{
    return (struct timeval *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_UFSRV_TIME)->stored_value.blob_val;
}

void
UfsrvConfigSetUfsrvTime(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_UFSRV_TIME);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_UFSRV_TIME, &item->value, config_source);
}

enum ServerProcessMode
UfsrvConfigServerProcessMode(void)
{
    return (enum ServerProcessMode)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_SERVER_PROCESS_MODE)->stored_value.int_val;
}

void
UfsrvConfigSetServerProcessMode(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_SERVER_PROCESS_MODE);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_SERVER_PROCESS_MODE, &item->value, config_source);
}

unsigned
UfsrvConfigLogMode(void)
{
    return (unsigned)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_LOG_MODE)->stored_value.int_val;
}

void
UfsrvConfigSetLogMode(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_LOG_MODE);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_LOG_MODE, &item->value, config_source);
}

unsigned
UfsrvConfigSslSupport(void)
{
    return (unsigned)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_SSL_SUPPORT)->stored_value.int_val;
}

void
UfsrvConfigSetSslSupport(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_SSL_SUPPORT);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_SSL_SUPPORT, &item->value, config_source);
}

char *
UfsrvConfigConfigDir(void)
{
    return UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_CONFIG_DIR)->stored_value.str_val;
}

void
UfsrvConfigSetConfigDir(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_CONFIG_DIR);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_CONFIG_DIR, &item->value, config_source);
}

char *
UfsrvConfigConfigFile(void)
{
    return UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_CONFIG_FILE)->stored_value.str_val;
}

void
UfsrvConfigSetConfigFile(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_CONFIG_FILE);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_CONFIG_FILE, &item->value, config_source);
}

lua_State *
UfsrvConfigLuaHandle(void)
{
    return (lua_State *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_LUA_HANDLE)->stored_value.ptr_val;
}

void
UfsrvConfigSetLuaHandle(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_LUA_HANDLE);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_LUA_HANDLE, &item->value, config_source);
}

InstrumentationBackend *
UfsrvConfigInstrumentationBackend(void)
{
    return (InstrumentationBackend *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_INSTRUMENTATION_BACKEND)->stored_value.ptr_val;
}

void
UfsrvConfigSetInstrumentationBackend(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_INSTRUMENTATION_BACKEND);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_INSTRUMENTATION_BACKEND, &item->value, config_source);
}

struct sockaddr_in
UfsrvConfigInstrumentationBackendServer(void)
{
    return *(const struct sockaddr_in *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_INSTRUMENTATION_BACKEND_SERVER)->stored_value.blob_val;
}

struct sockaddr_in *
UfsrvConfigInstrumentationBackendServerAsPtr(void)
{
    return (struct sockaddr_in *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_INSTRUMENTATION_BACKEND_SERVER)->stored_value.blob_val;
}

void
UfsrvConfigSetInstrumentationBackendServer(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_INSTRUMENTATION_BACKEND_SERVER);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_INSTRUMENTATION_BACKEND_SERVER, &item->value, config_source);
}

SessionsDelegator *
UfsrvConfigSessionsDelegator(void)
{
    return (SessionsDelegator *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_SESSIONS_DELEGATOR)->stored_value.ptr_val;
}

void
UfsrvConfigSetSessionsDelegator(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_SESSIONS_DELEGATOR);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_SESSIONS_DELEGATOR, &item->value, config_source);
}

InstanceHolderForSession *
UfsrvConfigWorkDelegatorPipe(void)
{
    return (InstanceHolderForSession *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_WORK_DELEGATOR_PIPE)->stored_value.ptr_val;
}

InstanceHolderForSession * *
UfsrvConfigWorkDelegatorPipeAsPtr(void)
{
    return (InstanceHolderForSession * *)&UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_WORK_DELEGATOR_PIPE)->stored_value.ptr_val;
}

void
UfsrvConfigSetWorkDelegatorPipe(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_WORK_DELEGATOR_PIPE);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_WORK_DELEGATOR_PIPE, &item->value, config_source);
}


void
UfsrvConfigSetPipefds(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_PIPEFDS);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_PIPEFDS, &item->value, config_source);
}

SSL *
UfsrvConfigSslCommandConsole(void)
{
    return (SSL *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_SSL_COMMAND_CONSOLE)->stored_value.ptr_val;
}

void
UfsrvConfigSetSslCommandConsole(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_SSL_COMMAND_CONSOLE);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_SSL_COMMAND_CONSOLE, &item->value, config_source);
}

SSL_CTX *
UfsrvConfigSslCtx(void)
{
    return (SSL_CTX *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_SSL_CTX)->stored_value.ptr_val;
}

void
UfsrvConfigSetSslCtx(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_SSL_CTX);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_SSL_CTX, &item->value, config_source);
}

SSL_CTX *
UfsrvConfigSslUserCtx(void)
{
    return (SSL_CTX *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_SSL_USER_CTX)->stored_value.ptr_val;
}

void
UfsrvConfigSetSslUserCtx(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_SSL_USER_CTX);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_SSL_USER_CTX, &item->value, config_source);
}

pthread_mutex_t *
UfsrvConfigSslMutexes(void)
{
    return (pthread_mutex_t *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_SSL_MUTEXES)->stored_value.ptr_val;
}

void
UfsrvConfigSetSslMutexes(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_SSL_MUTEXES);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_SSL_MUTEXES, &item->value, config_source);
}

pthread_t
UfsrvConfigThreadHandleTimerDelegator(void)
{
    return *(const pthread_t *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_THREAD_HANDLE_TIMER_DELEGATOR)->stored_value.blob_val;
}

pthread_t *
UfsrvConfigThreadHandleTimerDelegatorAsPtr(void)
{
    return (pthread_t *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_THREAD_HANDLE_TIMER_DELEGATOR)->stored_value.blob_val;
}

void
UfsrvConfigSetThreadHandleTimerDelegator(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_THREAD_HANDLE_TIMER_DELEGATOR);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_THREAD_HANDLE_TIMER_DELEGATOR, &item->value, config_source);
}

FileLoaderServiceDescriptor
UfsrvConfigFileLoaderService(void)
{
    return *(const FileLoaderServiceDescriptor *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_FILE_LOADER_SERVICE)->stored_value.blob_val;
}

FileLoaderServiceDescriptor *
UfsrvConfigFileLoaderServiceAsPtr(void)
{
    return (FileLoaderServiceDescriptor *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_FILE_LOADER_SERVICE)->stored_value.blob_val;
}

void
UfsrvConfigSetFileLoaderService(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_FILE_LOADER_SERVICE);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_FILE_LOADER_SERVICE, &item->value, config_source);
}

UfsrvConfigOauth
UfsrvConfigOauthSettings(void)
{
    return *(const UfsrvConfigOauth *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_OAUTH_SETTINGS)->stored_value.blob_val;
}

UfsrvConfigOauth *
UfsrvConfigOauthSettingsAsPtr(void)
{
    return (UfsrvConfigOauth *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_OAUTH_SETTINGS)->stored_value.blob_val;
}

void
UfsrvConfigOauthSettingsSetter(int field_index, const UfsrvConfigValue *value_ptr, UfsrvConfigSource source)
{
    (void)field_index;
    UfsrvConfigItem item = { .layer = source, .value = *value_ptr };
    UfsrvConfigSetOauthSettings(&item, source);
}

UFSRVResult
UfsrvConfigUfsrvResult(void)
{
    return *(const UFSRVResult *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_UFSRV_RESULT)->stored_value.blob_val;
}

UFSRVResult *
UfsrvConfigUfsrvResultAsPtr(void)
{
    return (UFSRVResult *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_UFSRV_RESULT)->stored_value.blob_val;
}

void
UfsrvConfigSetUfsrvResult(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_UFSRV_RESULT);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_UFSRV_RESULT, &item->value, config_source);
}

pthread_once_t
UfsrvConfigThreadsSubsystemUfsrvOnce(void)
{
    return *(const pthread_once_t *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_THREADS_SUBSYSTEM_UFSRV_ONCE)->stored_value.blob_val;
}

pthread_once_t *
UfsrvConfigThreadsSubsystemUfsrvOnceAsPtr(void)
{
    return (pthread_once_t *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_THREADS_SUBSYSTEM_UFSRV_ONCE)->stored_value.blob_val;
}

void
UfsrvConfigSetThreadsSubsystemUfsrvOnce(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_THREADS_SUBSYSTEM_UFSRV_ONCE);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_THREADS_SUBSYSTEM_UFSRV_ONCE, &item->value, config_source);
}

UfLogger *
UfsrvConfigUfLogger(void)
{
    return (UfLogger *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_UF_LOGGER)->stored_value.ptr_val;
}

void
UfsrvConfigSetUfLogger(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_UF_LOGGER);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_UF_LOGGER, &item->value, config_source);
}

// ── AsString marshallers — accessor per field ─────────────────────────

BufferDescriptor *
UfsrvConfigConfFileStoreOperatingStateAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigConfFileStoreOperatingStateAsString_enum_UfsrvConfigStoreOperatingState(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigRuntimeStoreOperatingStateAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigRuntimeStoreOperatingStateAsString_enum_UfsrvConfigStoreOperatingState(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigUfsrvSecretKeyAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericUnsignedCharAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigUfsrvSecretKeyStoreAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigServeridRuntimeRandomAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericUnsignedAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigServerInstanceIdAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericIntAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigServerpidAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericPidTAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigServerClassAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigServerDescriptiveNameAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigUfsrvGeogroupAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericIntAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigWhenAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericTimeTAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigUfsrvTimeAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericTimevalAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigServerProcessModeAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigServerProcessModeAsString_enum_ServerProcessMode(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigServerRunModeAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigServerRunModeAsString_enum_ServerRunMode(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigLogModeAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericUnsignedAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigSslSupportAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericUnsignedAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigCommandConsolePortAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericIntAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigCommandConsoleAddressAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigConfigDirAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigConfigFileAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigServerCpuAffinityAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigServerCpuAffinityAsString_enum_CpuAffinityPolicy(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigLuaHandleAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigLuaHandleAsString_lua_State_pointer(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigInstrumentationBackendAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigInstrumentationBackendAsString_InstrumentationBackend_pointer(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigInstrumentationBackendServerAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigInstrumentationBackendServerAsString_struct_sockaddr_in(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigSessionsDelegatorAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigSessionsDelegatorAsString_SessionsDelegator_pointer(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigWorkDelegatorPipeAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigWorkDelegatorPipeAsString_InstanceHolderForSession_pointer(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigPipefdsAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigPipefdsAsString_int_pointer(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigSslCommandConsoleAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigSslCommandConsoleAsString_SSL_pointer(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigSslCommandConsoleKeyFileAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigSslCommandConsoleCertificateFileAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigSslCommandConsoleIsEnabledAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericBoolAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigSslCommandConsoleAllowListGodAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigSslCommandConsoleAllowListGodAsString_DistinctArray(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigSslCommandConsoleAllowListBrowserAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigSslCommandConsoleAllowListBrowserAsString_DistinctArray(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigSslCommandConsoleDenyListAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigSslCommandConsoleDenyListAsString_DistinctArray(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigSslCommandConsoleClientKeyFileAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigSslCommandConsoleClientCertificateFileAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigSslCommandConsoleClientIsEnabledAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericBoolAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigSslConsoleIsClientCertificateRequiredAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericBoolAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigSslCtxAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigSslCtxAsString_SSL_CTX_pointer(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigSslUserCtxAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigSslUserCtxAsString_SSL_CTX_pointer(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigSslMutexesAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigSslMutexesAsString_pthread_mutex_t_pointer(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigThreadHandleTimerDelegatorAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigThreadHandleTimerDelegatorAsString_pthread_t(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigFileloaderEnabledAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericIntAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigFileloaderRegistrySizeAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericSizeTAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigFileloaderRegistryHotLoadedSizeAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericSizeTAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigFileloaderBlackListedFilesRegistryAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigFileloaderBlackListedFilesRegistryAsString_CollectionDescriptor(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigFileloaderFilesRegistryAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigFileloaderFilesRegistryAsString_CollectionDescriptor(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigFileLoaderServiceAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigFileLoaderServiceAsString_FileLoaderServiceDescriptor(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigListenerWorkersThreadPoolAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericSizeTAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigUfsrvWorkersThreadPoolAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericSizeTAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigSessionWorkersThreadPoolAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericSizeTAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigMemorySpecsSessionsPerAllocationGroupAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericSizeTAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigMemorySpecsAllocationGroupsAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericSizeTAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigMemorySpecsAllocationThresholdTriggerAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigStatsBackendAddressAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigStatsBackendPortAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericUnsignedAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigUserTimeoutsUnauthenticatedTimeoutAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericSizeTAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigUserTimeoutsConnectedTimeoutAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericSizeTAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigUserTimeoutsSuspendedTimeoutAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericSizeTAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigBufferSizesIncomingBufferSizeAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericSizeTAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigBufferSizesIncomingBufferMaxsizeAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericSizeTAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigBufferSizesOutgoingBufferSizeAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericSizeTAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigBufferSizesHoldingBufferSizeAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericSizeTAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigSysRuntimeRunAsUserAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigSysRuntimeChrootAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigLoggerDriverAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigLoggerFormatAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigLoggerPrefixAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigRatelimitIsEnabledAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericBoolAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigRatelimitIsBypassPrivateIpAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericBoolAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigOauthAccessTokenFormatAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigOauthJwtIssuerAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigOauthJwtSigningKeyAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigOauthJwtActiveKidAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigOauthSettingsAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigOauthSettingsAsString_UfsrvConfigOauth(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigUfsrvResultAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigUfsrvResultAsString_UFSRVResult(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigThreadsSubsystemUfsrvOnceAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigThreadsSubsystemUfsrvOnceAsString_pthread_once_t(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigUfLoggerAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigUfLoggerAsString_UfLogger_pointer(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigEmailApiKeyAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigEmailApiUrlAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigListenersAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigListenersAsString_CollectionDescriptor(field_index, value_ptr, buffer_provided);
}

// Config accessors

char *
UfsrvConfigUfsrvmediaUploadUri(void)
{
    return UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_UFSRVMEDIA_UPLOAD_URI)->stored_value.str_val;
}

void
UfsrvConfigSetUfsrvmediaUploadUri(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_UFSRVMEDIA_UPLOAD_URI);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_UFSRVMEDIA_UPLOAD_URI, &item->value, config_source);
}

char *
UfsrvConfigMsgqueueAddress(void)
{
    return UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_MSGQUEUE_ADDRESS)->stored_value.str_val;
}

void
UfsrvConfigSetMsgqueueAddress(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_MSGQUEUE_ADDRESS);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_MSGQUEUE_ADDRESS, &item->value, config_source);
}

unsigned
UfsrvConfigMsgqueuePort(void)
{
    return (unsigned)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_MSGQUEUE_PORT)->stored_value.int_val;
}

void
UfsrvConfigSetMsgqueuePort(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_MSGQUEUE_PORT);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_MSGQUEUE_PORT, &item->value, config_source);
}

char *
UfsrvConfigPersistenceBackendAddress(void)
{
    return UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_PERSISTENCE_BACKEND_ADDRESS)->stored_value.str_val;
}

void
UfsrvConfigSetPersistenceBackendAddress(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_PERSISTENCE_BACKEND_ADDRESS);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_PERSISTENCE_BACKEND_ADDRESS, &item->value, config_source);
}

unsigned
UfsrvConfigPersistenceBackendPort(void)
{
    return (unsigned)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_PERSISTENCE_BACKEND_PORT)->stored_value.int_val;
}

void
UfsrvConfigSetPersistenceBackendPort(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_PERSISTENCE_BACKEND_PORT);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_PERSISTENCE_BACKEND_PORT, &item->value, config_source);
}

char *
UfsrvConfigCacheBackendAddressUsrmsg(void)
{
    return UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_CACHE_BACKEND_ADDRESS_USRMSG)->stored_value.str_val;
}

void
UfsrvConfigSetCacheBackendAddressUsrmsg(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_CACHE_BACKEND_ADDRESS_USRMSG);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_CACHE_BACKEND_ADDRESS_USRMSG, &item->value, config_source);
}

unsigned
UfsrvConfigCacheBackendPortUsrmsg(void)
{
    return (unsigned)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_CACHE_BACKEND_PORT_USRMSG)->stored_value.int_val;
}

void
UfsrvConfigSetCacheBackendPortUsrmsg(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_CACHE_BACKEND_PORT_USRMSG);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_CACHE_BACKEND_PORT_USRMSG, &item->value, config_source);
}

char *
UfsrvConfigCacheBackendAddressFence(void)
{
    return UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_CACHE_BACKEND_ADDRESS_FENCE)->stored_value.str_val;
}

void
UfsrvConfigSetCacheBackendAddressFence(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_CACHE_BACKEND_ADDRESS_FENCE);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_CACHE_BACKEND_ADDRESS_FENCE, &item->value, config_source);
}

unsigned
UfsrvConfigCacheBackendPortFence(void)
{
    return (unsigned)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_CACHE_BACKEND_PORT_FENCE)->stored_value.int_val;
}

void
UfsrvConfigSetCacheBackendPortFence(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_CACHE_BACKEND_PORT_FENCE);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_CACHE_BACKEND_PORT_FENCE, &item->value, config_source);
}

char *
UfsrvConfigGeoipServiceAddress(void)
{
    return UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_GEOIP_SERVICE_ADDRESS)->stored_value.str_val;
}

void
UfsrvConfigSetGeoipServiceAddress(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_GEOIP_SERVICE_ADDRESS);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_GEOIP_SERVICE_ADDRESS, &item->value, config_source);
}

unsigned
UfsrvConfigGeoipServicePort(void)
{
    return (unsigned)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_GEOIP_SERVICE_PORT)->stored_value.int_val;
}

void
UfsrvConfigSetGeoipServicePort(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_GEOIP_SERVICE_PORT);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_GEOIP_SERVICE_PORT, &item->value, config_source);
}

char *
UfsrvConfigDbBackendAddress(void)
{
    return UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_DB_BACKEND_ADDRESS)->stored_value.str_val;
}

void
UfsrvConfigSetDbBackendAddress(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_DB_BACKEND_ADDRESS);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_DB_BACKEND_ADDRESS, &item->value, config_source);
}

unsigned
UfsrvConfigDbBackendPort(void)
{
    return (unsigned)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_DB_BACKEND_PORT)->stored_value.int_val;
}

void
UfsrvConfigSetDbBackendPort(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_DB_BACKEND_PORT);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_DB_BACKEND_PORT, &item->value, config_source);
}

char *
UfsrvConfigDbBackendUsername(void)
{
    return UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_DB_BACKEND_USERNAME)->stored_value.str_val;
}

void
UfsrvConfigSetDbBackendUsername(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_DB_BACKEND_USERNAME);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_DB_BACKEND_USERNAME, &item->value, config_source);
}

char *
UfsrvConfigDbBackendPassword(void)
{
    return UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_DB_BACKEND_PASSWORD)->stored_value.str_val;
}

void
UfsrvConfigSetDbBackendPassword(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_DB_BACKEND_PASSWORD);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_DB_BACKEND_PASSWORD, &item->value, config_source);
}

// Runtime accessors

MessageQueueBackend *
UfsrvConfigMsgqueueSub(void)
{
    return (MessageQueueBackend *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_MSGQUEUE_SUB)->stored_value.ptr_val;
}

void
UfsrvConfigSetMsgqueueSub(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_MSGQUEUE_SUB);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_MSGQUEUE_SUB, &item->value, config_source);
}

PersistenceBackend *
UfsrvConfigPersistenceBackend(void)
{
    return (PersistenceBackend *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_PERSISTENCE_BACKEND)->stored_value.ptr_val;
}

void
UfsrvConfigSetPersistenceBackend(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_PERSISTENCE_BACKEND);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_PERSISTENCE_BACKEND, &item->value, config_source);
}

UserMessageCacheBackend *
UfsrvConfigUsrmsgCachebackend(void)
{
    return (UserMessageCacheBackend *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_USRMSG_CACHEBACKEND)->stored_value.ptr_val;
}

void
UfsrvConfigSetUsrmsgCachebackend(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_USRMSG_CACHEBACKEND);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_USRMSG_CACHEBACKEND, &item->value, config_source);
}

FenceCacheBackend *
UfsrvConfigFenceCachebackend(void)
{
    return (FenceCacheBackend *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_FENCE_CACHEBACKEND)->stored_value.ptr_val;
}

void
UfsrvConfigSetFenceCachebackend(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_FENCE_CACHEBACKEND);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_FENCE_CACHEBACKEND, &item->value, config_source);
}

CloudAuthorizationToken *
UfsrvConfigCloudAuthorizationFcm(void)
{
    return (CloudAuthorizationToken *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_CLOUD_AUTHORIZATION_FCM)->stored_value.ptr_val;
}

void
UfsrvConfigSetCloudAuthorizationFcm(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_CLOUD_AUTHORIZATION_FCM);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_CLOUD_AUTHORIZATION_FCM, &item->value, config_source);
}

CloudAuthorizationToken *
UfsrvConfigCloudAuthorizationIntegrityApi(void)
{
    return (CloudAuthorizationToken *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_CLOUD_AUTHORIZATION_INTEGRITY_API)->stored_value.ptr_val;
}

void
UfsrvConfigSetCloudAuthorizationIntegrityApi(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_CLOUD_AUTHORIZATION_INTEGRITY_API);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_CLOUD_AUTHORIZATION_INTEGRITY_API, &item->value, config_source);
}

uint8_t
UfsrvConfigCryptoPrivateServerParams(void)
{
    return *(const uint8_t *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_CRYPTO_PRIVATE_SERVER_PARAMS)->stored_value.blob_val;
}

uint8_t *
UfsrvConfigCryptoPrivateServerParamsAsPtr(void)
{
    return (uint8_t *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_CRYPTO_PRIVATE_SERVER_PARAMS)->stored_value.blob_val;
}

void
UfsrvConfigSetCryptoPrivateServerParams(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_CRYPTO_PRIVATE_SERVER_PARAMS);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_CRYPTO_PRIVATE_SERVER_PARAMS, &item->value, config_source);
}

uint8_t
UfsrvConfigCryptoPublicServerParams(void)
{
    return *(const uint8_t *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_CRYPTO_PUBLIC_SERVER_PARAMS)->stored_value.blob_val;
}

uint8_t *
UfsrvConfigCryptoPublicServerParamsAsPtr(void)
{
    return (uint8_t *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_CRYPTO_PUBLIC_SERVER_PARAMS)->stored_value.blob_val;
}

void
UfsrvConfigSetCryptoPublicServerParams(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_CRYPTO_PUBLIC_SERVER_PARAMS);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_CRYPTO_PUBLIC_SERVER_PARAMS, &item->value, config_source);
}

ec_private_key
UfsrvConfigCryptoPrivateKeyServer(void)
{
    return *(const ec_private_key *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_CRYPTO_PRIVATE_KEY_SERVER)->stored_value.blob_val;
}

ec_private_key *
UfsrvConfigCryptoPrivateKeyServerAsPtr(void)
{
    return (ec_private_key *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_CRYPTO_PRIVATE_KEY_SERVER)->stored_value.blob_val;
}

void
UfsrvConfigSetCryptoPrivateKeyServer(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_CRYPTO_PRIVATE_KEY_SERVER);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_CRYPTO_PRIVATE_KEY_SERVER, &item->value, config_source);
}

ec_public_key
UfsrvConfigCryptoPublicKeyServer(void)
{
    return *(const ec_public_key *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_CRYPTO_PUBLIC_KEY_SERVER)->stored_value.blob_val;
}

ec_public_key *
UfsrvConfigCryptoPublicKeyServerAsPtr(void)
{
    return (ec_public_key *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_CRYPTO_PUBLIC_KEY_SERVER)->stored_value.blob_val;
}

void
UfsrvConfigSetCryptoPublicKeyServer(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_CRYPTO_PUBLIC_KEY_SERVER);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_CRYPTO_PUBLIC_KEY_SERVER, &item->value, config_source);
}

ec_public_key
UfsrvConfigCryptoPublicKeyServerSerialised(void)
{
    return *(const ec_public_key *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_CRYPTO_PUBLIC_KEY_SERVER_SERIALISED)->stored_value.blob_val;
}

ec_public_key *
UfsrvConfigCryptoPublicKeyServerSerialisedAsPtr(void)
{
    return (ec_public_key *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_CRYPTO_PUBLIC_KEY_SERVER_SERIALISED)->stored_value.blob_val;
}

void
UfsrvConfigSetCryptoPublicKeyServerSerialised(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_CRYPTO_PUBLIC_KEY_SERVER_SERIALISED);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_CRYPTO_PUBLIC_KEY_SERVER_SERIALISED, &item->value, config_source);
}

size_t
UfsrvConfigServerKeyid(void)
{
    return (size_t)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_SERVER_KEYID)->stored_value.int_val;
}

void
UfsrvConfigSetServerKeyid(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_SERVER_KEYID);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_SERVER_KEYID, &item->value, config_source);
}

unsigned
UfsrvConfigCryptoInitialised(void)
{
    return (unsigned)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_CRYPTO_INITIALISED)->stored_value.int_val;
}

void
UfsrvConfigSetCryptoInitialised(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_CRYPTO_INITIALISED);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_CRYPTO_INITIALISED, &item->value, config_source);
}

RateLimitApplicationPolicy
UfsrvConfigRatelimitPolicy(void)
{
    return *(const RateLimitApplicationPolicy *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_RATELIMIT_POLICY)->stored_value.blob_val;
}

RateLimitApplicationPolicy *
UfsrvConfigRatelimitPolicyAsPtr(void)
{
    return (RateLimitApplicationPolicy *)UfsrvConfigRuntimeValuesInstance(
        UFSRV_RUNTIME_FIELD_RATELIMIT_POLICY)->stored_value.blob_val;
}

void
UfsrvConfigSetRatelimitPolicy(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigRuntimeStoreInstance(
        UFSRV_RUNTIME_FIELD_RATELIMIT_POLICY);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_RUNTIME_FIELD_RATELIMIT_POLICY, &item->value, config_source);
}

// ── AsString marshallers — accessor per field ─────────────────────────

BufferDescriptor *
UfsrvConfigUfsrvmediaUploadUriAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigMsgqueueAddressAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigMsgqueuePortAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericUnsignedAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigPersistenceBackendAddressAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigPersistenceBackendPortAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericUnsignedAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigCacheBackendAddressUsrmsgAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigCacheBackendPortUsrmsgAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericUnsignedAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigCacheBackendAddressFenceAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigCacheBackendPortFenceAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericUnsignedAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigGeoipServiceAddressAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigGeoipServicePortAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericUnsignedAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigMsgqueueSubAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigMsgqueueSubAsString_MessageQueueBackend_pointer(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigPersistenceBackendAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigPersistenceBackendAsString_PersistenceBackend_pointer(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigUsrmsgCachebackendAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigUsrmsgCachebackendAsString_UserMessageCacheBackend_pointer(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigFenceCachebackendAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigFenceCachebackendAsString_FenceCacheBackend_pointer(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigDbBackendAddressAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigDbBackendPortAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericUnsignedAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigDbBackendUsernameAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigDbBackendPasswordAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigCloudAuthorizationFcmAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigCloudAuthorizationFcmAsString_CloudAuthorizationToken_pointer(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigCloudAuthorizationIntegrityApiAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigCloudAuthorizationIntegrityApiAsString_CloudAuthorizationToken_pointer(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigCryptoPrivateServerParamsAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigCryptoPrivateServerParamsAsString_uint8_t(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigCryptoPublicServerParamsAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigCryptoPublicServerParamsAsString_uint8_t(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigCryptoPrivateKeyServerAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigCryptoPrivateKeyServerAsString_ec_private_key(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigCryptoPublicKeyServerAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigCryptoPublicKeyServerAsString_ec_public_key(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigCryptoPublicKeyServerSerialisedAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigCryptoPublicKeyServerSerialisedAsString_ec_public_key(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigServerKeyidAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericSizeTAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigCryptoInitialisedAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericUnsignedAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigRatelimitPolicyAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return UfsrvConfigRatelimitPolicyAsString_RateLimitApplicationPolicy(field_index, value_ptr, buffer_provided);
}

// Config accessors

int
UfsrvConfigWebsocketMaxFrameSize(void)
{
    return (int)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_WEBSOCKET_MAX_FRAME_SIZE)->stored_value.int_val;
}

void
UfsrvConfigSetWebsocketMaxFrameSize(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_WEBSOCKET_MAX_FRAME_SIZE);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_WEBSOCKET_MAX_FRAME_SIZE, &item->value, config_source);
}

int
UfsrvConfigWebsocketMaxMessageSize(void)
{
    return (int)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_WEBSOCKET_MAX_MESSAGE_SIZE)->stored_value.int_val;
}

void
UfsrvConfigSetWebsocketMaxMessageSize(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_WEBSOCKET_MAX_MESSAGE_SIZE);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_WEBSOCKET_MAX_MESSAGE_SIZE, &item->value, config_source);
}

int
UfsrvConfigWebsocketHeartbeatIntervalSecs(void)
{
    return (int)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_WEBSOCKET_HEARTBEAT_INTERVAL_SECS)->stored_value.int_val;
}

void
UfsrvConfigSetWebsocketHeartbeatIntervalSecs(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_WEBSOCKET_HEARTBEAT_INTERVAL_SECS);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_WEBSOCKET_HEARTBEAT_INTERVAL_SECS, &item->value, config_source);
}

int
UfsrvConfigWebsocketCloseTimeoutSecs(void)
{
    return (int)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_WEBSOCKET_CLOSE_TIMEOUT_SECS)->stored_value.int_val;
}

void
UfsrvConfigSetWebsocketCloseTimeoutSecs(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_WEBSOCKET_CLOSE_TIMEOUT_SECS);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_WEBSOCKET_CLOSE_TIMEOUT_SECS, &item->value, config_source);
}

int
UfsrvConfigWebsocketMaxConnectionsPerSession(void)
{
    return (int)UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_WEBSOCKET_MAX_CONNECTIONS_PER_SESSION)->stored_value.int_val;
}

void
UfsrvConfigSetWebsocketMaxConnectionsPerSession(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_WEBSOCKET_MAX_CONNECTIONS_PER_SESSION);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_WEBSOCKET_MAX_CONNECTIONS_PER_SESSION, &item->value, config_source);
}

char *
UfsrvConfigCloudServiceAccountsGpcFirebaseMessaging(void)
{
    return UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_CLOUD_SERVICE_ACCOUNTS_GPC_FIREBASE_MESSAGING)->stored_value.str_val;
}

void
UfsrvConfigSetCloudServiceAccountsGpcFirebaseMessaging(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_CLOUD_SERVICE_ACCOUNTS_GPC_FIREBASE_MESSAGING);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_CLOUD_SERVICE_ACCOUNTS_GPC_FIREBASE_MESSAGING, &item->value, config_source);
}

char *
UfsrvConfigCloudServiceAccountsGpcIntegrity(void)
{
    return UfsrvConfigConfFileValuesInstance(
        UFSRV_CONFIG_FIELD_CLOUD_SERVICE_ACCOUNTS_GPC_INTEGRITY)->stored_value.str_val;
}

void
UfsrvConfigSetCloudServiceAccountsGpcIntegrity(UfsrvConfigItem *item, UfsrvConfigSource config_source)
{
    UfsrvConfigFieldEntry *_entry = UfsrvConfigConfFileStoreInstance(
        UFSRV_CONFIG_FIELD_CLOUD_SERVICE_ACCOUNTS_GPC_INTEGRITY);
    if (_entry->lifecycle.on_value_available)
        _entry->lifecycle.on_value_available(
            UFSRV_CONFIG_FIELD_CLOUD_SERVICE_ACCOUNTS_GPC_INTEGRITY, &item->value, config_source);
}

// Runtime accessors

// ── AsString marshallers — accessor per field ─────────────────────────

BufferDescriptor *
UfsrvConfigWebsocketMaxFrameSizeAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericIntAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigWebsocketMaxMessageSizeAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericIntAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigWebsocketHeartbeatIntervalSecsAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericIntAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigWebsocketCloseTimeoutSecsAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericIntAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigWebsocketMaxConnectionsPerSessionAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericIntAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigCloudServiceAccountsGpcFirebaseMessagingAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}

BufferDescriptor *
UfsrvConfigCloudServiceAccountsGpcIntegrityAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
    return GenericCharPtrAsString(field_index, value_ptr, buffer_provided);
}
