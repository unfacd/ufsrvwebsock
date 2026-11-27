/**
 * @file fence_command_delivery.h
 * @brief Fence attribute changes delivered through the fence staged out queue.
 *
 * Copyright (C) 2015-2026  unfacd works
 *
 * This file is part of ufsrvwebsock source code.
 * Created by ayman on 25/11/2026.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 */

#ifndef UFSRVWEBSOCK_FENCE_COMMAND_DELIVERY_H
#define UFSRVWEBSOCK_FENCE_COMMAND_DELIVERY_H

#include <stdbool.h>
#include <stddef.h>
#include <ufnetcorelib/ufnet_msgcore/UnfacdService.pb-c.h>
#include <ufnetcorelib/ufnet_msgcore/fence/fence.h>
#include <ufnetcorelib/ufnet_msgcore/session.h>
#include <ufsrv_core/ufsrvresult_type.h>
#include <uflib/collection/collection.h>

/**
 * @brief Collect the fence members a fence command is staged for.
 *
 * Wraps the fence member uid collection with the fence-command policy: every well-formed member
 * except the uids the caller excludes. No session is materialised here, so a target that is offline
 * or connected to another instance still receives the command when it next resolves.
 *
 * @param fid              Fence the command belongs to.
 * @param exclude_uids     Uids that must not be in the snapshot; may be NULL when
 *                         @p exclude_uids_sz is zero, in which case every member is kept.
 * @param exclude_uids_sz  Number of uids in @p exclude_uids.
 * @param[out] out         Collection of plain uids. An empty result with count 0 means every member
 *                         was filtered out, not a backend failure. Release with
 *                         @ref FenceCommandFreeTargetUids.
 *
 * @return @c true when the collection ran, @c false on a backend or argument failure.
 */
bool FenceCommandCollectTargetUids(unsigned long fid, const unsigned long *exclude_uids, size_t exclude_uids_sz,
                                   CollectionResult *out);

/**
 * @brief Release a collection produced by @ref FenceCommandCollectTargetUids.
 *
 * @param out Collection to release; safe on a zeroed or empty collection.
 */
void FenceCommandFreeTargetUids(CollectionResult *out);

/**
 * @brief Stage one fence command for an explicit list of target uids.
 *
 * The accept thread only stores the packed envelope against the fence event id; a job worker relays
 * it per target, retries the ones that failed and logs the attempt. One record covers the whole
 * list, so an event is queued once however many recipients it has.
 *
 * @param ctx_originator       Session that issued the command; it is the record's @c userid and the
 *                             session the callback is replayed against.
 * @param fid                  Fence the command belongs to; must be non-zero.
 * @param eid                  Fence event id the command was registered under; must be non-zero,
 *                             because it is part of the queue key and the record.
 * @param gid                  Global event id, zero when the command has no cross-instance event.
 * @param command_envelope_ptr Envelope to deliver; packed here, never retained.
 * @param uids                 Target snapshot; must be non-empty.
 * @param uids_sz              Number of targets.
 *
 * @return Store result: success once the record is queued, or the failure that prevented staging. A
 *         failed signal still leaves the record queued for a later drain or restart.
 */
UFSRVResult *FenceCommandStageForTargets(InstanceContextForSession *ctx_originator, unsigned long fid,
                                         unsigned long eid, unsigned long gid, Envelope *command_envelope_ptr,
                                         const unsigned long *uids, size_t uids_sz);

/**
 * @brief Stage one fence command for every fence member but the excluded uids.
 *
 * The fence identity and event id are taken from the envelope header, so callers only supply the
 * recipients that must not receive the queued copy (typically the originator, whose own copy is an
 * immediate acknowledgement, and any user a per-user branch already addressed).
 *
 * @param ctx_originator       Session that issued the command; see @ref FenceCommandStageForTargets.
 * @param f_ptr                Fence the command belongs to; read for its id only.
 * @param command_envelope_ptr Envelope to deliver; packed here, never retained.
 * @param exclude_uids         Uids already delivered to; see @ref FenceCommandCollectTargetUids.
 * @param exclude_uids_sz      Number of uids in @p exclude_uids.
 *
 * @return Store result, or success with nothing queued when the snapshot is empty.
 */
UFSRVResult *FenceCommandStageForFenceMembers(InstanceContextForSession *ctx_originator, Fence *f_ptr,
                                              Envelope *command_envelope_ptr, const unsigned long *exclude_uids,
                                              size_t exclude_uids_sz);

/**
 * @brief Ask the job pool to drain one fence out queue.
 *
 * @param fid Fence whose event queue is drained.
 * @param eid Fence event id of the staged record.
 *
 * @return @c true when the work was queued; the record stays queued otherwise.
 */
bool FenceCommandDeliverySignal(unsigned long fid, unsigned long eid);

#endif //UFSRVWEBSOCK_FENCE_COMMAND_DELIVERY_H
