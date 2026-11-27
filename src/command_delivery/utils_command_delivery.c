/**
 * @file utils_command_delivery.c
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

#ifdef HAVE_CONFIG_UFSRVWEBSOCK_H
#include "config_ufsrvwebsock.h"
#endif

#include "main.h"
#include "command_delivery/utils_command_delivery.h"
#include <uflib/utils.h>
#include <uflib/utils_time.h>
#include <uflib/lzf/lzf.h>
#include <stdarg.h>
#include <stdint.h>

/**
 * @brief Release a heap buffer at the end of the enclosing scope.
 *
 * @param ptr Address of the buffer pointer; a NULL pointer or a NULL buffer is ignored.
 */
void
DeliveryCleanupFree(void *ptr)
{
  free(*(void **)ptr);
}

/**
 * @brief Release a redis reply at the end of the enclosing scope.
 *
 * @param reply_ptr Address of the reply pointer; a NULL pointer or a NULL reply is ignored.
 */
void
DeliveryCleanupReply(redisReply **reply_ptr)
{
  if (IS_PRESENT(reply_ptr) && IS_PRESENT(*reply_ptr)) freeReplyObject(*reply_ptr);
}

/**
 * @brief Release an unpacked command envelope at the end of the enclosing scope.
 *
 * @param envelope_ptr Address of the envelope pointer; a NULL pointer or a NULL envelope is ignored.
 */
void
DeliveryCleanupEnvelope(Envelope **envelope_ptr)
{
  if (IS_PRESENT(envelope_ptr) && IS_PRESENT(*envelope_ptr)) envelope__free_unpacked(*envelope_ptr, NULL);
}

/**
 * @brief Serialise and compress a command envelope for a staged out queue.
 *
 * @param command_envelope_ptr Envelope to serialise; read only.
 * @param[out] payload_ptr_out Compressed bytes, a fresh allocation owned by the caller.
 * @param[out] payload_sz_out  Size of the compressed bytes.
 * @param[out] raw_sz_out      Size before compression, needed to decompress.
 *
 * @return @c true when the envelope was serialised and compressed; nothing is allocated otherwise.
 *
 * @dynamic_memory allocates the compressed buffer, released by the caller.
 */
bool
DeliveryPackEnvelope(Envelope *command_envelope_ptr, void **payload_ptr_out, size_t *payload_sz_out,
                     size_t *raw_sz_out)
{
  if (IS_EMPTY(command_envelope_ptr) || IS_EMPTY(payload_ptr_out) || IS_EMPTY(payload_sz_out)
      || IS_EMPTY(raw_sz_out)) return false;

  size_t raw_sz = envelope__get_packed_size(command_envelope_ptr);

  if (raw_sz == 0) return false;

  __attribute__((cleanup(DeliveryCleanupFree))) uint8_t *packed_ptr = malloc(raw_sz);

  if (IS_EMPTY(packed_ptr)) return false;

  envelope__pack(command_envelope_ptr, packed_ptr);

  size_t   payload_capacity = raw_sz + (raw_sz / 16) + 64;
  uint8_t *payload_ptr      = malloc(payload_capacity);

  if (IS_EMPTY(payload_ptr)) return false;

  unsigned int payload_sz = lzf_compress(packed_ptr, (unsigned int)raw_sz, payload_ptr, (unsigned int)payload_capacity);

  if (payload_sz == 0) {
    free(payload_ptr);

    return false;
  }

  *payload_ptr_out = payload_ptr;
  *payload_sz_out  = (size_t)payload_sz;
  *raw_sz_out      = raw_sz;

  return true;
}

/**
 * @brief Decompress and parse a command envelope taken from a staged out queue.
 *
 * @param payload_ptr Compressed bytes as stored.
 * @param payload_sz  Size of @p payload_ptr as stored.
 * @param raw_sz      Size before compression, carried by the record.
 *
 * @return The unpacked envelope, owned by the caller, or NULL when the bytes do not decompress to
 *         @p raw_sz or do not parse.
 *
 * @dynamic_memory allocates the unpacked envelope, released by the caller.
 */
