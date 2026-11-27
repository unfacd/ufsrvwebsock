/**
 * @file user_command_delivery.h
 * @brief Client-bound command delivery through the user staged out queue.
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

#ifndef UFSRVWEBSOCK_USER_COMMAND_DELIVERY_H
#define UFSRVWEBSOCK_USER_COMMAND_DELIVERY_H

#include <stdbool.h>
#include <ufnetcorelib/ufnet_msgcore/UnfacdService.pb-c.h>
#include <ufnetcorelib/ufnet_msgcore/session.h>
#include <ufsrv_core/ufsrvresult_type.h>

/**
 * @brief Stage a client-bound command envelope on the recipient's user out queue.
 *
 * The accept thread only stores the envelope; a job worker replays it per recipient, retries
 * failures and logs the attempt, exactly like the fence message path. The record is keyed by the
 * recipient's uid and their freshly allocated eid, and the envelope carries that same eid.
 *
 * @param instance_sesn_ptr_sender Session that accepted the command; recorded as the sender.
 * @param uid_target               Recipient user whose eid keys the queue.
 * @param command_envelope_ptr     Envelope to deliver; packed here, never retained.
 * @param eid                      Recipient's event id for this network activity.
 * @param fid                      Fence the command belongs to, zero when it has no fence.
 *
 * @return Store result: success once the record is queued.
 */
UFSRVResult *UserCommandStageEnvelopeForDelivery(InstanceHolderForSession *instance_sesn_ptr_sender,
                                                 unsigned long uid_target, Envelope *command_envelope_ptr,
                                                 unsigned long eid, unsigned long fid);

/**
 * @brief Ask the job pool to drain one user out queue.
 *
 * @param uid Target user owning the queue.
 * @param eid Event id of the staged record.
 *
 * @return @c true when the work was queued; the record stays queued otherwise.
 */
bool UserCommandDeliverySignal(unsigned long uid, unsigned long eid);

#endif //UFSRVWEBSOCK_USER_COMMAND_DELIVERY_H
