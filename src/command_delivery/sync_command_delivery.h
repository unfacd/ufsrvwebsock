/**
 * @file sync_command_delivery.h
 * @brief Guardian sync copies delivered through the sync staged out queue.
 *
 * Copyright (C) 2015-2026  unfacd works
 *
 * This file is part of ufsrvwebsock source code.
 * Created by ayman on 17/11/2026.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 */

#ifndef UFSRVWEBSOCK_SYNC_COMMAND_DELIVERY_H
#define UFSRVWEBSOCK_SYNC_COMMAND_DELIVERY_H

#include <stdbool.h>
#include <ufnetcorelib/ufnet_msgcore/UnfacdService.pb-c.h>
#include <ufnetcorelib/ufnet_msgcore/session.h>
#include <ufsrv_core/ufsrvresult_type.h>

/**
 * @brief Stage the guardian's copy of an accepted message event.
 *
 * The accepting thread allocates the event id for the originating user, stamps it on the sync
 * envelope and stores one record; a job worker later resolves the guardian and delivers the copy.
 * The caller must own the originator's session lock, because the event id is recorded on it.
 *
 * @param ctx_sesn_ptr    Originator context; its session is the record's @c userid and queue uid.
 * @param dm_ptr_received Received command the copy is built from; read only.
 *
 * @return Store result: success once the record is queued, a failure when the event id could not be
 *         allocated or the record could not be written. A failed signal still leaves the record
 *         queued for a later drain or restart.
 *
 * @dynamic_memory packs the envelope and releases the packed buffer before returning.
 * @locks the caller's session lock is assumed; nothing else is taken here.
 */
UFSRVResult *SyncCommandStageForGuardian(InstanceContextForSession *ctx_sesn_ptr, DataMessage *dm_ptr_received);

/**
 * @brief Ask the job pool to drain one sync out queue.
 *
 * @param uid User whose event the queue belongs to.
 * @param eid Event id of the staged record.
 *
 * @return @c true when the work was queued; the record stays queued otherwise.
 */
bool SyncCommandDeliverySignal(unsigned long uid, unsigned long eid);

#endif //UFSRVWEBSOCK_SYNC_COMMAND_DELIVERY_H
