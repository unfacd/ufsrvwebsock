/**
 * @file ufsrv_linked_libs.c
 * @brief Full versions of the linked uf** libraries, rendered once.
 *
 * Copyright (C) 2015-2026  unfacd works
 *
 * This file is part of the ufsrv server source code.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "ufsrv_linked_libs.h"

#include <uflib/version.h>
#include <ufnetcorelib/version.h>
#include <ufsrvcorelib/version.h>
#include <ufsrvrxlib/version.h>

#include <stdlib.h>
#include <syslog.h>

/**
 * One linked library.  The accessor is the library's own generated version.c
 * function — never the compile-time macros, so what is reported is the code that
 * was actually linked, not what this translation unit was compiled against.
 *
 * The string is used verbatim: a library that folds its internal-gate counter
 * into the version (uflib, ufnetcorelib, ufsrvcorelib) reports four parts, one
 * that does not (ufsrvrxlib, ufsrvscriptlib, ufsrvclilib) reports three.
 *
 * ufsrvscriptlib is linked transitively (libufsrvscriptlib.a) but installs no
 * headers, so it cannot be named here until its install ships include/.
 */
typedef struct UfsrvLinkedLib
{
  const char *name;
  const char *(*version)(void);
} UfsrvLinkedLib;

static const UfsrvLinkedLib sLinkedLibs[] =
{
  { "uflib",        UfsrvUfLibVersion   },
  { "ufnetcorelib", UfNetCoreLibVersion },
  { "ufsrvcorelib", UfsrvCoreLibVersion },
  { "ufsrvrxlib",   UfsrvRxLibVersion   },
};

#define LINKED_LIBS_COUNT (sizeof sLinkedLibs / sizeof sLinkedLibs[0])

static BufferDescriptor *
sLiveBuffer(BufferDescriptor *buffer_provided)
{
  if (buffer_provided)	return buffer_provided;

  buffer_provided = calloc(1, sizeof(BufferDescriptor));
  if (!buffer_provided)	return NULL;

  BufferDescriptorInit(buffer_provided, 256);
  if (!buffer_provided->data) {
    free(buffer_provided);
    return NULL;
  }

  return buffer_provided;
}

BufferDescriptor *
UfsrvLinkedLibsDescribe(BufferDescriptor *buffer_provided)
{
  BufferDescriptor *out = sLiveBuffer(buffer_provided);

  if (!out)	return NULL;

  BufferDescriptorAppendFormatted(out, "{");

  for (size_t i = 0; i < LINKED_LIBS_COUNT; i++) {
    const UfsrvLinkedLib *lib = &sLinkedLibs[i];
    const char *version = lib->version();

    BufferDescriptorAppendFormatted(out, "%s\"%s\":\"%s\"",
        i > 0 ? "," : "", lib->name, version ? version : "");
  }

  BufferDescriptorAppendFormatted(out, "}");

  return out;
}

void
UfsrvLinkedLibsLog(void)
{
  BufferDescriptor *linked_libs = UfsrvLinkedLibsDescribe(NULL);

  if (!linked_libs)	return;

  syslog(LOG_INFO, "%s", linked_libs->data ? linked_libs->data : "(unavailable)");

  BufferDescriptorRelease(linked_libs);
  free(linked_libs);
}

BufferDescriptor *
UfsrvConsoleLinkedLibsOnGet(ScriptHandlerContext *ctx)
{
	BufferDescriptor *buffer_provided = ctx->buffer;
  return UfsrvLinkedLibsDescribe(buffer_provided);
}
