/**
 * @file ufsrvapi_console_handlers.c
 * @brief ufsrvapi command-console handlers + registration
 *
 * Copyright (C) 2015-2026  unfacd works
 *
 * This file is part of ufsrvapi source code.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <ufnetcorelib/nportredird.h>
#include <ufnetcorelib/ufnet_msgcore/fence/fence.h>

#include <ufsrvcorelib/ufsrv_console_script/ufsrv_console_script.h>
#include <ufsrvcorelib/ufsrv_console_script/ufsrv_console_script_type.h>

#include "ufsrv_linked_libs.h"
#include "ufsrv_version_self.h"

#include <uflib/buffer_descriptor/buffer_descriptor.h>
#include <uflib/logger/logger.h>
#include <uflib/adt/adt_hashtable.h>
#include <uflib/utils_json.h>
#include <ufsrvcorelib/ufsrv_core/ufsrv_logger.h>
#include <ufsrvcorelib/ufsrv_core/system/utils_jemalloc.h>
#include <ufsrv_core/delegator_sessions/command/session_delegator_command_handler.h>
#include "ufsrv_core/ufsrv_config/ufsrv_config_host.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//TODO: replace these stubs with real handlers that reflect live server state.

static BufferDescriptor *
sStubGet(BufferDescriptor *buffer_provided)
{
	if (!buffer_provided) {
		buffer_provided = calloc(1, sizeof(BufferDescriptor));
		if (!buffer_provided) {
			return NULL;
		}
		BufferDescriptorInit(buffer_provided, 256);
	}

	BufferDescriptorAppendFormatted(buffer_provided, "{\"status\":\"ok\"}");

	return buffer_provided;
}

#define DEFINE_GET_STUB(fn) \
	BufferDescriptor *fn(ScriptHandlerContext *ctx) \
	{ \
		return sStubGet(ctx->buffer); \
	}

//Serialise a config store as its native JSON document. The host formatters are
//the same ones used by the --dump-config-json diagnostic path (see main.c).
//They append to the (already initialised) console buffer and grow it as needed;
//on error nothing is written back to the peer.
static BufferDescriptor *
sConfigStoreGet(ScriptHandlerContext *ctx, int (*formatter)(BufferDescriptor *))
{
	BufferDescriptor *buffer_provided = ctx->buffer;

	if (!buffer_provided) {
		buffer_provided = calloc(1, sizeof(BufferDescriptor));
		if (!buffer_provided) {
			return NULL;
		}
		BufferDescriptorInit(buffer_provided, 1024);
		if (!buffer_provided->data) {
			free(buffer_provided);
			return NULL;
		}
	}

	if (formatter(buffer_provided) != 0) {
		if (buffer_provided != ctx->buffer) {
			BufferDescriptorRelease(buffer_provided);
			free(buffer_provided);
		}
		return NULL;
	}

	return buffer_provided;
}

BufferDescriptor *
UfsrvConsoleInternalsConfigRuntimeOnGet(ScriptHandlerContext *ctx)
{
	return sConfigStoreGet(ctx, JsonFormatRuntimeConfigStore);
}

BufferDescriptor *
UfsrvConsoleInternalsConfigConfigFileOnGet(ScriptHandlerContext *ctx)
{
	return sConfigStoreGet(ctx, JsonFormatConfigurationFileConfigStore);
}

DEFINE_GET_STUB(UfsrvConsoleInternalsThreadsOnGet)
DEFINE_GET_STUB(UfsrvConsoleInternalsRecyclersSessionsOnGet)

BufferDescriptor *
UfsrvConsoleInternalsLoggerOnGet(ScriptHandlerContext *ctx)
{
	BufferDescriptor *buffer_provided = ctx->buffer;
	if (!buffer_provided) {
		buffer_provided = calloc(1, sizeof(BufferDescriptor));
		if (!buffer_provided) {
			return NULL;
		}
		BufferDescriptorInit(buffer_provided, 512);
	}

	return UfLoggerDescribeConfiguration(UfsrvLogger(), buffer_provided);
}

static BufferDescriptor *
sHashtableBuffer(BufferDescriptor *buffer_provided)
{
	if (!buffer_provided) {
		buffer_provided = calloc(1, sizeof(BufferDescriptor));
		if (!buffer_provided) return NULL;
		BufferDescriptorInit(buffer_provided, 256);
	}

	return buffer_provided;
}

static BufferDescriptor *
sHashtableUnavailable(BufferDescriptor *buffer_provided, const char *table_name, const char *reason_ptr)
{
	buffer_provided = sHashtableBuffer(buffer_provided);
	if (!buffer_provided) return NULL;

	BufferDescriptorAppendFormatted(buffer_provided, "{\"table\":\"%s\",\"error\":\"%s\"}\n", table_name, reason_ptr);

	return buffer_provided;
}

BufferDescriptor *
UfsrvConsoleInternalsHashtablesSessionsOnGet(ScriptHandlerContext *ctx)
{
	SessionsDelegator *delegator = GetUfsrvSessionsDelegator();
	if (!delegator) return sHashtableUnavailable(ctx->buffer, "SessionsHashTable", "session delegator not published");

	HashTable *table_ptr = SESNDELEGATE_SESSIONCACHE(delegator);
	if (!HASHTABLE_NAME(table_ptr)) return sHashtableUnavailable(ctx->buffer, "SessionsHashTable", "table not initialised");

	return DescribeHashTable(table_ptr, ctx->buffer);
}

static bool
sFenceIdFromStem(const char *stem, unsigned long *fence_id_out)
{
	char         *end_ptr;
	unsigned long fence_id;

	if (!stem || !*stem || !isdigit((unsigned char)*stem)) return false;

	errno    = 0;
	fence_id = strtoul(stem, &end_ptr, 10);
	if (errno != 0 || *end_ptr != '\0') return false;

	*fence_id_out = fence_id;

	return true;
}

static void
sJsonString(BufferDescriptor *buffer_provided, const char *value_ptr)
{
	if (!IS_STR_LOADED(value_ptr)) {
		BufferDescriptorAppendFormatted(buffer_provided, "null");
		return;
	}

	JsonEscapeString(buffer_provided, value_ptr);
}

static BufferDescriptor *
sFenceRecordByKey(BufferDescriptor *buffer_provided, HashTable *table_ptr, void *key_ptr)
{
	if (HashTable_RdLock(table_ptr, 1) != 0)
		return sHashtableUnavailable(buffer_provided, HASHTABLE_NAME(table_ptr), "busy");

	void *holder = HashLookup(table_ptr, key_ptr, true);

	BufferDescriptor *out = holder
	    ? DescribeFence(FenceOffInstanceHolder(holder), buffer_provided)
	    : sHashtableUnavailable(buffer_provided, HASHTABLE_NAME(table_ptr), "no such fence");

	HashTable_UnLock(table_ptr);

	return out;
}

static HashTable *
sFenceTableByName(const char *name_ptr, size_t name_len)
{
	HashTable *tables[] = {FenceRegistryIdHashTableProvide(), FenceRegistryCanonicalNameHashTableProvide()};

	for (size_t i = 0; i < sizeof(tables) / sizeof(tables[0]); i++) {
		const char *table_name = HASHTABLE_NAME(tables[i]);

		if (table_name && strlen(table_name) == name_len && !strncmp(table_name, name_ptr, name_len)) return tables[i];
	}

	return NULL;
}

BufferDescriptor *
UfsrvConsoleInternalsHashtablesFencesOnGet(ScriptHandlerContext *ctx)
{
	HashTable  *id_ptr        = FenceRegistryIdHashTableProvide();
	HashTable  *canonical_ptr = FenceRegistryCanonicalNameHashTableProvide();
	const char *stem          = ctx->stemmed_path;
	size_t      name_len;

	if (!id_ptr || !HASHTABLE_NAME(id_ptr))
		return sHashtableUnavailable(ctx->buffer, "FenceRegistry", "fence registry not initialised");
	if (!canonical_ptr || !HASHTABLE_NAME(canonical_ptr))
		return sHashtableUnavailable(ctx->buffer, "FenceRegistryCanonicalName", "fence registry not initialised");

	BufferDescriptor *buffer_provided = sHashtableBuffer(ctx->buffer);
	if (!buffer_provided) return NULL;

	if (!stem || !*stem) {
		BufferDescriptorAppendFormatted(buffer_provided, "{\"tables\":[");
		sJsonString(buffer_provided, HASHTABLE_NAME(id_ptr));
		BufferDescriptorAppendFormatted(buffer_provided, ",");
		sJsonString(buffer_provided, HASHTABLE_NAME(canonical_ptr));
		BufferDescriptorAppendFormatted(buffer_provided, "]}\n");

		return buffer_provided;
	}

	const char *slash = strchr(stem, '/');
	name_len          = slash ? (size_t)(slash - stem) : strlen(stem);

	HashTable *table_ptr = sFenceTableByName(stem, name_len);
	if (!table_ptr) return sHashtableUnavailable(buffer_provided, "fences", "unknown fence table");

	if (!slash || !*(slash + 1)) return DescribeHashTable(table_ptr, buffer_provided);

	const char *key_ptr = slash + 1;
	if (strchr(key_ptr, '/'))
		return sHashtableUnavailable(buffer_provided, HASHTABLE_NAME(table_ptr), "expected <table>/<key>");

	if (table_ptr == id_ptr) {
		unsigned long fence_id;
		if (!sFenceIdFromStem(key_ptr, &fence_id))
			return sHashtableUnavailable(buffer_provided, HASHTABLE_NAME(table_ptr), "expected numeric fence id");

		return sFenceRecordByKey(buffer_provided, table_ptr, &fence_id);
	}

	return sFenceRecordByKey(buffer_provided, table_ptr, (void *)key_ptr);
}

BufferDescriptor *
UfsrvConsoleInternalsHashtablesCookiesOnGet(ScriptHandlerContext *ctx)
{
	SessionsDelegator *delegator = GetUfsrvSessionsDelegator();
	if (!delegator) return sHashtableUnavailable(ctx->buffer, "CookiesHashTable", "session delegator not published");

	HashTable *table_ptr = SESNDELEGATE_COOKIECACHE(delegator);
	if (!HASHTABLE_NAME(table_ptr)) return sHashtableUnavailable(ctx->buffer, "CookiesHashTable", "table not initialised");

	return DescribeHashTable(table_ptr, ctx->buffer);
}

BufferDescriptor *
UfsrvConsoleInternalsHashtablesUsernameOnGet(ScriptHandlerContext *ctx)
{
	SessionsDelegator *delegator = GetUfsrvSessionsDelegator();
	if (!delegator) return sHashtableUnavailable(ctx->buffer, "UsernamesHashtable", "session delegator not published");

	HashTable *table_ptr = SESNDELEGATE_USERNAMECACHE(delegator);
	if (!HASHTABLE_NAME(table_ptr)) return sHashtableUnavailable(ctx->buffer, "UsernamesHashtable", "table not initialised");

	return DescribeHashTable(table_ptr, ctx->buffer);
}

//Register the ufsrvapi command-console hierarchy with the dynamic dispatch.
//Intermediate group nodes (/internals, /internals/config, /internals/hashtables,
///internals/recyclers) are materialised automatically by the library.
void
UfsrvConsoleRegisterHandlers(void)
{
	UfsrvConsoleScriptHandler h = {0};

	h.on_get = UfsrvConsoleVersionSelfOnGet;
	UfsrvConsoleScriptRegister("/internals/version/self", "server binary version", &h);

	//same rendering the startup banner logs; see misc/ufsrv_linked_libs.c
	h.on_get = UfsrvConsoleLinkedLibsOnGet;
	UfsrvConsoleScriptRegister("/internals/version/linked_libs", "linked uf** library versions", &h);

	h.on_get = GetMethodInternalsMemoryJemalloc;
	h.methods = UFSRV_CONSOLE_METHOD_GET;
	UfsrvConsoleScriptRegister("/internals/memory/jemalloc", "jemalloc stats", &h);

	h.on_get = UfsrvConsoleInternalsLoggerOnGet;
	UfsrvConsoleScriptRegister("/internals/logger", "logger configuration", &h);

	h.on_get = UfsrvConsoleInternalsConfigRuntimeOnGet;
	UfsrvConsoleScriptRegister("/internals/config/runtime", "runtime config", &h);

	h.on_get = UfsrvConsoleInternalsConfigConfigFileOnGet;
	UfsrvConsoleScriptRegister("/internals/config/config_file", "config file", &h);

	h.on_get = UfsrvConsoleInternalsThreadsOnGet;
	UfsrvConsoleScriptRegister("/internals/threads", "thread info", &h);

	h.on_get = UfsrvConsoleInternalsHashtablesSessionsOnGet;
	UfsrvConsoleScriptRegister("/internals/hashtables/sessions", "session hashtable", &h);

	UfsrvConsoleScriptHandler fence_handler = {0};
	fence_handler.on_get = UfsrvConsoleInternalsHashtablesFencesOnGet;
	fence_handler.methods = UFSRV_CONSOLE_METHOD_GET;
	fence_handler.options.is_stemmed = true;
	UfsrvConsoleScriptRegister("/internals/hashtables/fences",
	    "fence registry hashtable; append a fence id or a canonical name", &fence_handler);

	h.on_get = UfsrvConsoleInternalsHashtablesCookiesOnGet;
	UfsrvConsoleScriptRegister("/internals/hashtables/cookies", "cookie hashtable", &h);

	h.on_get = UfsrvConsoleInternalsHashtablesUsernameOnGet;
	UfsrvConsoleScriptRegister("/internals/hashtables/username", "username hashtable", &h);

	h.on_get = UfsrvConsoleInternalsRecyclersSessionsOnGet;
	UfsrvConsoleScriptRegister("/internals/recyclers/sessions", "session recycler", &h);

	UfsrvConsoleScriptRegister("/internals/delegator", "Session Delegator signaling", SessionDelegatorCommandProvideHandler(&h));

}

#include <uflib/recycler_v2/recycler_v2.h>

/* Per-type on_get needs a distinct function pointer per type name (the
 * console handler carries no context), so we bind the ListRecyclerTypes()
 * names into a static registry and expose a fixed pool of wrappers. */
