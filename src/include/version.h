/**
 * Copyright (C) 2015-2026 unfacd works
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

#ifndef UFSRV_VERSION_H
#define UFSRV_VERSION_H

#define UFSRV_MAJOR 5
#define UFSRV_MINOR 0
#define UFSRV_PATCH 1
#define UFSRV_INTERNAL 0

const char *UfsrvCompiledTimestamp(void) __attribute__((const));
const char *UfsrvCompiledBy(void)        __attribute__((const));
const char *UfsrvVersion(void)           __attribute__((const));
const char *UfsrvVersionMajor(void)      __attribute__((const));
const char *UfsrvVersionMinor(void)      __attribute__((const));
const char *UfsrvVersionPatch(void)      __attribute__((const));


#endif //UFSRV_VERSION_H