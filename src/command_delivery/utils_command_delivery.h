/**
 * @file utils_command_delivery.h
 * @brief Helpers shared by the fence and user command delivery modules.
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

#ifndef UFSRVWEBSOCK_UTILS_COMMAND_DELIVERY_H
#define UFSRVWEBSOCK_UTILS_COMMAND_DELIVERY_H

#include <stdbool.h>
#include <stddef.h>
#include <ufsrv_core/cache_backend/redis.h>
#include <ufnetcorelib/ufnet_core/staged_message_descriptor_type.h>
#include <ufnetcorelib/ufnet_msgcore/UnfacdService.pb-c.h>
#include <ufsrv_core/ufsrvresult_type.h>
#include <uflib/collection/collection.h>
#include <uflib/collection_descriptor_type.h>

#define DELIVERY_LOG_UID_DIGITS		(24U)		///< headroom per failed uid in a log entry, in decimal digits
#define DELIVERY_LOG_ENTRY_EXTRA	(256U)		///< headroom for the fixed fields of a log entry
#define DELIVERY_LOG_TTL_SECONDS	(604800U)	///< retention window, refreshed on every append

/**
 * @brief Per-recipient delivery state collected for one fan-out attempt.
 *
 * Elements sit in a @c CollectionDescriptor indexed exactly like a staged record's @c uids:
 * element @c i describes @c uids[i]. They are carried by value, so @c collection_base_offset is
 * @c sizeof(StagedUserDeliveryResult). The delivery path fills @c result_type and @c rescode for
 * every recipient, which is what lets a caller pinpoint which user an outcome belongs to.
 */
typedef struct StagedUserDeliveryResult {
  unsigned long uid;
  int           result_type;
  int           rescode;
} StagedUserDeliveryResult;

/**
 * @brief Read one uid from a collection whose elements are carried by value.
 *
 * @param collection_ptr Collection of uids, must not be NULL.
 * @param index          Zero-based element index.
 *
 * @return The uid stored at @p index.
 */
static inline unsigned long
sCollectionUidAt(const CollectionDescriptor *collection_ptr, size_t index)
{
  if (collection_ptr->collection_base_offset == 0) return (unsigned long)(uintptr_t)collection_ptr->collection[index];

  return *(const unsigned long *)((const char *)collection_ptr->collection
                                  + (index * collection_ptr->collection_base_offset));
}

/**
 * @brief Address one element of a @ref StagedUserDeliveryResult collection.
 */
static inline StagedUserDeliveryResult *
sStagedDeliveryResultAt(CollectionDescriptor *collection_ptr, size_t index)
{
  size_t stride = collection_ptr->collection_base_offset ? collection_ptr->collection_base_offset
                                                         : sizeof(StagedUserDeliveryResult);

  return (StagedUserDeliveryResult *)((char *)collection_ptr->collection + (index * stride));
}

/**
 * @brief Store one recipient's delivery state, ignoring out-of-range writes.
 */
static inline void
sStagedDeliveryResultStore(CollectionDescriptor *collection_ptr, size_t index, unsigned long uid,
                           int result_type, int rescode)
{
  if (collection_ptr == NULL || collection_ptr->collection == NULL || index >= collection_ptr->collection_sz) return;

  StagedUserDeliveryResult *result_ptr = sStagedDeliveryResultAt(collection_ptr, index);

  result_ptr->uid = uid;
  result_ptr->result_type = result_type;
  result_ptr->rescode = rescode;
}

/**
 * @brief Copy a downstream @ref UFSRVResult into one recipient's delivery state.
 */
static inline void
sStagedDeliveryResultRecord(CollectionDescriptor *collection_ptr, size_t index, unsigned long uid,
                            const UFSRVResult *res_ptr)
{
  if (collection_ptr == NULL || collection_ptr->collection == NULL || index >= collection_ptr->collection_sz) return;

  if (res_ptr == NULL) {
    sStagedDeliveryResultStore(collection_ptr, index, uid, RESULT_TYPE_ERR, RESCODE_PROG_NULL_POINTER);

    return;
  }

  sStagedDeliveryResultStore(collection_ptr, index, uid, (int)_RESULT_TYPE(res_ptr), (int)_RESULT_CODE(res_ptr));
}

/**
 * @brief Whether a recipient's delivery state was never attempted.
 */
static inline bool
sStagedDeliveryResultPending(const StagedUserDeliveryResult *result_ptr)
{
  return result_ptr->result_type == RESULT_TYPE_STAGED;
}

/**
 * @brief Whether a recipient's delivery state represents a failed delivery.
 */
static inline bool
sStagedDeliveryResultFailed(const StagedUserDeliveryResult *result_ptr)
{
  return result_ptr->result_type == RESULT_TYPE_ERR || result_ptr->result_type == RESULT_TYPE_BACKENDERR
      || result_ptr->result_type == RESULT_TYPE_PROTOCOLERR || result_ptr->result_type == RESULT_TYPE_IOERR;
}

/**
 * @brief Release a heap buffer at the end of the enclosing scope.
 *
 * For use with @c __attribute__((cleanup)); receives the address of the pointer variable.
 *
 * @param ptr Address of the buffer pointer; a NULL pointer or a NULL buffer is ignored.
 */
