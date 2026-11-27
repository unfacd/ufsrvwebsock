#!/usr/bin/env bash
#
# Generates version.c with a fresh compilation timestamp, builder id, and the
# UFSRV_MAJOR/MINOR/PATCH version. Invoked by a
# custom target on every build (see the root CMakeLists.txt). Writes version.c
# into the current working directory, which CMake sets to the build directory.
#
#Clean up windows CRLF: sed -i 's/\r$//' version.c.sh
#
UFSRV_CLASS=""
HOST="`hostname | sed -e 's/\([a-zA-Z0-9\-]*\).*/\1/'`"
USER=`whoami`
HIM=$USER@$HOST
WHEN=`date | \
 awk '{ \
       if (NF==6) \
          { print $1 " "  $2 " " $3 " "  $6 " at " $4 " " $5 } \
       else \
         { print $1 " "  $2 " " $3 " " $7 " at " $4 " " $5 " " $6 }}'`

cat >version.c <<!END!
/**
 * Copyright (C) 2015-2021 unfacd works
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <version.h>

#define STRINGISE_NX(A) #A
#define STRINGISE(A) STRINGISE_NX(A)

static const char *t_compiled = "$WHEN";
static const char *u_compiled = "$HIM";
static const char *ufsrv_version = STRINGISE(UFSRV_MAJOR)"."STRINGISE(UFSRV_MINOR)"."STRINGISE(UFSRV_PATCH)"."STRINGISE(UFSRV_INTERNAL);

const char *UfsrvCompiledTimestamp(void) { return t_compiled; }
const char *UfsrvCompiledBy(void)        { return u_compiled; }
const char *UfsrvVersion(void)           { return ufsrv_version; }
const char *UfsrvVersionMajor(void)      { return STRINGISE(UFSRV_MAJOR); }
const char *UfsrvVersionMinor(void)      { return STRINGISE(UFSRV_MINOR); }
const char *UfsrvVersionPatch(void)      { return STRINGISE(UFSRV_PATCH); }
const char *UfsrvVersionInternal(void)      { return STRINGISE(UFSRV_INTERNAL); }
!END!
