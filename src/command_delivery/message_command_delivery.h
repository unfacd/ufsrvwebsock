/**
 * @file message_command_delivery.h
 * @brief message_command_delivery
 *
 * Copyright (C) 2015-2026  unfacd works
 *
 * This file is part of ufsrvwebsock source code.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 */

#ifndef UFSRVWEBSOCK_MESSAGE_COMMAND_DELIVERY_H
#define UFSRVWEBSOCK_MESSAGE_COMMAND_DELIVERY_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <ufnet_core/staged_message_descriptor_type.h>
#include <uflib/collection_descriptor_type.h>
#include <ufnetcorelib/ufnet_msgcore/UnfacdService.pb-c.h>
#include <ufnetcorelib/ufnet_msgcore/fence/fence_event_type.h>
#include <ufnetcorelib/ufnet_msgcore/fence/fence_redis_backend.h>
#include <ufnetcorelib/ufnet_msgcore/session.h>
#include "ufsrv_core/ufsrvresult_type.h"
#include "command_delivery/utils_command_delivery.h"

/**
 * @brief Release a collection produced by @ref MessageCommandCollectFenceTargetUids.
 *
 * @param out Collection to release; safe to call on a zeroed or empty collection.
 */
void MessageCommandFreeFenceTargetUids(CollectionResult *out);

/**
 * @brief Prepared SAY envelope, owned by this delivery module.
 *
 * Controllers only consume it: they build the buffer set once, call
 * @ref PrepareMarshalMessageForMessage to fill it, and hand the envelope to
 * @ref MessageCommandDeliverEnvelopeToUser or @ref MessageCommandDeliverAcceptEcho. The caller
 * owns the backing locals referenced here and must keep them alive until the envelope has been
 * packed.
 */
struct MarshalMessageEnvelopeForMessage {
  Envelope          *envelope;              ///< wire envelope
  UfsrvCommandWire  *ufsrv_command_wire;    ///< envelope payload container
  MessageCommand    *message_command;       ///< SAY command payload
  CommandHeader     *header;                ///< command header shared by envelope and command
  FenceRecord       *fence_record;          ///< single fence record for this command
  FenceRecord       **fence_records;        ///< array holding @ref fence_record
  UserRecord        *user_record_originator;///< originator record supplied by the caller
};
typedef struct MarshalMessageEnvelopeForMessage MarshalMessageEnvelopeForMessage;

/**
 * @brief Fill a prepared message envelope from a received DataMessage.
 *
 * @param envelope_ptr         Prepared descriptor; every backing buffer must already be initialised.
 * @param sesn_ptr             Originator session, used for the originator record.
 * @param fence_id             Fence the command targets.
 * @param fence_msg_expiry     Message expiry to advertise on the fence record.
 * @param fence_event_ptr      Event identity (eid/gid/when), may be NULL.
 * @param data_msg_ptr_orig    Received command whose payload is being repackaged.
 * @param command_type         Command type to advertise.
 * @param command_arg          Command args to advertise (ACCEPTED for the echo, SYNCED for fan-out).
 * @param attach_payload       Attach the message body/originator. The originator echo leaves it off:
 *                             it is an acknowledgement, and a payload would make the client classify
 *                             it as a received message rather than the accepted ACK.
 */
void PrepareMarshalMessageForMessage(MarshalMessageEnvelopeForMessage *envelope_ptr, Session *sesn_ptr,
                                     unsigned long fence_id, time_t fence_msg_expiry,
                                     FenceEvent *fence_event_ptr, DataMessage *data_msg_ptr_orig,
                                     enum _MessageCommand__CommandTypes command_type,
                                     enum _CommandArgs command_arg, bool attach_payload);

/**
 * @brief Send one already-prepared message envelope to a recipient or to the originator.
 *
 * @param ctx_sesn_ptr              Originator context.
 * @param instance_sesn_ptr_target  Recipient, NULL to address the originator's own socket.
 * @param wsm_ptr_received          Received message controlling request/response framing, may be NULL.
 * @param fid                       Fence id carried in the marshalling descriptor.
 * @param command_envelope_ptr      Prepared envelope.
 *
 * @return Explicit delivery result; contention arrives as RESCODE_PROG_WONTLOCK.
 */
UFSRVResult *MessageCommandDeliverEnvelopeToUser(InstanceContextForSession *ctx_sesn_ptr,
                                                 InstanceHolderForSession *instance_sesn_ptr_target,
                                                 WebSocketMessage *wsm_ptr_received, unsigned long fid,
                                                 Envelope *command_envelope_ptr);

/**
 * @brief Emit the originator's own accepted copy of a message command.
 *
 * Runs once per accepted message, on the thread that already owns the originator's session, so the
 * downstream session try-lock short-circuits instead of racing the originator's I/O worker.
 *
 * @param ctx_sesn_ptr          Originator context (caller holds the session lock).
 * @param fence_event_ptr       Event identity of the accepted message.
 * @param data_msg_ptr_received Received command being echoed back.
 * @param wsm_ptr_received      Received message controlling framing, may be NULL.
 * @param fence_id              Fence the command targets.
 * @param fence_msg_expiry      Message expiry to advertise.
 *
 * @return Explicit delivery result for the echo.
 */
UFSRVResult *MessageCommandDeliverAcceptEcho(InstanceContextForSession *ctx_sesn_ptr, FenceEvent *fence_event_ptr,
                                             DataMessage *data_msg_ptr_received, WebSocketMessage *wsm_ptr_received,
                                             unsigned long fence_id, time_t fence_msg_expiry);


UFSRVResult *MessageCommandDeliverToFence(const StagedMessageDescriptor *staged_message_descriptor_ptr,
                                          CollectionDescriptor *delivery_results_out);
bool MessageCommandDeliverySignal(unsigned long fid, unsigned long eid);


/**
 * @brief Record the outcome of one fence out-queue drain attempt in `FENCE_OUT_QUEUE_LOG:<fid>:<eid>`.
 *
 * Best effort: returns false when the entry could not be written, and never changes the delivery
 * outcome or the caller's thread context result. Failed uids are handed over as a
 * `CollectionDescriptor` whose elements are carried by value (`collection_base_offset` is
 * `sizeof(unsigned long)`); NULL or an empty collection means every target was delivered.
 *
 * @code{.c}
 * CollectionDescriptor failed_uids = {
 *     .collection             = AS_COLLECTION_TYPE(retry_uids),
 *     .collection_sz          = failed_sz,
 *     .collection_base_offset = sizeof(unsigned long)
 * };
 *
 * MessageCommandDeliveryLogAttempt(staged_message_descriptor_ptr, &failed_uids,
 *                                  _RESULT_CODE(delivery_res_ptr));
 * @endcode
 *
 * See the definition in message_command_delivery.c for the entry grammar.
 */
bool MessageCommandDeliveryLogAttempt(const StagedMessageDescriptor *staged_message_descriptor_ptr,
                                      const CollectionDescriptor *failed_uids_ptr, int outcome_rescode);

#endif //UFSRVWEBSOCK_MESSAGE_COMMAND_DELIVERY_H
