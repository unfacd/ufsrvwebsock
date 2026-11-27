/**
 * @file ufsrv_config_generic_callbacks.c
 * @brief Generic lifecycle callbacks for ufsrv_config — inflators, setters,
 *        reload stub, and teardowns for INT, STR, STRUCT, and PTR value types.
 *
 * Each value type has two variants: CONFIG domain (targeting
 * UfsrvConfigConfFileValuesInstance) and RUNTIME domain (targeting
 * UfsrvConfigRuntimeValuesInstance).  Callbacks are wired into field tables
 * by value_type and domain via the cold catalogue.
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
#include <time.h>
#include <sys/time.h>
#include <uflib/utils_hex.h>
#include <uflib/buffer_descriptor/buffer_descriptor.h>
#include <ufsrvcorelib/ufsrv_core/ufsrv_config/ufsrv_config.h>
#include "ufsrv_config_generic_callbacks_priv.h"

void GenericIntInflator(int field_index, const UfsrvConfigValue *value_ptr, bool is_deflated_first)
{
  (void)field_index;
  (void)value_ptr;
  (void)is_deflated_first; /* scalar — no allocation */
}

void GenericBoolInflator(int field_index, const UfsrvConfigValue *value_ptr, bool is_deflated_first)
{
  (void)field_index;
  (void)value_ptr;
  (void)is_deflated_first; /* scalar — no allocation */
}

void GenericStrInflator(int field_index, const UfsrvConfigValue *value_ptr, bool is_deflated_first)
{
  if (is_deflated_first) {
    if (UfsrvConfigConfFileValuesInstance(field_index)->stored_value.str_val) {
      free(UfsrvConfigConfFileValuesInstance(field_index)->stored_value.str_val);
      UfsrvConfigConfFileValuesInstance(field_index)->stored_value.str_val = NULL;
    }
  }
  if (value_ptr->type_allocation_sz > 0)
    UfsrvConfigConfFileValuesInstance(field_index)->stored_value.str_val = malloc(value_ptr->type_allocation_sz);
}

void GenericStructInflator(int field_index, const UfsrvConfigValue *value_ptr, bool is_deflated_first)
{
  if (is_deflated_first) {
    if (UfsrvConfigConfFileValuesInstance(field_index)->stored_value.blob_val) {
      free(UfsrvConfigConfFileValuesInstance(field_index)->stored_value.blob_val);
      UfsrvConfigConfFileValuesInstance(field_index)->stored_value.blob_val = NULL;
    }
  }
  /* calloc (not malloc): an inflated-but-never-set STRUCT/blob slot must read
   * as a well-defined empty value (e.g. a zeroed DistinctArray with
   * stored_value_idx == NULL), not uninitialised garbage that AsString/AsPtr
   * would dereference. */
  if (value_ptr->type_allocation_sz > 0)
    UfsrvConfigConfFileValuesInstance(field_index)->stored_value.blob_val = calloc(1, value_ptr->type_allocation_sz);
}

void GenericPtrInflator(int field_index, const UfsrvConfigValue *value_ptr, bool is_deflated_first)
{
  (void)field_index;
  (void)value_ptr;
  (void)is_deflated_first; /* copy-by-ref — no allocation */
}

void GenericIntInflatorRuntime(int field_index, const UfsrvConfigValue *value_ptr, bool is_deflated_first)
{
  (void)field_index;
  (void)value_ptr;
  (void)is_deflated_first; /* scalar — no allocation */
}

void GenericBoolInflatorRuntime(int field_index, const UfsrvConfigValue *value_ptr, bool is_deflated_first)
{
  (void)field_index;
  (void)value_ptr;
  (void)is_deflated_first; /* scalar — no allocation */
}

void GenericStrInflatorRuntime(int field_index, const UfsrvConfigValue *value_ptr, bool is_deflated_first)
{
  if (is_deflated_first) {
    if (UfsrvConfigRuntimeValuesInstance(field_index)->stored_value.str_val) {
      free(UfsrvConfigRuntimeValuesInstance(field_index)->stored_value.str_val);
      UfsrvConfigRuntimeValuesInstance(field_index)->stored_value.str_val = NULL;
    }
  }
  if (value_ptr->type_allocation_sz > 0)
    UfsrvConfigRuntimeValuesInstance(field_index)->stored_value.str_val = malloc(value_ptr->type_allocation_sz);
}

