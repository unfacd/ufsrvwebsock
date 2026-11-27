/**
 * @file user_command_delivery.c
 * @brief user_command_delivery
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
#include "protocol_websocket.h"
#include "ufsrvcmd_user_callbacks.h"
#include "command_delivery/user_command_delivery.h"
#include "command_delivery/message_command_delivery.h"
#include "command_delivery/utils_command_delivery.h"
#include <ufsrv_core/delegator_sessions/workers/user_callback_job.h>
#include <ufsrv_core/delegator_sessions/workers/user_callback_job_descriptor_type.h>
#include <ufnetcorelib/ufnet_msgcore/user/user_staged_out_queue.h>
#include <ufnetcorelib/ufnet_msgcore/UnfacdService.pb-c.h>
#include <ufsrv_core/delegator_sessions/thread_context_type.h>
#include <ufnetcorelib/ufnet_msgcore/session.h>
#include <ufnetcorelib/ufnet_msgcore/type_providers/session_provider.h>
#include <uflib/lzf/lzf.h>
#include <uflib/utils.h>
#include <uflib/utils_time.h>
#include <ufsrv_core/cache_backend/redis.h>
#include <uflib/main_types.h>
#include <uflib/collection_descriptor_type.h>

extern __thread ThreadContext ufsrv_thread_context;

/**
 * Attempt log for the user out queue, a rolling redis list per user event.
 */
#define USER_OUT_QUEUE_LOG_CMD_ADD     "RPUSH USER_OUT_QUEUE_LOG:%lu:%lu %b"
#define USER_OUT_QUEUE_LOG_CMD_EXPIRE  "EXPIRE USER_OUT_QUEUE_LOG:%lu:%lu %u"
#define USER_OUT_QUEUE_MAX_ATTEMPTS    (3)		///< total drain attempts per record, attempts are zero-based




/**
 * @brief Release an unpacked command envelope at the end of the enclosing scope.
 *
 * @param envelope_ptr Address of the envelope pointer; a NULL pointer or a NULL envelope is ignored.
 */
static void
sCleanupEnvelope(Envelope **envelope_ptr)
{
  if (IS_PRESENT(envelope_ptr) && IS_PRESENT(*envelope_ptr)) envelope__free_unpacked(*envelope_ptr, NULL);
}

/**
 * @brief Pack and compress a command envelope for the user out queue.
 *
 * @param command_envelope_ptr Envelope to serialise; read only.
 * @param[out] payload_ptr_out Compressed bytes, a fresh allocation owned by the caller.
 * @param[out] payload_sz_out  Size of the compressed bytes.
 * @param[out] raw_sz_out      Size before compression, needed to decompress.
 *
 * @return @c true when the envelope was serialised and compressed.
 */
