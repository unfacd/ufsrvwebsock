/**
 * @file ufsrv_version_self.h
 * @brief This server binary's own version, rendered for the command console.
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

#ifndef UFSRV_VERSION_SELF_H
#define UFSRV_VERSION_SELF_H

#include <ufsrvcorelib/ufsrv_console_script/ufsrv_console_script_type.h>

#include <uflib/buffer_descriptor/buffer_descriptor.h>

/**
 * @brief Render this binary's own version into a buffer.
 *
 * The version comes from the binary's generated version.c (UfsrvVersion()), so
 * it describes the executable that is running, not a library it linked.
 *
 *   {"name":"ufsrvapi","version":"3.9.16.74","compiled":"...","by":"..."}
 *
 * @param buffer_provided Caller buffer to append to, or NULL to return a fresh
 *                        one the caller owns and releases with
 *                        BufferDescriptorRelease() (and free()).
 * @return The buffer, or NULL when allocation fails.
 */
BufferDescriptor *UfsrvSelfVersionDescribe(BufferDescriptor *buffer_provided);

/**
 * @brief Console handler for GET /internals/version/self.
 *
 * Satisfies the console on-get interface, BufferDescriptor *(*)(BufferDescriptor *).
 */
BufferDescriptor *UfsrvConsoleVersionSelfOnGet(ScriptHandlerContext *ctx);

#endif /* UFSRV_VERSION_SELF_H */