void GenericStructInflatorRuntime(int field_index, const UfsrvConfigValue *value_ptr, bool is_deflated_first)
{
  if (is_deflated_first) {
    if (UfsrvConfigRuntimeValuesInstance(field_index)->stored_value.blob_val) {
      free(UfsrvConfigRuntimeValuesInstance(field_index)->stored_value.blob_val);
      UfsrvConfigRuntimeValuesInstance(field_index)->stored_value.blob_val = NULL;
    }
  }
  /* calloc (not malloc): same reason as GenericStructInflator — an
   * inflated-but-never-set slot must be zeroed, not garbage. */
  if (value_ptr->type_allocation_sz > 0)
    UfsrvConfigRuntimeValuesInstance(field_index)->stored_value.blob_val = calloc(1, value_ptr->type_allocation_sz);
}

void GenericPtrInflatorRuntime(int field_index, const UfsrvConfigValue *value_ptr, bool is_deflated_first)
{
  (void)field_index;
  (void)value_ptr;
  (void)is_deflated_first; /* copy-by-ref — no allocation */
}

void GenericIntSetter(int field_index, const UfsrvConfigValue *value, UfsrvConfigSource source)
{
  UfsrvConfigConfFileValuesInstance(field_index)->stored_value.int_val = value->stored_value.int_val;
  UfsrvConfigConfFileValuesInstance(field_index)->source = source;
}

void GenericBoolSetter(int field_index, const UfsrvConfigValue *value, UfsrvConfigSource source)
{
  UfsrvConfigConfFileValuesInstance(field_index)->stored_value.int_val = value->stored_value.int_val;
  UfsrvConfigConfFileValuesInstance(field_index)->source = source;
}

void GenericStrSetter(int field_index, const UfsrvConfigValue *value, UfsrvConfigSource source)
{
  if (IS_PRESENT(value->stored_value.str_val) && value->type_allocation_sz > 0) {
    UfsrvConfigValue *_slot = UfsrvConfigConfFileValuesInstance(field_index);
    assert(value->type_allocation_sz <= _slot->type_allocation_sz);
    strncpy(_slot->stored_value.str_val, value->stored_value.str_val, value->type_allocation_sz);
    _slot->stored_value.str_val[value->type_allocation_sz - 1] = '\0';
    _slot->source = source;
  }
}

void GenericStructSetter(int field_index, const UfsrvConfigValue *value, UfsrvConfigSource source)
{
  if (IS_PRESENT(value->stored_value.blob_val) && value->type_allocation_sz > 0) {
    UfsrvConfigValue *_slot = UfsrvConfigConfFileValuesInstance(field_index);
    assert(value->type_allocation_sz == _slot->type_allocation_sz);
    memcpy(_slot->stored_value.blob_val, value->stored_value.blob_val, value->type_allocation_sz);
    _slot->source = source;
  }
}

void GenericPtrSetter(int field_index, const UfsrvConfigValue *value, UfsrvConfigSource source)
{
  UfsrvConfigConfFileValuesInstance(field_index)->stored_value.ptr_val = value->stored_value.ptr_val;
  UfsrvConfigConfFileValuesInstance(field_index)->source = source;
}

void GenericIntSetterRuntime(int field_index, const UfsrvConfigValue *value, UfsrvConfigSource source)
{
  UfsrvConfigRuntimeValuesInstance(field_index)->stored_value.int_val = value->stored_value.int_val;
  UfsrvConfigRuntimeValuesInstance(field_index)->source = source;
}

void GenericBoolSetterRuntime(int field_index, const UfsrvConfigValue *value, UfsrvConfigSource source)
{
  UfsrvConfigRuntimeValuesInstance(field_index)->stored_value.int_val = value->stored_value.int_val;
  UfsrvConfigRuntimeValuesInstance(field_index)->source = source;
}

void GenericStrSetterRuntime(int field_index, const UfsrvConfigValue *value, UfsrvConfigSource source)
{
  if (IS_PRESENT(value->stored_value.str_val) && value->type_allocation_sz > 0) {
    UfsrvConfigValue *_slot = UfsrvConfigRuntimeValuesInstance(field_index);
    assert(value->type_allocation_sz <= _slot->type_allocation_sz);
    strncpy(_slot->stored_value.str_val, value->stored_value.str_val, value->type_allocation_sz);
    _slot->stored_value.str_val[value->type_allocation_sz - 1] = '\0';
    _slot->source = source;
  }
}