Envelope *
DeliveryUnpackEnvelope(const void *payload_ptr, size_t payload_sz, size_t raw_sz)
{
  if (IS_EMPTY(payload_ptr) || payload_sz == 0 || raw_sz == 0) return NULL;

  __attribute__((cleanup(DeliveryCleanupFree))) uint8_t *packed_ptr = malloc(raw_sz);

  if (IS_EMPTY(packed_ptr)) return NULL;

  unsigned int packed_sz = lzf_decompress(payload_ptr, (unsigned int)payload_sz, packed_ptr, (unsigned int)raw_sz);

  if (packed_sz != (unsigned int)raw_sz) return NULL;

  return envelope__unpack(NULL, packed_sz, packed_ptr);
}

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
size_t
DeliveryLogAppend(char *buffer_ptr, size_t buffer_sz, size_t offset, const char *format, ...)
{
  va_list args;
  int     written;

  va_start(args, format);
  written = vsnprintf(buffer_ptr + offset, buffer_sz - offset, format, args);
  va_end(args);

  if (written < 0 || (size_t)written >= (buffer_sz - offset)) return 0;

  return (size_t)written;
}

/**
 * @brief Append one drain-attempt entry to a per-event delivery log.
 *
 * @param pers_ptr        Cache backend holding the log.
 * @param add_command     RPUSH template for the log key.
 * @param expire_command  EXPIRE template for the log key.
 * @param queue_name      Queue family used in log messages.
 * @param key_a           First key part.
 * @param key_b           Second key part.
 * @param attempt         Zero-based drain attempt.
 * @param targets_sz      Target count of the drained record.
 * @param failed_uids_ptr Failed uids by value, NULL or empty when every target was delivered.
 * @param outcome_rescode Rescode observed for this attempt.
 *
 * @return @c true when the entry was appended.
 *
 * @dynamic_memory allocates and releases the formatted entry.
 * @locks none
 */
bool
DeliveryLogAttemptAppend(PersistenceBackend *pers_ptr, const char *add_command, const char *expire_command,
                         const char *queue_name, unsigned long key_a, unsigned long key_b, unsigned attempt,
                         size_t targets_sz, const CollectionDescriptor *failed_uids_ptr, int outcome_rescode)
{
  size_t failed_sz = IS_PRESENT(failed_uids_ptr) ? failed_uids_ptr->collection_sz : 0;

  if (unlikely(failed_sz > targets_sz || (failed_sz > 0 && IS_EMPTY(failed_uids_ptr->collection)))) {
    syslog(LOG_DEBUG, "%s {pid:'%lu', queue:'%s', key:'%lu:%lu', attempt:'%u', targets:'%zu', failed:'%zu'}: ERROR: INCONSISTENT ATTEMPT OUTCOME: NOT LOGGING",
           __func__, pthread_self(), queue_name, key_a, key_b, attempt, targets_sz, failed_sz);

    return false;
  }

  if (unlikely(IS_EMPTY(pers_ptr))) {
    syslog(LOG_DEBUG, "%s {pid:'%lu', queue:'%s', key:'%lu:%lu'}: ERROR: NO CACHE BACKEND: NOT LOGGING ATTEMPT",
           __func__, pthread_self(), queue_name, key_a, key_b);

    return false;
  }

  size_t entry_sz = (failed_sz * DELIVERY_LOG_UID_DIGITS) + DELIVERY_LOG_ENTRY_EXTRA;
  __attribute__((cleanup(DeliveryCleanupFree))) char *entry_ptr = malloc(entry_sz);

  if (unlikely(IS_EMPTY(entry_ptr))) return false;

  size_t entry_len;

  if (failed_sz == 0) {
    entry_len = DeliveryLogAppend(entry_ptr, entry_sz, 0, "%u:outcome:*", attempt);
  } else {
    entry_len = DeliveryLogAppend(entry_ptr, entry_sz, 0, "%u:outcome:%zu-%zu:rescode:%d", attempt, targets_sz,
                                  failed_sz, outcome_rescode);
  }

  if (entry_len == 0) return false;

  size_t written = DeliveryLogAppend(entry_ptr, entry_sz, entry_len, ":when:%lld:worker:%lu", GetTimeNowInMillis(),
                                     (unsigned long)pthread_self());

  if (written == 0) return false;

  entry_len += written;

  for (size_t i = 0; i < failed_sz; i++) {
    written = DeliveryLogAppend(entry_ptr, entry_sz, entry_len, ":uid:%lu", sCollectionUidAt(failed_uids_ptr, i));

    if (written == 0) return false;

    entry_len += written;
  }

  {
    __attribute__((cleanup(DeliveryCleanupReply))) redisReply *redis_ptr = (*pers_ptr->send_command)(
      NULL, pers_ptr, add_command, key_a, key_b, entry_ptr, entry_len);

    if (unlikely(IS_EMPTY(redis_ptr)) || redis_ptr->type != REDIS_REPLY_INTEGER || redis_ptr->integer < 0) return false;
  }

  {
    __attribute__((cleanup(DeliveryCleanupReply))) redisReply *redis_ptr = (*pers_ptr->send_command)(
      NULL, pers_ptr, expire_command, key_a, key_b, (unsigned)DELIVERY_LOG_TTL_SECONDS);

    (void)redis_ptr;
  }

  return true;
}

