/**
 * @file main_protocols.h
 * @brief main_protocols
 *
 * Copyright (C) 2015-2026  unfacd works
 *
 * This file is part of ufsrvwebsock source code.
 * Created by ayman on 23/11/2026.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 */

#ifndef UFSRVWEBSOCK_MAIN_PROTOCOLS_H
#define UFSRVWEBSOCK_MAIN_PROTOCOLS_H

#include <ufsrv_core/protocol/protocol_type.h>

#include "protocol_index_enum.h"

const Protocol * UfsrvWebsockProtocol(void);
size_t UfsrvProtocolRegistrySize();

#endif //UFSRVWEBSOCK_MAIN_PROTOCOLS_H
