/**
 * @file ufsrv_linked_libs.h
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

#ifndef UFSRV_LINKED_LIBS_H
#define UFSRV_LINKED_LIBS_H

#include <ufsrvcorelib/ufsrv_console_script/ufsrv_console_script_type.h>

#include <uflib/buffer_descriptor/buffer_descriptor.h>

/**
 * @brief Render the full version of every linked uf** library into a buffer.
 *
 * One rendering, three consumers: the /internals/linked_libs console handler
 * returns it, the startup banner logs the same bytes to syslog, and anything
 * that wants it on stdout prints the buffer unchanged.
 *
 * The output is a single JSON object with one member per library:
 *
 *   {"uflib":"4.0.1.12","ufnetcorelib":"...","ufsrvcorelib":"...","ufsrvrxlib":"..."}
 *
 * The four-part form is MAJOR.MINOR.PATCH.INTERNAL.  A library that exposes no
 * internal-gate accessor reports the three-part release.
 *
 * @param buffer_provided Caller buffer to append to, or NULL to return a fresh
 *                        one the caller owns and releases with
 *                        BufferDescriptorRelease() (and free()).
 * @return The buffer, or NULL when allocation fails.
 */
BufferDescriptor *UfsrvLinkedLibsDescribe(BufferDescriptor *buffer_provided);

/**
 * @brief Log UfsrvLinkedLibsDescribe() output to syslog at LOG_INFO.
 */
void UfsrvLinkedLibsLog(void);

/**
 * @brief Console handler for GET /internals/linked_libs.
 *
 * Satisfies the console on-get interface, BufferDescriptor *(*)(BufferDescriptor *).
 */
BufferDescriptor *UfsrvConsoleLinkedLibsOnGet(ScriptHandlerContext *ctx);

#endif /* UFSRV_LINKED_LIBS_H */
