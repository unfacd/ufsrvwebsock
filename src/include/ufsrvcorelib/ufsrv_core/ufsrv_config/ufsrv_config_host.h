/*

Copyright (c) 2015-2026 unfacd works

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 */

#ifndef UFSRVWEBSOCK_UFSRV_CONFIG_HOST_H
#define UFSRVWEBSOCK_UFSRV_CONFIG_HOST_H

#include <stdbool.h>
#include <uflib/buffer_descriptor/buffer_descriptor_type.h>

void BootstrapRuntimeConfigStore();
void BootstrapConfigurationFileConfiStore();
void LoadConfigurationFileConfiStoreDefaults(bool is_forced_reload);
void LoadRuntimeConfigStoreDefaults(bool is_forced_reload);
int JsonFormatConfigurationFileConfigStore(BufferDescriptor *buffer_descriptor_provided);
int JsonFormatRuntimeConfigStore(BufferDescriptor *buffer_descriptor_provided);

#endif //UFSRVWEBSOCK_UFSRV_CONFIG_HOST_H