#define UFSRV_CONSOLE_RECYCLER_MAX_TYPES 32

static char sRecyclerTypeNames[UFSRV_CONSOLE_RECYCLER_MAX_TYPES][256];

static BufferDescriptor *
sRecyclerDescribeAll(ScriptHandlerContext *ctx)
{
	return DescribeRecycler(NULL, ctx->buffer);
}

static BufferDescriptor *
sRecyclerDescribeNth(BufferDescriptor *buffer, size_t idx)
{
	return DescribeRecycler(sRecyclerTypeNames[idx], buffer);
}

#define DEFINE_RECYCLER_DESCRIBE(n) \
	static BufferDescriptor *sRecyclerDescribe##n(ScriptHandlerContext *ctx) \
	{ \
		return sRecyclerDescribeNth(ctx->buffer, n); \
	}

DEFINE_RECYCLER_DESCRIBE(0)
DEFINE_RECYCLER_DESCRIBE(1)
DEFINE_RECYCLER_DESCRIBE(2)
DEFINE_RECYCLER_DESCRIBE(3)
DEFINE_RECYCLER_DESCRIBE(4)
DEFINE_RECYCLER_DESCRIBE(5)
DEFINE_RECYCLER_DESCRIBE(6)
DEFINE_RECYCLER_DESCRIBE(7)
DEFINE_RECYCLER_DESCRIBE(8)
DEFINE_RECYCLER_DESCRIBE(9)
DEFINE_RECYCLER_DESCRIBE(10)
DEFINE_RECYCLER_DESCRIBE(11)
DEFINE_RECYCLER_DESCRIBE(12)
DEFINE_RECYCLER_DESCRIBE(13)
DEFINE_RECYCLER_DESCRIBE(14)
DEFINE_RECYCLER_DESCRIBE(15)
DEFINE_RECYCLER_DESCRIBE(16)
DEFINE_RECYCLER_DESCRIBE(17)
DEFINE_RECYCLER_DESCRIBE(18)
DEFINE_RECYCLER_DESCRIBE(19)
DEFINE_RECYCLER_DESCRIBE(20)
DEFINE_RECYCLER_DESCRIBE(21)
DEFINE_RECYCLER_DESCRIBE(22)
DEFINE_RECYCLER_DESCRIBE(23)
DEFINE_RECYCLER_DESCRIBE(24)
DEFINE_RECYCLER_DESCRIBE(25)
DEFINE_RECYCLER_DESCRIBE(26)
DEFINE_RECYCLER_DESCRIBE(27)
DEFINE_RECYCLER_DESCRIBE(28)
DEFINE_RECYCLER_DESCRIBE(29)
DEFINE_RECYCLER_DESCRIBE(30)
DEFINE_RECYCLER_DESCRIBE(31)

