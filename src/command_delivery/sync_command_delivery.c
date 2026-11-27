/**
 * @file sync_command_delivery.c
 * @brief Guardian sync copies delivered through the sync staged out queue.
 *
 * A message event accepted from a user who has a guardian attached also produces a copy for that
 * guardian. The copy is not sent from the accepting thread: the event id is allocated for the
 * originating user, stamped on the sync envelope, and the envelope is stored under
 * @c SYNC_OUT_QUEUE:<uid>:<eid>. A job worker pops it, resolves the guardian from the originator's
 * session at delivery time, replays the command through @ref uSYNC_V1 and records the attempt.
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
#include "command_delivery/sync_command_delivery.h"
#include "command_delivery/utils_command_delivery.h"
#include <ufsrv_core/delegator_sessions/workers/user_callback_job.h>
#include <ufsrv_core/delegator_sessions/workers/user_callback_job_descriptor_type.h>
#include <ufnetcorelib/ufnet_msgcore/user/sync_staged_out_queue.h>
#include <ufnetcorelib/ufnetcorelib_defs.h>
#include <ufsrv_core/delegator_sessions/thread_context_type.h>
#include <ufnetcorelib/ufnet_msgcore/session.h>
#include <ufnetcorelib/ufnet_msgcore/type_providers/session_provider.h>
#include <ufnetcorelib/ufnet_msgcore/user/users_protobuf.h>
#include "command_controllers/message_command_utils.h"
#include <uflib/utils.h>
#include <uflib/utils_time.h>
#include <uflib/main_types.h>
#include <uflib/collection_descriptor_type.h>

extern __thread ThreadContext ufsrv_thread_context;

/**
 * Attempt log for the sync out queue, a rolling redis list per user event.
 */
#define SYNC_OUT_QUEUE_LOG_CMD_ADD     "RPUSH SYNC_OUT_QUEUE_LOG:%lu:%lu %b"
#define SYNC_OUT_QUEUE_LOG_CMD_EXPIRE  "EXPIRE SYNC_OUT_QUEUE_LOG:%lu:%lu %u"
#define SYNC_OUT_QUEUE_MAX_ATTEMPTS    (3)		///< total drain attempts per record, attempts are zero-based

/**
 * @brief Stage the guardian's copy of an accepted message event.
 *
 * @param ctx_sesn_ptr    Originator context; its session is the record's @c userid and queue uid.
 * @param dm_ptr_received Received command the copy is built from; read only.
 *
 * @return Store result, or the failure that prevented staging.
 *
 * @dynamic_memory packs the sync envelope, releases the packed buffer before returning, and leaves
 *                the record owned by the queue.
 * @locks the caller's session lock is assumed; nothing else is taken here.
 */