void GenericStructSetterRuntime(int field_index, const UfsrvConfigValue *value, UfsrvConfigSource source)
{
  if (IS_PRESENT(value->stored_value.blob_val) && value->type_allocation_sz > 0) {
    UfsrvConfigValue *_slot = UfsrvConfigRuntimeValuesInstance(field_index);
    memcpy(_slot->stored_value.blob_val, value->stored_value.blob_val, value->type_allocation_sz);
    _slot->source = source;
  }
}

void GenericPtrSetterRuntime(int field_index, const UfsrvConfigValue *value, UfsrvConfigSource source)
{
  UfsrvConfigRuntimeValuesInstance(field_index)->stored_value.ptr_val = value->stored_value.ptr_val;
  UfsrvConfigRuntimeValuesInstance(field_index)->source = source;
}

void GenericReloadedStub(int field_index, const UfsrvConfigValue *value)
{
  (void)field_index;
  (void)value;
}

void GenericStrTeardown(int field_index, UfsrvConfigValue *value_ptr)
{
  (void)value_ptr;
  free(UfsrvConfigConfFileValuesInstance(field_index)->stored_value.str_val);
  UfsrvConfigConfFileValuesInstance(field_index)->stored_value.str_val = NULL;
}

void GenericStructTeardown(int field_index, UfsrvConfigValue *value_ptr)
{
  (void)value_ptr;
  free(UfsrvConfigConfFileValuesInstance(field_index)->stored_value.blob_val);
  UfsrvConfigConfFileValuesInstance(field_index)->stored_value.blob_val = NULL;
}

void GenericStrTeardownRuntime(int field_index, UfsrvConfigValue *value_ptr)
{
  (void)value_ptr;
  free(UfsrvConfigRuntimeValuesInstance(field_index)->stored_value.str_val);
  UfsrvConfigRuntimeValuesInstance(field_index)->stored_value.str_val = NULL;
}

void GenericStructTeardownRuntime(int field_index, UfsrvConfigValue *value_ptr)
{
  (void)value_ptr;
  free(UfsrvConfigRuntimeValuesInstance(field_index)->stored_value.blob_val);
  UfsrvConfigRuntimeValuesInstance(field_index)->stored_value.blob_val = NULL;
}

/* Render payload into buffer_provided (or a freshly allocated BufferDescriptor
 * when buffer_provided == NULL).  Returns the descriptor on success, NULL when
 * the caller buffer is unusable or allocation fails. */
BufferDescriptor *
UfsrvConfigAsStringCommit(const char *payload, BufferDescriptor *buffer_provided)
{
  size_t n = payload ? strlen(payload) : 0;

  if (buffer_provided) {
    if (!buffer_provided->data || buffer_provided->size_max == 0) return NULL;
    size_t cap  = buffer_provided->size_max;
    size_t copy = (n + 1 <= cap) ? n : (cap ? cap - 1 : 0);
    if (cap > 0) {
      if (payload && copy > 0) memcpy(buffer_provided->data, payload, copy);
      buffer_provided->data[copy] = '\0';
      buffer_provided->size       = copy;
    }
    return buffer_provided;
  }

  BufferDescriptor *bd = malloc(sizeof(*bd) + n + 1);
  if (!bd) return NULL;
  bd->data     = (char *)(bd + 1);
  bd->size     = n;
  bd->size_max = n + 1;
  if (payload) memcpy(bd->data, payload, n + 1);
  else bd->data[0] = '\0';
  return bd;
}


PUBLIC_API BufferDescriptor *
GenericIntAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
  (void)field_index;
  if (!buffer_provided || !value_ptr) return NULL;

  if (BufferDescriptorAppendFormatted(buffer_provided, "%lld", (long long)value_ptr->stored_value.int_val) != 0) {
    BufferDescriptorRelease(buffer_provided);
    return NULL;
  }
  return buffer_provided;
}

PUBLIC_API BufferDescriptor *
GenericDoubleAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
  (void)field_index;
  if (!buffer_provided || !value_ptr) return NULL;

  if (BufferDescriptorAppendFormatted(buffer_provided, "%f", value_ptr->stored_value.double_value) != 0) {
    BufferDescriptorRelease(buffer_provided);
    return NULL;
  }
  return buffer_provided;
}

PUBLIC_API BufferDescriptor *
GenericBoolAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
  (void)field_index;
  if (!buffer_provided || !value_ptr) return NULL;

  if (BufferDescriptorAppendFormatted(buffer_provided, "%s", value_ptr->stored_value.int_val ? "true" : "false") != 0) {
    BufferDescriptorRelease(buffer_provided);
    return NULL;
  }
  return buffer_provided;
}