static BufferDescriptor *(*const sRecyclerDescribers[UFSRV_CONSOLE_RECYCLER_MAX_TYPES])(ScriptHandlerContext *) = {
	sRecyclerDescribe0,
	sRecyclerDescribe1,
	sRecyclerDescribe2,
	sRecyclerDescribe3,
	sRecyclerDescribe4,
	sRecyclerDescribe5,
	sRecyclerDescribe6,
	sRecyclerDescribe7,
	sRecyclerDescribe8,
	sRecyclerDescribe9,
	sRecyclerDescribe10,
	sRecyclerDescribe11,
	sRecyclerDescribe12,
	sRecyclerDescribe13,
	sRecyclerDescribe14,
	sRecyclerDescribe15,
	sRecyclerDescribe16,
	sRecyclerDescribe17,
	sRecyclerDescribe18,
	sRecyclerDescribe19,
	sRecyclerDescribe20,
	sRecyclerDescribe21,
	sRecyclerDescribe22,
	sRecyclerDescribe23,
	sRecyclerDescribe24,
	sRecyclerDescribe25,
	sRecyclerDescribe26,
	sRecyclerDescribe27,
	sRecyclerDescribe28,
	sRecyclerDescribe29,
	sRecyclerDescribe30,
	sRecyclerDescribe31
};