UFSRVResult *
SyncCommandStageForGuardian(InstanceContextForSession *ctx_sesn_ptr, DataMessage *dm_ptr_received)
{
  if (unlikely(IS_EMPTY(ctx_sesn_ptr) || IS_EMPTY(ctx_sesn_ptr->sesn_ptr) || IS_EMPTY(dm_ptr_received)
               || IS_EMPTY(dm_ptr_received->ufsrvcommand)
               || IS_EMPTY(dm_ptr_received->ufsrvcommand->msgcommand))) {
    THREAD_CONTEXT_RETURN_RESULT_ERROR(NULL, RESCODE_PROG_NULL_POINTER)
  }

  Session         *sesn_ptr  = AS_SESSION(ctx_sesn_ptr->sesn_ptr);
  MessageCommand  *msgcmd_ptr = dm_ptr_received->ufsrvcommand->msgcommand;
  unsigned long    userid    = SESSION_USERID(sesn_ptr);

  if (unlikely(SESSION_USERGUARDIAN_UID(sesn_ptr) == 0)) THREAD_CONTEXT_RETURN_RESULT_ERROR(
    NULL, RESCODE_PROG_MISSING_PARAM)

  /* the accepting thread owns the session lock: the event id and the envelope both describe this
     event, so the id is allocated and stamped before anything is packed */
  unsigned long eid = GenerateUserEventId(sesn_ptr);

  if (unlikely(eid == 0)) THREAD_CONTEXT_RETURN_RESULT_ERROR(NULL, RESCODE_BACKEND_CONNECTION)

  Envelope          command_envelope = ENVELOPE__INIT;
  CommandHeader     header           = COMMAND_HEADER__INIT;
  SyncCommand       sync_command     = SYNC_COMMAND__INIT;
  UfsrvCommandWire  ufsrv_command    = UFSRV_COMMAND_WIRE__INIT;
  UfsrvCommandWire *originator_command[1];
  UserRecord        user_record_originator;

  command_envelope.sourceufsrvuid  = "0"; /* ufsrv initiated origin */
  command_envelope.timestamp       = GetTimeNowInMillis(); command_envelope.has_timestamp = 1;
  command_envelope.servertimestamp = command_envelope.timestamp; command_envelope.has_servertimestamp = 1;
  command_envelope.type            = GetProtocolType(msgcmd_ptr, NULL); command_envelope.has_type = 1;
  command_envelope.ufsrvcommand    = &ufsrv_command;

  ufsrv_command.ufsrvtype          = UFSRV_COMMAND_WIRE__UFSRV_TYPE__UFSRV_SYNC;
  ufsrv_command.synccommand        = &sync_command;

  sync_command.header              = &header;
  sync_command.type                = SYNC_COMMAND__COMMAND_TYPES__GUARDIAN_SYNC;
  sync_command.originator          = ProvideUserRecordFromSessionInProto(sesn_ptr, &user_record_originator,
                                                                        PROTO_USER_RECORD_MINIMAL,
                                                                        PROTO_USER_RECORD_BYREF);
  sync_command.originator_command  = originator_command; sync_command.n_originator_command = 1;
  originator_command[0]            = dm_ptr_received->ufsrvcommand;

  header.when          = msgcmd_ptr->header->when; header.has_when = 1;
  header.when_client   = msgcmd_ptr->header->when_client; header.has_when_client = 1;
  header.eid           = eid; header.has_eid = 1;

  __attribute__((cleanup(DeliveryCleanupFree))) void *payload_ptr = NULL;
  size_t payload_sz = 0;
  size_t raw_sz     = 0;

  if (!DeliveryPackEnvelope(&command_envelope, &payload_ptr, &payload_sz, &raw_sz)) {
    THREAD_CONTEXT_RETURN_RESULT_ERROR(NULL, RESCODE_PROG_INCONSISTENT_DATA)
  }

  /* one recipient, named by the key: the record stores no target snapshot and no fence identity */
  StagedMessageDescriptor staged_message_descriptor = {
      .gid      = 0,
      .eid      = eid,
      .fid      = 0,
      .userid   = userid,
      .attempts = 0,
      .payload  = {
          .uids        = &userid,
          .uids_sz     = 1,
          .payload_ptr = payload_ptr,
          .payload_sz  = payload_sz,
          .raw_sz      = raw_sz
      }
  };

  UFSRVResult *store_res_ptr = StoreSyncOutQueueMessage(userid, &staged_message_descriptor);

  if (unlikely(_RESULT_TYPE_ERROR(store_res_ptr))) return store_res_ptr;

  if (!SyncCommandDeliverySignal(userid, eid)) {
    /* the record is already queued: a later drain, or a restart, still delivers it */
    syslog(LOG_DEBUG, "%s {pid:'%lu', uid:'%lu', eid:'%lu'}: ERROR: STAGED SYNC COPY NOT SIGNALLED",
           __func__, pthread_self(), userid, eid);
  }

  return store_res_ptr;
}