PUBLIC_API BufferDescriptor *
GenericCharPtrAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
  (void)field_index;
  if (!buffer_provided || !value_ptr) return NULL;

  if (BufferDescriptorAppendFormatted(buffer_provided, "%s", value_ptr->stored_value.str_val) != 0) {
    BufferDescriptorRelease(buffer_provided);
    return NULL;
  }
  return buffer_provided;

}

PUBLIC_API BufferDescriptor *
GenericUnsignedAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
  (void)field_index;
  if (!buffer_provided || !value_ptr) return NULL;

  if (BufferDescriptorAppendFormatted(buffer_provided, "%llu", (unsigned long long)value_ptr->stored_value.int_val) != 0) {
    BufferDescriptorRelease(buffer_provided);
    return NULL;
  }
  return buffer_provided;
}

PUBLIC_API BufferDescriptor *
GenericUnsignedCharAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
  (void)field_index;
  if (!buffer_provided || !value_ptr) return NULL;

  size_t n = value_ptr->type_allocation_sz;
  if (!value_ptr->stored_value.blob_val || n == 0) {
    BufferDescriptorRelease(buffer_provided);
    return NULL;
  }

  char *hex = malloc(2 * n + 1);
  if (!hex) {
    BufferDescriptorRelease(buffer_provided);
    return NULL;
  }

  bin2hex(value_ptr->stored_value.blob_val, n, hex);

  if (BufferDescriptorAppendFormatted(buffer_provided, "%s", hex) != 0) {
    free(hex);
    BufferDescriptorRelease(buffer_provided);
    return NULL;
  }

  free(hex);
  return buffer_provided;
}

PUBLIC_API BufferDescriptor *
GenericSizeTAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
  (void)field_index;
  if (!buffer_provided || !value_ptr) return NULL;

  if (BufferDescriptorAppendFormatted(buffer_provided, "%zu", (size_t)value_ptr->stored_value.int_val) != 0) {
    BufferDescriptorRelease(buffer_provided);
    return NULL;
  }
  return buffer_provided;
}

PUBLIC_API BufferDescriptor *
GenericTimeTAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
  (void)field_index;
  if (!buffer_provided || !value_ptr) return NULL;

  char date[32];
  time_t t = (time_t)value_ptr->stored_value.int_val;
  struct tm tm_buf;

  localtime_r(&t, &tm_buf);
  strftime(date, sizeof(date), "%y%m%d-%H:%M:%S", &tm_buf);

  if (BufferDescriptorAppendFormatted(buffer_provided, "%lld, %s", (long long)value_ptr->stored_value.int_val, date) != 0) {
    BufferDescriptorRelease(buffer_provided);
    return NULL;
  }
  return buffer_provided;
}

PUBLIC_API BufferDescriptor *
GenericPidTAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
  (void)field_index;
  if (!buffer_provided || !value_ptr) return NULL;

  if (BufferDescriptorAppendFormatted(buffer_provided, "%lld", (long long)value_ptr->stored_value.int_val) != 0) {
    BufferDescriptorRelease(buffer_provided);
    return NULL;
  }
  return buffer_provided;
}

PUBLIC_API BufferDescriptor *
GenericTimevalAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
  (void)field_index;
  if (!buffer_provided || !value_ptr) return NULL;

  struct timeval *tv = value_ptr->stored_value.blob_val;
  if (!tv) {
    BufferDescriptorRelease(buffer_provided);
    return NULL;
  }

  if (BufferDescriptorAppendFormatted(buffer_provided, "%ld.%06ld", (long)tv->tv_sec, (long)tv->tv_usec) != 0) {
    BufferDescriptorRelease(buffer_provided);
    return NULL;
  }
  return buffer_provided;
}

PUBLIC_API BufferDescriptor *
GenericTimevalPtrAsString(int field_index, UfsrvConfigValue *value_ptr, BufferDescriptor *buffer_provided)
{
  (void)field_index;
  if (!buffer_provided || !value_ptr) return NULL;

  char buf[64];
  struct timeval *tv = value_ptr->stored_value.ptr_val;
  if (!tv) {
    BufferDescriptorRelease(buffer_provided);
    return NULL;
  }

  if (BufferDescriptorAppendFormatted(buffer_provided, "%ld.%06ld", (long)tv->tv_sec, (long)tv->tv_usec) != 0) {
    BufferDescriptorRelease(buffer_provided);
    return NULL;
  }
  return buffer_provided;

}