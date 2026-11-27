/**
 * @file state_command_delivery.h
 * @brief State commands delivered through the state staged out queue.
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

#ifndef UFSRVWEBSOCK_STATE_COMMAND_DELIVERY_H
#define UFSRVWEBSOCK_STATE_COMMAND_DELIVERY_H

#include <stdbool.h>
#include <stddef.h>
#include <ufnetcorelib/ufnet_msgcore/UnfacdService.pb-c.h>
#include <ufnetcorelib/ufnet_core/session/session_type.h>
#include <ufnetcorelib/ufnet_msgcore/session.h>
#include <ufsrv_core/ufsrvresult_type.h>
#include <uflib/collection/collection.h>

/**
 * @brief Collect the fence peers a state command is staged for.
 *
 * Wraps the fence member uid collection with the state policy: a filter that keeps every
 * well-formed member except the originator. No session is materialised here; peers that block the
 * originator are dropped when the record is delivered, exactly like the fence message fan-out.
 *
 * @param fid             Fence the state belongs to.
 * @param originator_uid  User generating the state; excluded from the snapshot.
 * @param[out] out        Collection of plain uids; an empty result with count 0 means every member
 *                        was filtered out, not a backend failure. Release with
 *                        @ref StateCommandFreeFenceTargetUids.
 *
 * @return @c true when the collection ran, @c false on a backend or argument failure.
 */
bool StateCommandCollectFenceTargetUids(unsigned long fid, unsigned long originator_uid, CollectionResult *out);

/**
 * @brief Release a collection produced by @ref StateCommandCollectFenceTargetUids.
 *
 * @param out Collection to release; safe on a zeroed or empty collection.
 */
void StateCommandFreeFenceTargetUids(CollectionResult *out);

/**
 * @brief Stage one state command for every target it was accepted for.
 *
 * The accept thread only stores the envelope against the originator's freshly allocated event id;
 * a job worker relays it per target, retries failures and logs the attempt, exactly like the other
 * staged queues. One record covers the whole snapshot, so the event is queued once however many
 * recipients it has.
 *
 * @param ctx_originator       Session that generated the state; it is the record's @c userid and
 *                             queue uid, and the caller must own its lock.
 * @param command_envelope_ptr Envelope to deliver; packed here, never retained.
 * @param eid                  Originator's event id for this state.
 * @param fid                  Fence the state belongs to, zero when it has no fence. Metadata only:
 *                             the record carries no fence identity.
 * @param uids                 Target snapshot; must be non-empty.
 * @param uids_sz              Number of targets.
 *
 * @return Store result: success once the record is queued, or the store failure. A failed signal
 *         still leaves the record queued for a later drain or restart.
 */
UFSRVResult *StateCommandStageEnvelopeForDelivery(InstanceContextForSession *ctx_originator,
                                                  Envelope *command_envelope_ptr, unsigned long eid,
                                                  unsigned long fid, const unsigned long *uids, size_t uids_sz);

/**
 * @brief Ask the job pool to drain one state out queue.
 *
 * @param uid User whose event the queue belongs to.
 * @param eid Event id of the staged record.
 *
 * @return @c true when the work was queued; the record stays queued otherwise.
 */
bool StateCommandDeliverySignal(unsigned long uid, unsigned long eid);

#endif //UFSRVWEBSOCK_STATE_COMMAND_DELIVERY_H