/**
 * @brief Append one drain-attempt outcome to @c SYNC_OUT_QUEUE_LOG:<uid>:<eid>.
 *
 * Same grammar and TTL as the other staged queues; best effort and never changes the outcome.
 *
 * @param uid             User whose event the queue belongs to.
 * @param eid             Event id of the record.
 * @param attempt         Zero-based drain attempt.
 * @param targets_sz      Target count of the drained record, one for this queue.
 * @param failed_uids_ptr Guardian uid when the attempt failed, empty when it was delivered.
 * @param outcome_rescode Rescode observed for this attempt.
 *
 * @return @c true when the entry was appended.
 */
static bool
sSyncCommandDeliveryLogAttempt(unsigned long uid, unsigned long eid, unsigned attempt, size_t targets_sz,
                               const CollectionDescriptor *failed_uids_ptr, int outcome_rescode)
{
  return DeliveryLogAttemptAppend(THREAD_CONTEXT_USRMSG_CACHEBACKEND, SYNC_OUT_QUEUE_LOG_CMD_ADD,
                                  SYNC_OUT_QUEUE_LOG_CMD_EXPIRE, "SYNC_OUT_QUEUE", uid, eid, attempt, targets_sz,
                                  failed_uids_ptr, outcome_rescode);
}

/**
 * @brief Replay one staged sync copy to the originator's guardian.
 *
 * The guardian is read from the originator's session at delivery time, so the copy is bound to the
 * relation as it stands now. The callback transmits through the staged gateway, so a contended
 * guardian session is recorded as a failure instead of a silent drop.
 *
 * @param staged_message_descriptor_ptr Drained sync queue record.
 *
 * @return The callback's result, or the failure that stopped the replay.
 */
static UFSRVResult *
sDeliverStagedSyncCommand(const StagedMessageDescriptor *staged_message_descriptor_ptr)
{
  if (IS_EMPTY(staged_message_descriptor_ptr) || IS_EMPTY(staged_message_descriptor_ptr->payload.payload_ptr)) {
    THREAD_CONTEXT_RETURN_RESULT_ERROR(NULL, RESCODE_PROG_MISSING_PARAM)
  }

  __attribute__((cleanup(DeliveryCleanupEnvelope))) Envelope *command_envelope_ptr =
      DeliveryUnpackEnvelope(staged_message_descriptor_ptr->payload.payload_ptr,
                             staged_message_descriptor_ptr->payload.payload_sz,
                             staged_message_descriptor_ptr->payload.raw_sz);

  if (unlikely(IS_EMPTY(command_envelope_ptr) || IS_EMPTY(command_envelope_ptr->ufsrvcommand)
               || IS_EMPTY(command_envelope_ptr->ufsrvcommand->header))) {
    THREAD_CONTEXT_RETURN_RESULT_ERROR(NULL, RESCODE_BACKEND_DATA_TRANSFORMATION)
  }

  unsigned long sesn_call_flags = (CALL_FLAG_HASH_SESSION_LOCALLY | CALL_FLAG_HASH_UID_LOCALLY |
                                   CALL_FLAG_HASH_USERNAME_LOCALLY | CALL_FLAG_ATTACH_FENCE_LIST_TO_SESSION |
                                   CALL_FLAG_REMOTE_SESSION);

  bool lock_already_owned = false;

  GetSessionForThisUserByUserId(NULL, staged_message_descriptor_ptr->userid, &lock_already_owned, sesn_call_flags);
  InstanceHolderForSession *instance_sesn_ptr_originator = THREAD_CONTEXT_UFSRV_RESULT_USERDATA;

  if (unlikely(IS_EMPTY(instance_sesn_ptr_originator))) THREAD_CONTEXT_RETURN_RESULT_ERROR(
    NULL, RESCODE_USER_INVALID_UID)

  Session *sesn_ptr_originator = SessionOffInstanceHolder(instance_sesn_ptr_originator);

  unsigned long guardian_uid = SESSION_USERGUARDIAN_UID(sesn_ptr_originator);

  if (guardian_uid == 0) {
    /* the relation was removed after the event was accepted: there is no copy left to deliver, and
       no retry can create one */
    THREAD_CONTEXT_RETURN_RESULT_SUCCESS(NULL, RESCODE_LOGIC_EMPTY_RESOURCE)
  }

  GetSessionForThisUserByUserId(NULL, guardian_uid, &lock_already_owned, sesn_call_flags);
  InstanceHolderForSession *instance_sesn_ptr_guardian = THREAD_CONTEXT_UFSRV_RESULT_USERDATA;

  if (unlikely(IS_EMPTY(instance_sesn_ptr_guardian))) THREAD_CONTEXT_RETURN_RESULT_ERROR(
    NULL, RESCODE_USER_INVALID_UID)

  Session *sesn_ptr_guardian = SessionOffInstanceHolder(instance_sesn_ptr_guardian);

  CommandHeader *command_header_ptr = command_envelope_ptr->ufsrvcommand->header;

  UfsrvCommandMarshallingDescriptor ufsrv_description = {
      command_header_ptr->eid, 0, command_header_ptr->when, &EnvelopeMetaData, command_envelope_ptr,
      command_header_ptr->gid
  };

  WebSocketMessage wsm_synthesised = {.request = NULL, .type = WEB_SOCKET_MESSAGE__TYPE__REQUEST};

  return UfsrvCommandInvokeUserCommand(
      &(InstanceContextForSession) {instance_sesn_ptr_originator, AS_UFSRV_SESSION(sesn_ptr_originator)},
      &(InstanceContextForSession) {instance_sesn_ptr_guardian, AS_UFSRV_SESSION(sesn_ptr_guardian)},
      &wsm_synthesised, NULL, &ufsrv_description, uSYNC_V1_IDX);
}

