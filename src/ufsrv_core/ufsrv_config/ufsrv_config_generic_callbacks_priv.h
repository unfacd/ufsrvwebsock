/**
 * @file ufsrv_config_generic_callbacks_priv.h
 * @brief Declarations for generic lifecycle callbacks — inflators, setters,
 *        reload stub, and teardowns for all value types in both CONFIG and
 *        RUNTIME domains.
 *
 * This header is self-contained — it includes ufsrv_config_value_type.h
 * so that UfsrvConfigValue and UfsrvConfigSource are resolved without
 * relying on include order.
 *
 * @note This is a PRIVATE header — these functions are internal to
 *       ufsrv_config.  It is co-located with ufsrv_config_generic_callbacks.c
 *       in src/ufsrv_core/ufsrv_config/, not in the public include/ path.
 *
 * Copyright (C) 2015-2026 unfacd works
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef UFSRV_CONFIG_GENERIC_CALLBACKS_PRIV_H
#define UFSRV_CONFIG_GENERIC_CALLBACKS_PRIV_H

#include <ufsrvcorelib/ufsrvcorelib_defs.h>
#include <ufsrvcorelib/ufsrv_core/ufsrv_config/ufsrv_config_value_type.h>
#include <uflib/main_types.h>

/* ── Inflators — CONFIG domain */
void GenericIntInflator(int field_index, const UfsrvConfigValue *value_ptr, bool is_deflated_first);
void GenericBoolInflator(int field_index, const UfsrvConfigValue *value_ptr, bool is_deflated_first);
void GenericStrInflator(int field_index, const UfsrvConfigValue *value_ptr, bool is_deflated_first);
void GenericStructInflator(int field_index, const UfsrvConfigValue *value_ptr, bool is_deflated_first);
void GenericPtrInflator(int field_index, const UfsrvConfigValue *value_ptr, bool is_deflated_first);

/* ── Setters — CONFIG domain */
void GenericIntSetter(int field_index, const UfsrvConfigValue *value, UfsrvConfigSource source);
void GenericBoolSetter(int field_index, const UfsrvConfigValue *value, UfsrvConfigSource source);
void GenericStrSetter(int field_index, const UfsrvConfigValue *value, UfsrvConfigSource source);
void GenericStructSetter(int field_index, const UfsrvConfigValue *value, UfsrvConfigSource source);
void GenericPtrSetter(int field_index, const UfsrvConfigValue *value, UfsrvConfigSource source);

/* ── Inflators, Setters — RUNTIME domain */
void GenericIntInflatorRuntime(int field_index, const UfsrvConfigValue *value_ptr, bool is_deflated_first);
void GenericBoolInflatorRuntime(int field_index, const UfsrvConfigValue *value_ptr, bool is_deflated_first);

/**
 * This function will copy string upto AND including '\0' terminator. Therefore user MUST
 * set UfsrvConfigValue.type_allocation_sz to total string buffer size, including '\0' terminator.
 * @param field_index of type enum UfsrvConfigField
 * @param value Pre-constructed value payload, which must include UfsrvConfigValue.type_allocation_sz where relevant.
 * @param source as per UfsrvConfigSource
 */
void GenericStrInflatorRuntime(int field_index, const UfsrvConfigValue *value_ptr, bool is_deflated_first);
void GenericStructInflatorRuntime(int field_index, const UfsrvConfigValue *value_ptr, bool is_deflated_first);
void GenericPtrInflatorRuntime(int field_index, const UfsrvConfigValue *value_ptr, bool is_deflated_first);
void GenericIntSetterRuntime(int field_index, const UfsrvConfigValue *value, UfsrvConfigSource source);
void GenericBoolSetterRuntime(int field_index, const UfsrvConfigValue *value, UfsrvConfigSource source);

/**
 * This function will copy string upto AND including '\0' terminator. Therefore user MUST
 * set UfsrvConfigValue.type_allocation_sz to total string buffer size, including '\0' terminator.
 * @param field_index of type enum UfsrvConfigField
 * @param value Pre-constructed value payload, which must include UfsrvConfigValue.type_allocation_sz where relevant.
 * @param source as per UfsrvConfigSource
 */
void GenericStrSetterRuntime(int field_index, const UfsrvConfigValue *value, UfsrvConfigSource source);
void GenericStructSetterRuntime(int field_index, const UfsrvConfigValue *value, UfsrvConfigSource source);
void GenericPtrSetterRuntime(int field_index, const UfsrvConfigValue *value, UfsrvConfigSource source);

/* ── Reload stub */
void GenericReloadedStub(int field_index, const UfsrvConfigValue *value);

/* ── Teardowns — CONFIG domain */
void GenericStrTeardown(int field_index, UfsrvConfigValue *value_ptr);
void GenericStructTeardown(int field_index, UfsrvConfigValue *value_ptr);
/* ── Teardowns — RUNTIME domain */
void GenericStrTeardownRuntime(int field_index, UfsrvConfigValue *value_ptr);
void GenericStructTeardownRuntime(int field_index, UfsrvConfigValue *value_ptr);


/* ── AsString marshallers — standard scalar c_lang_types */
PUBLIC_API BufferDescriptor * GenericIntAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided);
PUBLIC_API BufferDescriptor * GenericDoubleAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided);
PUBLIC_API BufferDescriptor * GenericBoolAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided);
PUBLIC_API BufferDescriptor * GenericCharPtrAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided);
PUBLIC_API BufferDescriptor * GenericUnsignedAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided);
PUBLIC_API BufferDescriptor * GenericUnsignedCharAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided);
PUBLIC_API BufferDescriptor * GenericSizeTAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided);
PUBLIC_API BufferDescriptor * GenericTimeTAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided);
PUBLIC_API BufferDescriptor * GenericPidTAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided);
PUBLIC_API BufferDescriptor * GenericTimevalAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided);
PUBLIC_API BufferDescriptor * GenericTimevalPtrAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided);

/* ── AsString render helper — shared by generic + stub marshallers */
BufferDescriptor * UfsrvConfigAsStringCommit(const char *payload, BufferDescriptor *buffer_provided);

#endif /* UFSRV_CONFIG_GENERIC_CALLBACKS_PRIV_H */