void
UfsrvConsoleRegisterRecyclerHandlers(void)
{
	UfsrvConsoleScriptHandler h = {0};
	CollectionDescriptor *types;
	size_t i, n;

	/* Group node: GET /internals/recycler describes every pool at once. */
	h.on_get = sRecyclerDescribeAll;
	h.methods = UFSRV_CONSOLE_METHOD_GET;
	UfsrvConsoleScriptRegister("/internals/recycler", "recycler type pools", &h);

	types = ListRecyclerTypes();
	if (!types) {
		return;
	}

	n = types->collection_sz;
	if (n > UFSRV_CONSOLE_RECYCLER_MAX_TYPES) {
		n = UFSRV_CONSOLE_RECYCLER_MAX_TYPES;
	}

	for (i = 0; i < n; i++) {
		const char *name = (const char *)types->collection[i];
		char path[256];
		char desc[256];

		/* Copy into the static registry — `types` is freed below and the
		 * per-type on_get reads from this registry at dispatch time. */
		snprintf(sRecyclerTypeNames[i], sizeof(sRecyclerTypeNames[i]), "%s", name);
		snprintf(path, sizeof(path), "/internals/recycler/%s", name);
		snprintf(desc, sizeof(desc), "type %s", name);

		h.on_get = sRecyclerDescribers[i];
		h.methods = UFSRV_CONSOLE_METHOD_GET;
		UfsrvConsoleScriptRegister(path, desc, &h);
	}

	free(types);
}