static bool
sPackEnvelopeForUserOutQueue(Envelope *command_envelope_ptr, void **payload_ptr_out, size_t *payload_sz_out,
                             size_t *raw_sz_out)
{
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
 * @brief Stage a client-bound command envelope on the recipient's user out queue.
 *
 * The accept thread only builds and stores the envelope; delivery happens later on a job worker,
 * which replays it per recipient, retries failures and logs the attempt. The record is keyed by the
 * recipient's uid and their freshly allocated eid, so the key and the envelope identity agree.
 *
 * @param instance_sesn_ptr_sender Session that accepted the command; it is the record's @c userid.
 * @param uid_target               Recipient user whose eid keys the queue.
 * @param command_envelope_ptr     Envelope to deliver; packed here, never retained.
 * @param eid                      Recipient's event id for this network activity.
 * @param fid                      Fence the command belongs to, zero when it has no fence.
 *
 * @return Store result: success once the record is queued, or the store failure. A failed signal
 *         still leaves the record queued for a later drain or restart.
 */
UFSRVResult *
UserCommandStageEnvelopeForDelivery(InstanceHolderForSession *instance_sesn_ptr_sender, unsigned long uid_target,
                                    Envelope *command_envelope_ptr, unsigned long eid, unsigned long fid)
{
  if (unlikely(IS_EMPTY(instance_sesn_ptr_sender) || IS_EMPTY(command_envelope_ptr)
               || IS_EMPTY(command_envelope_ptr->ufsrvcommand)
               || IS_EMPTY(command_envelope_ptr->ufsrvcommand->header))) {
    THREAD_CONTEXT_RETURN_RESULT_ERROR(NULL, RESCODE_PROG_NULL_POINTER)
  }

  Session *sesn_ptr = SessionOffInstanceHolder(instance_sesn_ptr_sender);

  __attribute__((cleanup(DeliveryCleanupFree))) void *payload_ptr = NULL;
  size_t payload_sz = 0;
  size_t raw_sz     = 0;

  if (!sPackEnvelopeForUserOutQueue(command_envelope_ptr, &payload_ptr, &payload_sz, &raw_sz)) {
    THREAD_CONTEXT_RETURN_RESULT_ERROR(NULL, RESCODE_PROG_INCONSISTENT_DATA)
  }

  StagedMessageDescriptor staged_message_descriptor = {
      .gid      = 0,
      .eid      = eid,
      .fid      = fid,
      .userid   = SESSION_USERID(sesn_ptr),
      .attempts = 0,
      .payload  = {
          .uids        = &uid_target,
          .uids_sz     = 1,
          .payload_ptr = payload_ptr,
          .payload_sz  = payload_sz,
          .raw_sz      = raw_sz
      }
  };

  UFSRVResult *store_res_ptr = StoreUserOutQueueMessage(uid_target, &staged_message_descriptor);

  if (_RESULT_TYPE_ERROR(store_res_ptr)) return store_res_ptr;

  if (!UserCommandDeliverySignal(uid_target, eid)) {
    /* the record is already queued: a later drain, or a restart, still delivers it */
    syslog(LOG_DEBUG, "%s {pid:'%lu', uid:'%lu', eid:'%lu'}: ERROR: STAGED USER COMMAND NOT SIGNALLED",
           __func__, pthread_self(), uid_target, eid);
  }

  return store_res_ptr;
}

/**
 * @brief Append one drain-attempt outcome to @c USER_OUT_QUEUE_LOG:<uid>:<eid>.
 *
 * Same grammar and TTL as the fence attempt log; best effort and never changes the outcome.
 *
 * @param uid             Recipient user, part of the log key.
 * @param eid             Event id, part of the log key.
 * @param attempt         Zero-based drain attempt.
 * @param targets_sz      Target count of the drained record (@c m).
 * @param failed_uids_ptr Failed target uids carried by value, NULL when all were delivered.
 * @param outcome_rescode Rescode observed for this attempt.
 *
 * @return @c true when the entry was appended.
 */
static bool
sUserCommandDeliveryLogAttempt(unsigned long uid, unsigned long eid, unsigned attempt, size_t targets_sz,
                               const CollectionDescriptor *failed_uids_ptr, int outcome_rescode)
{
  return DeliveryLogAttemptAppend(THREAD_CONTEXT_USRMSG_CACHEBACKEND, USER_OUT_QUEUE_LOG_CMD_ADD,
                                  USER_OUT_QUEUE_LOG_CMD_EXPIRE, "USER_OUT_QUEUE", uid, eid, attempt, targets_sz,
                                  failed_uids_ptr, outcome_rescode);
}

/**
 * @brief Replay one staged user command to its recipients and record each outcome.
 *
 * Resolves the sender from the record, then each recipient, and invokes the client-bound callback
 * for every one of them; the callback transmits through the staged gateway, so a contended target
 * session is recorded as a failure instead of a silent drop.
 *
 * @param staged_message_descriptor_ptr Drained user queue record.
 * @param[out] delivery_results_out Per-recipient outcomes, indexed like the uid snapshot.
 *
 * @return Success when every recipient was recorded successfully, otherwise the first failure.
 */
static UFSRVResult *
sDeliverStagedUserCommand(const StagedMessageDescriptor *staged_message_descriptor_ptr,
                          CollectionDescriptor *delivery_results_out)
{
  if (IS_EMPTY(staged_message_descriptor_ptr))                      THREAD_CONTEXT_RETURN_RESULT_ERROR(NULL, RESCODE_PROG_NULL_POINTER)
  if (IS_EMPTY(staged_message_descriptor_ptr->payload.payload_ptr)) THREAD_CONTEXT_RETURN_RESULT_ERROR(NULL, RESCODE_PROG_MISSING_PARAM)

  size_t snapshot_sz = staged_message_descriptor_ptr->payload.uids_sz;

  if (IS_EMPTY(staged_message_descriptor_ptr->payload.uids) || snapshot_sz == 0) {
    THREAD_CONTEXT_RETURN_RESULT_ERROR(NULL, RESCODE_PROG_MISSING_PARAM)
  }

  if (IS_PRESENT(delivery_results_out)
      && (IS_EMPTY(delivery_results_out->collection) || delivery_results_out->collection_sz < snapshot_sz)) {
    delivery_results_out = NULL;
  }

  for (size_t i = 0; IS_PRESENT(delivery_results_out) && i < snapshot_sz; i++) {
    sStagedDeliveryResultStore(delivery_results_out, i, staged_message_descriptor_ptr->payload.uids[i],
                                  RESULT_TYPE_STAGED, RESCODE_PROG_NULL_POINTER);
  }

  size_t raw_sz = staged_message_descriptor_ptr->payload.raw_sz;
  __attribute__((cleanup(DeliveryCleanupFree))) uint8_t *packed_ptr = malloc(raw_sz);

  if (unlikely(IS_EMPTY(packed_ptr))) THREAD_CONTEXT_RETURN_RESULT_ERROR(NULL, RESCODE_PROG_MEMORY_EXHAUSTED)

  unsigned int packed_sz = lzf_decompress(staged_message_descriptor_ptr->payload.payload_ptr,
                                          (unsigned int)staged_message_descriptor_ptr->payload.payload_sz,
                                          packed_ptr, (unsigned int)raw_sz);

  if (unlikely(packed_sz != (unsigned int)raw_sz)) {
    THREAD_CONTEXT_RETURN_RESULT_ERROR(NULL, RESCODE_BACKEND_DATA_TRANSFORMATION)
  }

  __attribute__((cleanup(sCleanupEnvelope))) Envelope *command_envelope_ptr = envelope__unpack(NULL, packed_sz, packed_ptr);

  if (unlikely(IS_EMPTY(command_envelope_ptr) || IS_EMPTY(command_envelope_ptr->ufsrvcommand)
               || IS_EMPTY(command_envelope_ptr->ufsrvcommand->header))) {
    THREAD_CONTEXT_RETURN_RESULT_ERROR(NULL, RESCODE_PROG_INCONSISTENT_DATA)
  }

  CommandHeader *command_header_ptr = command_envelope_ptr->ufsrvcommand->header;

  UfsrvCommandMarshallingDescriptor ufsrv_description = {
      command_header_ptr->eid, staged_message_descriptor_ptr->fid, command_header_ptr->when, &EnvelopeMetaData,
      command_envelope_ptr, command_header_ptr->gid
  };

  unsigned long sesn_call_flags = (CALL_FLAG_HASH_SESSION_LOCALLY | CALL_FLAG_HASH_UID_LOCALLY |
                                   CALL_FLAG_HASH_USERNAME_LOCALLY | CALL_FLAG_ATTACH_FENCE_LIST_TO_SESSION |
                                   CALL_FLAG_REMOTE_SESSION);

  bool sender_lock_already_owned = false;

  GetSessionForThisUserByUserId(NULL, staged_message_descriptor_ptr->userid, &sender_lock_already_owned, sesn_call_flags);
  InstanceHolderForSession *instance_sesn_ptr_sender = THREAD_CONTEXT_UFSRV_RESULT_USERDATA;

  if (IS_EMPTY(instance_sesn_ptr_sender)) THREAD_CONTEXT_RETURN_RESULT_ERROR(NULL, RESCODE_USER_INVALID_UID)

  Session *sesn_ptr_sender = SessionOffInstanceHolder(instance_sesn_ptr_sender);

  WebSocketMessage *wsm_ptr_synthesised = &(WebSocketMessage) {.request = NULL, .type = WEB_SOCKET_MESSAGE__TYPE__REQUEST};

  UFSRVResult *first_failure_ptr = NULL;

  for (size_t i=0; i<snapshot_sz; i++) {
    unsigned long uid_target         = staged_message_descriptor_ptr->payload.uids[i];
    bool          lock_already_owned = false;

    GetSessionForThisUserByUserId(NULL, uid_target, &lock_already_owned, sesn_call_flags);
    InstanceHolderForSession *instance_sesn_ptr_target = THREAD_CONTEXT_UFSRV_RESULT_USERDATA;

    if (IS_EMPTY(instance_sesn_ptr_target)) {
      syslog(LOG_DEBUG, "%s {pid:'%lu', eid:'%lu', target_idx:'%zu', uid_target:'%lu'}: ERROR: TARGET SESSION NOT FOUND",
             __func__, pthread_self(), staged_message_descriptor_ptr->eid, i, uid_target);

      sStagedDeliveryResultStore(delivery_results_out, i, uid_target, RESULT_TYPE_ERR, RESCODE_USER_INVALID_UID);

      continue;
    }

    Session *sesn_ptr_target = SessionOffInstanceHolder(instance_sesn_ptr_target);

    UFSRVResult *callback_res_ptr = UfsrvCommandInvokeUserCommand(
        &(InstanceContextForSession) {instance_sesn_ptr_sender, AS_UFSRV_SESSION(sesn_ptr_sender)},
        &(InstanceContextForSession) {instance_sesn_ptr_target, AS_UFSRV_SESSION(sesn_ptr_target)},
        wsm_ptr_synthesised, NULL, &ufsrv_description, uSETACCOUNT_ATTRS_V1_IDX);

    sStagedDeliveryResultRecord(delivery_results_out, i, uid_target, callback_res_ptr);

    if (IS_PRESENT(callback_res_ptr) && _RESULT_TYPE_ERROR(callback_res_ptr) && IS_EMPTY(first_failure_ptr)) {
      first_failure_ptr = callback_res_ptr;
    }
  }

  if (IS_PRESENT(first_failure_ptr)) return first_failure_ptr;

  THREAD_CONTEXT_RETURN_RESULT_SUCCESS(NULL, RESCODE_PROG_NULL_POINTER)
}

typedef struct UserOutQueueDrainArgs {
  unsigned long uid;
  unsigned long eid;
} UserOutQueueDrainArgs;

static void sDrainUserOutQueue(unsigned long uid, unsigned long eid);

static UserCallBackReturnArgs *
sDrainUserOutQueueJob(UserCallBackArgs *args_ptr, InstanceHolder *instance_holder_ptr)
{
  UserOutQueueDrainArgs *drain_args_ptr = (UserOutQueueDrainArgs *)args_ptr;

  sDrainUserOutQueue(drain_args_ptr->uid, drain_args_ptr->eid);

  return NULL;
}

static void
sFinaliseUserOutQueueJob(UserCallBackArgs *args_ptr)
{
  UserCallbackJobDescriptor *descriptor_ptr = AS_USER_CALLBACK_JOB_DESCRIPTOR(args_ptr);

  free(descriptor_ptr->callback_args);
  free(descriptor_ptr);
}

/**
 * @brief Ask the job pool to drain one user out queue.
 *
 * @param uid Target user owning the queue.
 * @param eid Event id of the staged record.
 *
 * @return @c true when the work was queued; the record stays queued otherwise.
 *
 * @dynamic_memory moves a drain descriptor to the job pool, released by its finaliser.
 */
bool
UserCommandDeliverySignal(unsigned long uid, unsigned long eid)
{
  __attribute__((cleanup(DeliveryCleanupFree))) UserOutQueueDrainArgs *drain_args_ptr = malloc(sizeof(UserOutQueueDrainArgs));

  if (unlikely(IS_EMPTY(drain_args_ptr))) return false;

  drain_args_ptr->uid = uid;
  drain_args_ptr->eid = eid;

  __attribute__((cleanup(DeliveryCleanupFree))) UserCallbackJobDescriptor *descriptor_ptr = calloc(1, sizeof(UserCallbackJobDescriptor));

  if (unlikely(IS_EMPTY(descriptor_ptr))) return false;

  descriptor_ptr->handler       = sDrainUserOutQueueJob;
  descriptor_ptr->finaliser     = sFinaliseUserOutQueueJob;
  descriptor_ptr->callback_args = drain_args_ptr;

  if (!QueueInUserCallBackJob(descriptor_ptr)) {
    syslog(LOG_DEBUG, "%s {pid:'%lu', uid:'%lu', eid:'%lu'}: ERROR: JOB POOL NOT ACCEPTING USER COMMAND WORK",
           __func__, pthread_self(), uid, eid);

    return false;
  }

  descriptor_ptr = NULL;
  drain_args_ptr = NULL;

  return true;
}

static void
sDrainUserOutQueue(unsigned long uid, unsigned long eid)
{
  size_t delivered = 0;

  for (;;) {
    StagedMessageDescriptor staged_message_descriptor = {0};
    UFSRVResult *res_ptr = ExtractUserOutQueueMessage(uid, eid, &staged_message_descriptor);

    if (_RESULT_CODE_EQUAL(res_ptr, RESCODE_BACKEND_DATA_EMPTYSET)) {
      syslog(LOG_DEBUG, "%s {pid:'%lu', uid:'%lu', eid:'%lu', delivered:'%zu'}: NOTICE: USER OUT QUEUE DRAINED",
             __func__, pthread_self(), uid, eid, delivered);

      return;
    }

    if (!_RESULT_TYPE_SUCCESS(res_ptr) && (_RESULT_CODE_EQUAL(res_ptr, RESCODE_BACKEND_CONNECTION)
                                           || _RESULT_CODE_EQUAL(res_ptr, RESCODE_BACKEND_RESOURCE_NULL))) {
      return;
    }

    if (!_RESULT_TYPE_SUCCESS(res_ptr)) continue;

    __attribute__((cleanup(DeliveryCleanupFree))) StagedUserDeliveryResult *delivery_results = NULL;
    delivery_results = calloc(staged_message_descriptor.payload.uids_sz, sizeof(StagedUserDeliveryResult));

    CollectionDescriptor delivery_results_collection = {
        .collection             = AS_COLLECTION_TYPE(delivery_results),
        .collection_sz          = staged_message_descriptor.payload.uids_sz,
        .collection_base_offset = sizeof(StagedUserDeliveryResult)
    };

    UFSRVResult *delivery_res_ptr = sDeliverStagedUserCommand(&staged_message_descriptor, &delivery_results_collection);

    size_t  failed_sz       = 0;
    int     failure_rescode = 0;
    bool    batch_retry     = false;

    __attribute__((cleanup(DeliveryCleanupFree))) unsigned long *retry_uids =
      calloc(staged_message_descriptor.payload.uids_sz, sizeof(unsigned long));

    DeliveryCollectRetryUids(&delivery_results_collection, delivery_res_ptr,
                              staged_message_descriptor.payload.uids_sz, retry_uids,
                              &failed_sz, &batch_retry, &failure_rescode);

    CollectionDescriptor failed_uids_collection = {
        .collection             = AS_COLLECTION_TYPE(batch_retry ? staged_message_descriptor.payload.uids : retry_uids),
        .collection_sz          = failed_sz,
        .collection_base_offset = sizeof(unsigned long)
    };

    sUserCommandDeliveryLogAttempt(uid, eid, staged_message_descriptor.attempts,
                                  staged_message_descriptor.payload.uids_sz, &failed_uids_collection, failure_rescode);

    if (failed_sz > 0 && staged_message_descriptor.attempts + 1 < USER_OUT_QUEUE_MAX_ATTEMPTS) {
      StagedMessageDescriptor retry_descriptor = staged_message_descriptor;

      retry_descriptor.attempts        = staged_message_descriptor.attempts + 1;
      retry_descriptor.payload.uids    = batch_retry ? (unsigned long *)staged_message_descriptor.payload.uids : retry_uids;
      retry_descriptor.payload.uids_sz = failed_sz;

      UFSRVResult *restage_res_ptr = StoreUserOutQueueMessage(uid, &retry_descriptor);

      if (_RESULT_TYPE_SUCCESS(restage_res_ptr)) {
        UserCommandDeliverySignal(uid, eid);
      } else {
        syslog(LOG_DEBUG, "%s {pid:'%lu', uid:'%lu', eid:'%lu', rescode:'%d'}: ERROR: COULD NOT RE-STAGE FAILED USER COMMAND TARGETS",
               __func__, pthread_self(), uid, eid, _RESULT_CODE(restage_res_ptr));
      }
    }

    if (_RESULT_TYPE_ERROR(delivery_res_ptr)) {
      syslog(LOG_DEBUG, "%s {pid:'%lu', uid:'%lu', eid:'%lu', rescode:'%d'}: ERROR: COULD NOT DELIVER STAGED USER COMMAND",
             __func__, pthread_self(), uid, eid, _RESULT_CODE(delivery_res_ptr));
    } else {
      delivered++;
    }

    DeliveryFreeExtractedRecord(&staged_message_descriptor);
  }
}