void DeliveryCleanupFree(void *ptr);

/**
 * @brief Release a redis reply at the end of the enclosing scope.
 *
 * @param reply_ptr Address of the reply pointer; a NULL pointer or a NULL reply is ignored.
 */
void DeliveryCleanupReply(redisReply **reply_ptr);

/**
 * @brief Release an unpacked command envelope at the end of the enclosing scope.
 *
 * For use with @c __attribute__((cleanup)); receives the address of the envelope pointer.
 *
 * @param envelope_ptr Address of the envelope pointer; a NULL pointer or a NULL envelope is ignored.
 */
void DeliveryCleanupEnvelope(Envelope **envelope_ptr);

/**
 * @brief Serialise and compress a command envelope for a staged out queue.
 *
 * @param command_envelope_ptr Envelope to serialise; read only.
 * @param[out] payload_ptr_out Compressed bytes, a fresh allocation owned by the caller.
 * @param[out] payload_sz_out  Size of the compressed bytes.
 * @param[out] raw_sz_out      Size before compression, needed to decompress.
 *
 * @return @c true when the envelope was serialised and compressed; nothing is allocated otherwise.
 */
bool DeliveryPackEnvelope(Envelope *command_envelope_ptr, void **payload_ptr_out, size_t *payload_sz_out,
                          size_t *raw_sz_out);

/**
 * @brief Decompress and parse a command envelope taken from a staged out queue.
 *
 * @param payload_ptr Compressed bytes as stored.
 * @param payload_sz  Size of @p payload_ptr as stored.
 * @param raw_sz      Size before compression, carried by the record.
 *
 * @return The unpacked envelope, owned by the caller and released with @ref DeliveryCleanupEnvelope,
 *         or NULL when the bytes do not decompress to @p raw_sz or do not parse.
 */
Envelope *DeliveryUnpackEnvelope(const void *payload_ptr, size_t payload_sz, size_t raw_sz);

/**
 * @brief Append one formatted field to a delivery-log entry.
 *
 * @param buffer_ptr Destination entry buffer.
 * @param buffer_sz  Total size of @p buffer_ptr.
 * @param offset     Write position, maintained by the caller across calls.
 * @param format     printf-style field format.
 * @param ...        Arguments for @p format.
 *
 * @return Bytes written, or zero when the field did not fit or formatting failed.
 */
size_t DeliveryLogAppend(char *buffer_ptr, size_t buffer_sz, size_t offset, const char *format, ...);

/**
 * @brief Append one drain-attempt entry to a per-event delivery log.
 *
 * Both queues share the entry grammar; only the key family, the backend and the command templates
 * differ, so those are passed in. Best effort: a backend failure is logged and reported as
 * @c false, never propagated.
 *
 * @param pers_ptr        Cache backend holding the log.
 * @param add_command     RPUSH template for the log key.
 * @param expire_command  EXPIRE template for the log key.
 * @param queue_name      Queue family used in log messages.
 * @param key_a           First key part.
 * @param key_b           Second key part.
 * @param attempt         Zero-based drain attempt.
 * @param targets_sz      Target count of the drained record (@c m).
 * @param failed_uids_ptr Failed uids by value; NULL or empty means every target was delivered.
 * @param outcome_rescode Rescode observed for this attempt.
 *
 * @return @c true when the entry was appended.
 */
bool DeliveryLogAttemptAppend(PersistenceBackend *pers_ptr, const char *add_command, const char *expire_command,
                              const char *queue_name, unsigned long key_a, unsigned long key_b, unsigned attempt,
                              size_t targets_sz, const CollectionDescriptor *failed_uids_ptr, int outcome_rescode);

/**
 * @brief Classify a drained record's per-recipient outcomes into the uids that must be retried.
 *
 * Slots still @c RESULT_TYPE_STAGED were never reached; slots flagged failed carry their own
 * reason. When no per-recipient state exists and the call itself failed, the whole batch is marked
 * for retry.
 *
 * @param delivery_results  Per-recipient collection, may be NULL.
 * @param delivery_res_ptr  Result returned by the deliverer, may be NULL.
 * @param targets_sz        Target count of the drained record.
 * @param[out] retry_uids   Buffer receiving the failed uids.
 * @param[out] failed_sz_out      Number of uids written to @p retry_uids.
 * @param[out] batch_retry_out    True when the record's own uid snapshot must be retried instead.
 * @param[out] failure_rescode_out First attributed failure rescode, or the deliverer's rescode.
 */
void DeliveryCollectRetryUids(CollectionDescriptor *delivery_results, const UFSRVResult *delivery_res_ptr,
                              size_t targets_sz, unsigned long *retry_uids, size_t *failed_sz_out,
                              bool *batch_retry_out, int *failure_rescode_out);

/**
 * @brief Release the allocations an extracted queue record hands to its caller.
 *
 * @param staged_message_descriptor_ptr Drained record; safe on a zeroed or partially parsed one.
 */
void DeliveryFreeExtractedRecord(const StagedMessageDescriptor *staged_message_descriptor_ptr);

#endif //UFSRVWEBSOCK_UTILS_COMMAND_DELIVERY_H