typedef struct SyncOutQueueDrainArgs {
  unsigned long uid;
  unsigned long eid;
} SyncOutQueueDrainArgs;

static void sDrainSyncOutQueue(unsigned long uid, unsigned long eid);

static UserCallBackReturnArgs *
sDrainSyncOutQueueJob(UserCallBackArgs *args_ptr, InstanceHolder *instance_holder_ptr)
{
  SyncOutQueueDrainArgs *drain_args_ptr = (SyncOutQueueDrainArgs *)args_ptr;

  sDrainSyncOutQueue(drain_args_ptr->uid, drain_args_ptr->eid);

  return NULL;
}

static void
sFinaliseSyncOutQueueJob(UserCallBackArgs *args_ptr)
{
  UserCallbackJobDescriptor *descriptor_ptr = AS_USER_CALLBACK_JOB_DESCRIPTOR(args_ptr);

  free(descriptor_ptr->callback_args);
  free(descriptor_ptr);
}

/**
 * @brief Ask the job pool to drain one sync out queue.
 *
 * @param uid User whose event the queue belongs to.
 * @param eid Event id of the staged record.
 *
 * @return @c true when the work was queued; the record stays queued otherwise.
 *
 * @dynamic_memory moves a drain descriptor to the job pool, released by its finaliser.
 */
bool
SyncCommandDeliverySignal(unsigned long uid, unsigned long eid)
{
  __attribute__((cleanup(DeliveryCleanupFree))) SyncOutQueueDrainArgs *drain_args_ptr = malloc(
    sizeof(SyncOutQueueDrainArgs));

  if (unlikely(IS_EMPTY(drain_args_ptr))) return false;

  drain_args_ptr->uid = uid;
  drain_args_ptr->eid = eid;

  __attribute__((cleanup(DeliveryCleanupFree))) UserCallbackJobDescriptor *descriptor_ptr = calloc(
    1, sizeof(UserCallbackJobDescriptor));

  if (unlikely(IS_EMPTY(descriptor_ptr))) return false;

  descriptor_ptr->handler       = sDrainSyncOutQueueJob;
  descriptor_ptr->finaliser     = sFinaliseSyncOutQueueJob;
  descriptor_ptr->callback_args = drain_args_ptr;

  if (!QueueInUserCallBackJob(descriptor_ptr)) {
    syslog(LOG_DEBUG, "%s {pid:'%lu', uid:'%lu', eid:'%lu'}: ERROR: JOB POOL NOT ACCEPTING SYNC WORK",
           __func__, pthread_self(), uid, eid);

    return false;
  }

  descriptor_ptr = NULL;
  drain_args_ptr = NULL;

  return true;
}