/**
 * @brief Classify a drained record's per-recipient outcomes into the uids to retry.
 *
 * @param delivery_results  Per-recipient collection, may be NULL.
 * @param delivery_res_ptr  Result returned by the deliverer, may be NULL.
 * @param targets_sz        Target count of the drained record.
 * @param[out] retry_uids   Buffer receiving the failed uids.
 * @param[out] failed_sz_out      Number of uids written.
 * @param[out] batch_retry_out    True when the whole snapshot must be retried.
 * @param[out] failure_rescode_out First attributed failure rescode.
 */
void
DeliveryCollectRetryUids(CollectionDescriptor *delivery_results, const UFSRVResult *delivery_res_ptr,
                         size_t targets_sz, unsigned long *retry_uids, size_t *failed_sz_out,
                         bool *batch_retry_out, int *failure_rescode_out)
{
  size_t failed_sz       = 0;
  bool   have_attributed = false;
  bool   batch_retry     = false;
  int    rescode         = IS_PRESENT(delivery_res_ptr) ? (int)_RESULT_CODE(delivery_res_ptr) : 0;

  if (IS_PRESENT(delivery_results) && IS_PRESENT(retry_uids) && IS_PRESENT(delivery_results->collection)
      && delivery_results->collection_sz >= targets_sz) {
    for (size_t i=0; i<targets_sz; i++) {
      StagedUserDeliveryResult *result_ptr = sStagedDeliveryResultAt(delivery_results, i);

      if (sStagedDeliveryResultPending(result_ptr)) {
        retry_uids[failed_sz++] = result_ptr->uid;

        continue;
      }

      if (!sStagedDeliveryResultFailed(result_ptr)) continue;

      if (!have_attributed) {
        rescode         = result_ptr->rescode;
        have_attributed = true;
      }

      retry_uids[failed_sz++] = result_ptr->uid;
    }
  } else if (IS_PRESENT(delivery_res_ptr) && _RESULT_TYPE_ERROR(delivery_res_ptr)) {
    batch_retry = true;
    failed_sz   = targets_sz;
  }

  if (failed_sz == 0 && IS_PRESENT(delivery_res_ptr) && _RESULT_TYPE_ERROR(delivery_res_ptr)) {
    batch_retry = true;
    failed_sz   = targets_sz;
  }

  if (IS_PRESENT(failed_sz_out))       *failed_sz_out       = failed_sz;
  if (IS_PRESENT(batch_retry_out))     *batch_retry_out     = batch_retry;
  if (IS_PRESENT(failure_rescode_out)) *failure_rescode_out = rescode;
}

/**
 * @brief Release the allocations an extracted queue record hands to its caller.
 *
 * @param staged_message_descriptor_ptr Drained record; safe on a zeroed or partially parsed one.
 */
void
DeliveryFreeExtractedRecord(const StagedMessageDescriptor *staged_message_descriptor_ptr)
{
  if (IS_EMPTY(staged_message_descriptor_ptr)) return;

  free((void *)staged_message_descriptor_ptr->payload.uids);
  free((void *)staged_message_descriptor_ptr->payload.payload_ptr);
}