static void
sDrainSyncOutQueue(unsigned long uid, unsigned long eid)
{
  size_t delivered = 0;

  for (;;) {
    StagedMessageDescriptor staged_message_descriptor = {0};
    UFSRVResult *res_ptr = ExtractSyncOutQueueMessage(uid, eid, &staged_message_descriptor);

    if (_RESULT_CODE_EQUAL(res_ptr, RESCODE_BACKEND_DATA_EMPTYSET)) {
      syslog(LOG_DEBUG, "%s {pid:'%lu', uid:'%lu', eid:'%lu', delivered:'%zu'}: NOTICE: SYNC OUT QUEUE DRAINED",
             __func__, pthread_self(), uid, eid, delivered);

      return;
    }

    if (!_RESULT_TYPE_SUCCESS(res_ptr) && (_RESULT_CODE_EQUAL(res_ptr, RESCODE_BACKEND_CONNECTION)
                                           || _RESULT_CODE_EQUAL(res_ptr, RESCODE_BACKEND_RESOURCE_NULL))) {
      return;
    }

    if (!_RESULT_TYPE_SUCCESS(res_ptr)) continue;

    UFSRVResult *delivery_res_ptr = sDeliverStagedSyncCommand(&staged_message_descriptor);

    bool delivered_ok = IS_PRESENT(delivery_res_ptr) && _RESULT_TYPE_SUCCESS(delivery_res_ptr);

    unsigned long failed_uid = (staged_message_descriptor.payload.uids_sz > 0
                                && IS_PRESENT(staged_message_descriptor.payload.uids))
                                 ? staged_message_descriptor.payload.uids[0] : uid;

    CollectionDescriptor failed_uids_collection = {
        .collection             = AS_COLLECTION_TYPE(&failed_uid),
        .collection_sz          = delivered_ok ? 0 : 1,
        .collection_base_offset = sizeof(unsigned long)
    };

    int failure_rescode = IS_PRESENT(delivery_res_ptr) ? (int)_RESULT_CODE(delivery_res_ptr)
                                                       : (int)RESCODE_PROG_NULL_POINTER;

    sSyncCommandDeliveryLogAttempt(uid, eid, staged_message_descriptor.attempts, 1, &failed_uids_collection,
                                   delivered_ok ? (int)RESCODE_PROG_NULL_POINTER : failure_rescode);

    if (!delivered_ok && staged_message_descriptor.attempts + 1 < SYNC_OUT_QUEUE_MAX_ATTEMPTS) {
      StagedMessageDescriptor retry_descriptor = staged_message_descriptor;

      retry_descriptor.attempts = staged_message_descriptor.attempts + 1;

      UFSRVResult *restage_res_ptr = StoreSyncOutQueueMessage(uid, &retry_descriptor);

      if (_RESULT_TYPE_SUCCESS(restage_res_ptr)) {
        SyncCommandDeliverySignal(uid, eid);
      } else {
        syslog(LOG_DEBUG, "%s {pid:'%lu', uid:'%lu', eid:'%lu', rescode:'%d'}: ERROR: COULD NOT RE-STAGE SYNC COPY",
               __func__, pthread_self(), uid, eid, _RESULT_CODE(restage_res_ptr));
      }
    }

    if (!delivered_ok) {
      syslog(LOG_DEBUG, "%s {pid:'%lu', uid:'%lu', eid:'%lu', rescode:'%d'}: ERROR: COULD NOT DELIVER STAGED SYNC COPY",
             __func__, pthread_self(), uid, eid, failure_rescode);
    } else {
      delivered++;
    }

    DeliveryFreeExtractedRecord(&staged_message_descriptor);
  }
}
