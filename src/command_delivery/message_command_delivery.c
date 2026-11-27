/**
 * @file message_command_delivery.c
 * @brief message_command_delivery
 *
 * Copyright (C) 2015-2026  unfacd works
 *
 * This file is part of ufsrvwebsock source code.
 * Created by ayman on 15/11/2026.
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
#include "command_controllers/message_command_utils.h"
#include "command_controllers/message_command_controller.h"
#include "command_delivery/message_command_delivery.h"
#include <ufsrv_core/delegator_sessions/workers/user_callback_job.h>
#include <ufsrv_core/delegator_sessions/workers/user_callback_job_descriptor_type.h>
#include <ufnetcorelib/ufnet_msgcore/fence/fence.h>
#include <ufnetcorelib/ufnet_msgcore/fence/fence_staged_out_queue.h>
#include <ufnetcorelib/ufnet_msgcore/fence/fence_proto.h>
#include <ufnetcorelib/ufnet_msgcore/user/users_protobuf.h>
#include <ufnetcorelib/ufnet_msgcore/UnfacdService.pb-c.h>
#include <ufsrv_core/delegator_sessions/thread_context_type.h>
#include <ufnetcorelib/ufnet_msgcore/session.h>
#include <ufnetcorelib/ufnet_msgcore/type_providers/session_provider.h>
#include <uflib/lzf/lzf.h>
#include <uflib/utils.h>
#include <uflib/utils_time.h>
#include <ufnet_core/staged_message_descriptor_type.h>
#include <ufsrv_core/cache_backend/redis.h>
#include <uflib/main_types.h>
#include <uflib/collection_descriptor_type.h>
#include <stdarg.h>

extern __thread ThreadContext ufsrv_thread_context;

/**
 * Attempt log for the fence out queue, a rolling redis list per fence event.
 *
 * The delivery queue itself (@c FENCE_OUT_QUEUE:<fid>:<eid>) is consumed as it is drained, so it
 * records only what is still pending. This list records what every drain attempt did, and is
 * kept separate precisely so that queue consumption cannot erase the evidence.
 */
#define FENCE_OUT_QUEUE_LOG_CMD_ADD     "RPUSH FENCE_OUT_QUEUE_LOG:%lu:%lu %b"
#define FENCE_OUT_QUEUE_LOG_CMD_EXPIRE  "EXPIRE FENCE_OUT_QUEUE_LOG:%lu:%lu %u"

#define FENCE_OUT_QUEUE_LOG_TTL_SECONDS	(604800U)	///< retention window, refreshed on every append
#define FENCE_OUT_QUEUE_LOG_UID_DIGITS	(24U)		///< headroom per failed uid, in decimal digits
#define FENCE_OUT_QUEUE_LOG_ENTRY_EXTRA	(256U)		///< headroom for the fixed fields of an entry




/**
 * @brief Release an unpacked protobuf message at the end of the enclosing scope.
 *
 * @param data_msg_ptr Address of the message pointer; a NULL pointer or a NULL message is ignored.
 */
static void
sCleanupDataMessage(DataMessage **data_msg_ptr)
{
  if (IS_PRESENT(data_msg_ptr) && IS_PRESENT(*data_msg_ptr)) data_message__free_unpacked(*data_msg_ptr, NULL);
}

/**
 * @brief Release a fence target uid collection.
 */
void
MessageCommandFreeFenceTargetUids(CollectionResult *out)
{
  if (IS_EMPTY(out)) return;

  CollectionResultFree(ProvideUidListFenceMembersCollectionDescriptor(), out, NULL);
}

/**
 * @brief Fill a prepared message envelope from a received DataMessage.
 *
 * This is the single packaging site for a SAY command: the accept-time echo and the fan-out build
 * the identical envelope and differ only in @p command_arg and in where the envelope is sent.
 */
void
PrepareMarshalMessageForMessage(MarshalMessageEnvelopeForMessage *envelope_ptr, Session *sesn_ptr,
                                unsigned long fence_id, time_t fence_msg_expiry, FenceEvent *fence_event_ptr,
                                DataMessage *data_msg_ptr_orig, enum _MessageCommand__CommandTypes command_type,
                                enum _CommandArgs command_arg, bool attach_payload)
{
  MessageCommand *msgcmd_ptr = data_msg_ptr_orig->ufsrvcommand->msgcommand;

  envelope_ptr->envelope->ufsrvcommand            = envelope_ptr->ufsrv_command_wire;
  envelope_ptr->envelope->ufsrvcommand->msgcommand= envelope_ptr->message_command;
  envelope_ptr->envelope->ufsrvcommand->ufsrvtype = UFSRV_COMMAND_WIRE__UFSRV_TYPE__UFSRV_MESSAGE;
  envelope_ptr->envelope->ufsrvcommand->header    = envelope_ptr->header;

  envelope_ptr->message_command->header           = envelope_ptr->header;

  envelope_ptr->envelope->sourceufsrvuid          = "0";//ufsrv initiated origin
  envelope_ptr->envelope->timestamp               = GetTimeNowInMillis(); envelope_ptr->envelope->has_timestamp = 1;
  envelope_ptr->envelope->servertimestamp         = envelope_ptr->envelope->timestamp;
  envelope_ptr->envelope->has_servertimestamp     = 1;

  GetProtocolType(msgcmd_ptr, ^(Envelope__Type type) {
    envelope_ptr->envelope->type = type;
    envelope_ptr->envelope->has_type = 1;
  });

  envelope_ptr->header->when                      = envelope_ptr->envelope->timestamp; envelope_ptr->header->has_when = 1;
  envelope_ptr->header->when_client               = msgcmd_ptr->header->when; envelope_ptr->header->has_when_client = 1;
  envelope_ptr->header->fid                       = fence_id; envelope_ptr->header->has_fid = 1;
  envelope_ptr->header->command                   = command_type;
  envelope_ptr->header->args                      = command_arg; envelope_ptr->header->has_args = 1;

  if (msgcmd_ptr->header->has_args) {
    envelope_ptr->header->args_client = msgcmd_ptr->header->args; envelope_ptr->header->has_args_client = 1;
  }

  if (IS_PRESENT(fence_event_ptr)) {
    envelope_ptr->header->eid                     = fence_event_ptr->eid; envelope_ptr->header->has_eid = 1;
    envelope_ptr->header->gid                     = fence_event_ptr->gid; envelope_ptr->header->has_gid = 1;
    envelope_ptr->header->when_eid                = fence_event_ptr->when; envelope_ptr->header->has_when_eid = 1;
  }

  /* fence identity only: the record carries no pointer into the shared fence object */
  envelope_ptr->fence_record = ProvideFenceRecordInProtoAsIdentifierByParams(sesn_ptr, fence_id, envelope_ptr->fence_record);
  envelope_ptr->fence_records[0] = envelope_ptr->fence_record;
  envelope_ptr->message_command->fences          = envelope_ptr->fence_records;
  envelope_ptr->message_command->n_fences        = 1;

  envelope_ptr->fence_record->expire_timer       = fence_msg_expiry;
  envelope_ptr->fence_record->has_expire_timer   = 1;

  if (!attach_payload) return;

  envelope_ptr->message_command->originator      = ProvideUserRecordFromSessionInProto(sesn_ptr,
                                                     envelope_ptr->user_record_originator,
                                                     PROTO_USER_RECORD_MINIMAL, PROTO_USER_RECORD_BYREF);

  if (msgcmd_ptr->n_messages > 0) {
    envelope_ptr->message_command->messages      = msgcmd_ptr->messages;
    envelope_ptr->message_command->n_messages    = msgcmd_ptr->n_messages;
  }

  if (msgcmd_ptr->n_attachments > 0) {
    envelope_ptr->message_command->attachments   = msgcmd_ptr->attachments;
    envelope_ptr->message_command->n_attachments = msgcmd_ptr->n_attachments;
  }

  if (msgcmd_ptr->quoted_message != NULL) {
    envelope_ptr->message_command->quoted_message = msgcmd_ptr->quoted_message;
  }

  if (msgcmd_ptr->contacts != NULL && IsContactSharing(sesn_ptr, msgcmd_ptr->contacts)) {
    envelope_ptr->message_command->contacts      = msgcmd_ptr->contacts;
    envelope_ptr->message_command->n_contacts    = msgcmd_ptr->n_contacts;
  }

  if (msgcmd_ptr->preview != NULL) {
    envelope_ptr->message_command->preview       = msgcmd_ptr->preview;
    envelope_ptr->message_command->n_preview     = msgcmd_ptr->n_preview;
  }

  if (msgcmd_ptr->bodyranges != NULL) {
    envelope_ptr->message_command->bodyranges    = msgcmd_ptr->bodyranges;
    envelope_ptr->message_command->n_bodyranges  = msgcmd_ptr->n_bodyranges;
  }
}

/**
 * @brief Send one already-prepared message envelope to a recipient or to the originator.
 */
UFSRVResult *
MessageCommandDeliverEnvelopeToUser(InstanceContextForSession *ctx_sesn_ptr,
                                    InstanceHolderForSession *instance_sesn_ptr_target,
                                    WebSocketMessage *wsm_ptr_received, unsigned long fid,
                                    Envelope *command_envelope_ptr)
{
  Session *sesn_ptr        = AS_SESSION(ctx_sesn_ptr->sesn_ptr);
  Session *sesn_ptr_target = IS_PRESENT(instance_sesn_ptr_target) ? SessionOffInstanceHolder(instance_sesn_ptr_target) : NULL;

  CommandHeader *command_header_ptr = command_envelope_ptr->ufsrvcommand->header;
  command_header_ptr->cid           = SESSION_ID((sesn_ptr_target ? sesn_ptr_target : sesn_ptr));
  command_header_ptr->has_cid       = 1;

  UfsrvCommandMarshallingDescriptor ufsrv_description = {
      command_header_ptr->eid, fid, command_header_ptr->when, &EnvelopeMetaData, command_envelope_ptr,
      command_header_ptr->gid
  };

  return UfsrvCommandInvokeUserCommand(ctx_sesn_ptr,
                                       (IS_PRESENT(instance_sesn_ptr_target) ?
                                          (&(InstanceContextForSession) {instance_sesn_ptr_target, AS_UFSRV_SESSION(sesn_ptr_target)}) : NULL),
                                       IS_EMPTY(wsm_ptr_received) ?
                                          (&(WebSocketMessage) {.request=NULL, .type=WEB_SOCKET_MESSAGE__TYPE__REQUEST}) : wsm_ptr_received,
                                       NULL, &ufsrv_description, uMSG_V1_IDX);
}

/**
 * @brief Emit the originator's own accepted copy of a message command.
 */
UFSRVResult *
MessageCommandDeliverAcceptEcho(InstanceContextForSession *ctx_sesn_ptr, FenceEvent *fence_event_ptr,
                                DataMessage *data_msg_ptr_received, WebSocketMessage *wsm_ptr_received,
                                unsigned long fence_id, time_t fence_msg_expiry)
{
  Envelope          command_envelope = ENVELOPE__INIT;
  CommandHeader     header           = COMMAND_HEADER__INIT;
  UfsrvCommandWire  ufsrv_command    = UFSRV_COMMAND_WIRE__INIT;
  MessageCommand    message_command  = MESSAGE_COMMAND__INIT;
  FenceRecord       fence_record;
  FenceRecord      *fence_records[1];
  UserRecord        user_record_originator;

  MarshalMessageEnvelopeForMessage envelope_marshal = {
      .envelope               = &command_envelope,
      .ufsrv_command_wire     = &ufsrv_command,
      .message_command        = &message_command,
      .header                 = &header,
      .fence_record           = &fence_record,
      .fence_records          = fence_records,
      .user_record_originator = &user_record_originator,
  };

  PrepareMarshalMessageForMessage(&envelope_marshal, AS_SESSION(ctx_sesn_ptr->sesn_ptr), fence_id, fence_msg_expiry,
                                  fence_event_ptr, data_msg_ptr_received,
                                  MESSAGE_COMMAND__COMMAND_TYPES__SAY, COMMAND_ARGS__ACCEPTED, false);

  UFSRVResult *res_ptr = MessageCommandDeliverEnvelopeToUser(ctx_sesn_ptr, NULL, wsm_ptr_received, fence_id,
                                                             &command_envelope);

  DestructFenceRecordsProto(fence_records, message_command.n_fences, false, false);

  return res_ptr;
}

UFSRVResult *
MessageCommandDeliverToFence(const StagedMessageDescriptor *staged_message_descriptor_ptr, CollectionDescriptor *delivery_results_out)
{
  if (IS_EMPTY(staged_message_descriptor_ptr))                        THREAD_CONTEXT_RETURN_RESULT_ERROR(NULL, RESCODE_PROG_NULL_POINTER)
  if (IS_EMPTY(staged_message_descriptor_ptr->payload.payload_ptr))   THREAD_CONTEXT_RETURN_RESULT_ERROR(NULL, RESCODE_PROG_MISSING_PARAM)
  if (IS_EMPTY(staged_message_descriptor_ptr->payload.uids)
      || staged_message_descriptor_ptr->payload.uids_sz == 0)         THREAD_CONTEXT_RETURN_RESULT_ERROR(NULL, RESCODE_PROG_MISSING_PARAM)

  const unsigned long *snapshot_uids_ptr = staged_message_descriptor_ptr->payload.uids;
  size_t               snapshot_sz       = staged_message_descriptor_ptr->payload.uids_sz;

  if (IS_PRESENT(delivery_results_out)
      && (IS_EMPTY(delivery_results_out->collection) || delivery_results_out->collection_sz < snapshot_sz)) {
    syslog(LOG_DEBUG, "%s {pid:'%lu', fid:'%lu', eid:'%lu', results_sz:'%zu', snapshot:'%zu'}: ERROR: DELIVERY RESULT COLLECTION TOO SMALL: NOT RECORDING PER-USER STATE",
           __func__, pthread_self(), staged_message_descriptor_ptr->fid, staged_message_descriptor_ptr->eid,
           IS_PRESENT(delivery_results_out) ? delivery_results_out->collection_sz : 0, snapshot_sz);

    delivery_results_out = NULL;
  }

  /* every recipient starts STAGED; resolution and delivery overwrite their own slot, so a slot
     still STAGED at the end means this recipient was never reached, not that it failed */
  for (size_t i = 0; IS_PRESENT(delivery_results_out) && i < snapshot_sz; i++) {
    sStagedDeliveryResultStore(delivery_results_out, i, snapshot_uids_ptr[i], RESULT_TYPE_STAGED, RESCODE_PROG_NULL_POINTER);
  }

  size_t   raw_sz     = staged_message_descriptor_ptr->payload.raw_sz;
  __attribute__((cleanup(DeliveryCleanupFree))) uint8_t *packed_ptr = malloc(raw_sz);

  if (unlikely(IS_EMPTY(packed_ptr))) THREAD_CONTEXT_RETURN_RESULT_ERROR(NULL, RESCODE_PROG_MEMORY_EXHAUSTED)

  unsigned int packed_sz = lzf_decompress(staged_message_descriptor_ptr->payload.payload_ptr,
                                          (unsigned int)staged_message_descriptor_ptr->payload.payload_sz,
                                          packed_ptr, (unsigned int)raw_sz);

  if (unlikely(packed_sz != (unsigned int)raw_sz)) {
    THREAD_CONTEXT_RETURN_RESULT_ERROR(NULL, RESCODE_BACKEND_DATA_TRANSFORMATION)
  }

  __attribute__((cleanup(sCleanupDataMessage))) DataMessage *data_msg_ptr = data_message__unpack(NULL, packed_sz, packed_ptr);

  if (unlikely(IS_EMPTY(data_msg_ptr))) THREAD_CONTEXT_RETURN_RESULT_ERROR(NULL, RESCODE_PROG_INCONSISTENT_DATA)

  /* the fence out queue is shared with fence attribute changes, which are keyed by fence event id
     and carry a different command: never hand one of those to the message fan-out */
  if (unlikely(data_msg_ptr->ufsrvcommand == NULL
               || data_msg_ptr->ufsrvcommand->ufsrvtype != UFSRV_COMMAND_WIRE__UFSRV_TYPE__UFSRV_MESSAGE
               || data_msg_ptr->ufsrvcommand->msgcommand == NULL)) {
    syslog(LOG_DEBUG, "%s {pid:'%lu', fid:'%lu', eid:'%lu'}: ERROR: STAGED RECORD IS NOT A MESSAGE COMMAND",
           __func__, pthread_self(), staged_message_descriptor_ptr->fid, staged_message_descriptor_ptr->eid);

    THREAD_CONTEXT_RETURN_RESULT_ERROR(NULL, RESCODE_BACKEND_DATA_TRANSFORMATION)
  }

  //region recall previously generated event details
  FenceEvent fence_event = {0};
  fence_event.eid = staged_message_descriptor_ptr->eid;
  fence_event.gid = staged_message_descriptor_ptr->gid;
  //endregion

  size_t                       targets_sz = 0;
  __attribute__((cleanup(DeliveryCleanupFree))) InstanceHolderForSession **targets = calloc(snapshot_sz, sizeof(InstanceHolderForSession *));
  __attribute__((cleanup(DeliveryCleanupFree))) unsigned long *target_uids = calloc(snapshot_sz, sizeof(unsigned long));

  if (unlikely(IS_EMPTY(targets) || IS_EMPTY(target_uids))) {
    THREAD_CONTEXT_RETURN_RESULT_ERROR(NULL, RESCODE_PROG_MEMORY_EXHAUSTED)
  }

  unsigned long sesn_call_flags = (CALL_FLAG_HASH_SESSION_LOCALLY | CALL_FLAG_HASH_UID_LOCALLY |
                                   CALL_FLAG_HASH_USERNAME_LOCALLY | CALL_FLAG_ATTACH_FENCE_LIST_TO_SESSION |
                                   CALL_FLAG_REMOTE_SESSION);

  bool sender_lock_already_owned = false;

  //region get session for original sender
  GetSessionForThisUserByUserId(NULL, staged_message_descriptor_ptr->userid, &sender_lock_already_owned, sesn_call_flags);
  InstanceHolderForSession *instance_sesn_ptr_sender = THREAD_CONTEXT_UFSRV_RESULT_USERDATA;

  if (IS_EMPTY(instance_sesn_ptr_sender)) {
    THREAD_CONTEXT_RETURN_RESULT_ERROR(NULL, RESCODE_USER_INVALID_UID)
  }

  Session *sesn_ptr_sender = SessionOffInstanceHolder(instance_sesn_ptr_sender);
  // endregion

  // region collect sessions for target recipients
  for (size_t i=0; i<snapshot_sz; i++) {
    bool lock_already_owned = false;

    GetSessionForThisUserByUserId(NULL, snapshot_uids_ptr[i], &lock_already_owned, sesn_call_flags);
    InstanceHolderForSession *instance_sesn_ptr_target = THREAD_CONTEXT_UFSRV_RESULT_USERDATA;

    if (IS_EMPTY(instance_sesn_ptr_target)) {
      syslog(LOG_DEBUG, "%s {pid:'%lu', fid:'%lu', eid:'%lu', uid_target:'%lu'}: ERROR: TARGET SESSION NOT FOUND", __func__, pthread_self(), staged_message_descriptor_ptr->fid, staged_message_descriptor_ptr->eid, snapshot_uids_ptr[i]);

      sStagedDeliveryResultStore(delivery_results_out, i, snapshot_uids_ptr[i], RESULT_TYPE_ERR, RESCODE_USER_INVALID_UID);

      continue;
    }

    target_uids[targets_sz] = i;
    targets[targets_sz++]   = instance_sesn_ptr_target;
  }
  //endregion

  syslog(LOG_DEBUG, "%s {pid:'%lu', fid:'%lu', eid:'%lu', uid:'%lu', snapshot:'%zu', resolved:'%zu'}: NOTICE: DRAIN TARGET PLAN", __func__, pthread_self(), staged_message_descriptor_ptr->fid, staged_message_descriptor_ptr->eid, staged_message_descriptor_ptr->userid, snapshot_sz, targets_sz);

  CollectionDescriptor targets_collection = {
      .collection    = AS_COLLECTION_TYPE(targets),
      .collection_sz = targets_sz
  };
  CollectionDescriptor target_uids_collection = {
      .collection             = AS_COLLECTION_TYPE(target_uids),
      .collection_sz          = targets_sz,
      .collection_base_offset = sizeof(unsigned long)
  };

  UFSRVResult *res_ptr = MarshalMessageToFenceTargets(&(InstanceContextForSession){instance_sesn_ptr_sender, AS_UFSRV_SESSION(sesn_ptr_sender)},
                                                      NULL, data_msg_ptr, staged_message_descriptor_ptr->fid,
                                                      &fence_event, &targets_collection, &target_uids_collection,
                                                      delivery_results_out);

  return res_ptr;
}

/**
 * @brief Append one drain-attempt outcome to @c FENCE_OUT_QUEUE_LOG:<fid>:<eid>.
 *
 * The attempt log is a redis list in the fence cache backend, separate from the delivery queue
 * itself. Every drain attempt appends exactly one entry, so the delivery history of a fence event
 * can be reconstructed without syslog:
 *
 * @code
 * <attempt>:outcome:*:when:<epoch_ms>:worker:<tid>                                  all targets delivered
 * <attempt>:outcome:<m>-<n>:rescode:<code>:when:<epoch_ms>:worker:<tid>:uid:<uid>… n of m targets failed
 * @endcode
 *
 * @li @c attempt   zero-based drain attempt of the queue record being processed
 * @li @c m         target count of the drained queue record
 * @li @c n         count of failed targets, each appended as its own @c :uid: tag
 * @li @c code      rescode observed by the drain for this attempt. A success code on an entry
 *                  whose scalar is @c <m>-<n> means the failing targets were not attributed to a
 *                  cause (the silent-drop signature); once per-target reasons are propagated this
 *                  is the real cause, e.g. @c RESCODE_PROG_WONTLOCK.
 *
 * Writes are best effort and never change the delivery outcome: the entry is appended before the
 * queue record is consumed, a backend failure is logged and reported as @c false, and the thread
 * context result is left untouched for the caller. The list expires after
 * @c FENCE_OUT_QUEUE_LOG_TTL_SECONDS, so retention is time-based; the number of entries per event
 * is bounded by the retry invariant, not by the logger.
 *
 * @param staged_message_descriptor_ptr Drained record: @c fid, @c eid and the zero-based
 *                          @c attempts identify the log key and entry, and @c payload.uids_sz is
 *                          the target count @c m.
 * @param failed_uids_ptr   Collection of failed target uids: @c collection_sz is @c n and the uids
 *                          are carried by value, so @c collection_base_offset is
 *                          @c sizeof(unsigned long). NULL, or an empty collection, means every
 *                          target was delivered.
 * @param outcome_rescode   Rescode observed for this attempt; only emitted on failure entries.
 *
 * @return @c true when the entry was appended, @c false when it was skipped or the backend failed.
 *
 * @locks none
 */
bool
MessageCommandDeliveryLogAttempt(const StagedMessageDescriptor *staged_message_descriptor_ptr,
                                 const CollectionDescriptor *failed_uids_ptr, int outcome_rescode)
{
  if (unlikely(IS_EMPTY(staged_message_descriptor_ptr))) return false;

  return DeliveryLogAttemptAppend(THREAD_CONTEXT_FENCE_CACHEBACKEND, FENCE_OUT_QUEUE_LOG_CMD_ADD,
                                  FENCE_OUT_QUEUE_LOG_CMD_EXPIRE, "FENCE_OUT_QUEUE",
                                  staged_message_descriptor_ptr->fid, staged_message_descriptor_ptr->eid,
                                  staged_message_descriptor_ptr->attempts,
                                  staged_message_descriptor_ptr->payload.uids_sz, failed_uids_ptr, outcome_rescode);
}

#define FENCE_OUT_QUEUE_MAX_ATTEMPTS  (3)	///< total drain attempts per message, attempts are zero-based

typedef struct FenceOutQueueDrainArgs {
  unsigned long fid;
  unsigned long eid;
} FenceOutQueueDrainArgs;

static void sDrainFenceOutQueue(unsigned long fid, unsigned long eid);

static UserCallBackReturnArgs *
sDrainFenceOutQueueJob(UserCallBackArgs *args_ptr, InstanceHolder *instance_holder_ptr)
{
  FenceOutQueueDrainArgs *drain_args_ptr = (FenceOutQueueDrainArgs *)args_ptr;

  sDrainFenceOutQueue(drain_args_ptr->fid, drain_args_ptr->eid);

  return NULL;
}

static void
sFinaliseFenceOutQueueJob(UserCallBackArgs *args_ptr)
{
  UserCallbackJobDescriptor *descriptor_ptr = AS_USER_CALLBACK_JOB_DESCRIPTOR(args_ptr);

  free(descriptor_ptr->callback_args);
  free(descriptor_ptr);
}

bool
MessageCommandDeliverySignal(unsigned long fid, unsigned long eid)
{
  __attribute__((cleanup(DeliveryCleanupFree))) FenceOutQueueDrainArgs *drain_args_ptr = malloc(sizeof(FenceOutQueueDrainArgs));

  if (unlikely(IS_EMPTY(drain_args_ptr))) {
    syslog(LOG_DEBUG, "%s {pid:'%lu', fid:'%lu', eid:'%lu'}: ERROR: COULD NOT ALLOCATE DRAIN ARGS", __func__, pthread_self(), fid, eid);

    return false;
  }

  drain_args_ptr->fid = fid;
  drain_args_ptr->eid = eid;

  __attribute__((cleanup(DeliveryCleanupFree))) UserCallbackJobDescriptor *descriptor_ptr = calloc(1, sizeof(UserCallbackJobDescriptor));

  if (unlikely(IS_EMPTY(descriptor_ptr))) {
    syslog(LOG_DEBUG, "%s {pid:'%lu', fid:'%lu', eid:'%lu'}: ERROR: COULD NOT ALLOCATE DRAIN DESCRIPTOR", __func__, pthread_self(), fid, eid);

    return false;
  }

  descriptor_ptr->handler        = sDrainFenceOutQueueJob;
  descriptor_ptr->finaliser      = sFinaliseFenceOutQueueJob;
  descriptor_ptr->callback_args  = drain_args_ptr;

  if (!QueueInUserCallBackJob(descriptor_ptr)) {
    syslog(LOG_DEBUG, "%s {pid:'%lu', fid:'%lu', eid:'%lu'}: ERROR: JOB POOL NOT ACCEPTING WORK", __func__, pthread_self(), fid, eid);

    return false;
  }

  /* ownership moved to the job pool: the descriptor is released by sFinaliseFenceOutQueueJob() */
  descriptor_ptr = NULL;
  drain_args_ptr = NULL;

  return true;
}

static void
sDrainFenceOutQueue(unsigned long fid, unsigned long eid)
{
  size_t delivered = 0;

  for (;;) {
    StagedMessageDescriptor staged_message_descriptor = {0};
    UFSRVResult *res_ptr = ExtractFenceOutQueueMessage(fid, eid, &staged_message_descriptor);

    //region short-circuit request
    if (_RESULT_CODE_EQUAL(res_ptr, RESCODE_BACKEND_DATA_EMPTYSET)) {
      syslog(LOG_DEBUG, "%s {pid:'%lu', fid:'%lu', eid:'%lu', delivered:'%zu'}: NOTICE: FENCE OUT QUEUE DRAINED", __func__, pthread_self(), fid, eid, delivered);

      return;
    }

    if (!_RESULT_TYPE_SUCCESS(res_ptr) && (_RESULT_CODE_EQUAL(res_ptr, RESCODE_BACKEND_CONNECTION) || _RESULT_CODE_EQUAL(res_ptr, RESCODE_BACKEND_RESOURCE_NULL))) {
      syslog(LOG_DEBUG, "%s {pid:'%lu', fid:'%lu', eid:'%lu', delivered:'%zu'}: ERROR: COULD NOT EXTRACT FENCE OUT QUEUE MESSAGE", __func__, pthread_self(), fid, eid, delivered);

      return;
    }

    if (!_RESULT_TYPE_SUCCESS(res_ptr)) {
      syslog(LOG_DEBUG, "%s {pid:'%lu', fid:'%lu', eid:'%lu', rescode:'%d'}: ERROR: DISCARDED UNUSABLE FENCE OUT QUEUE RECORD", __func__, pthread_self(), fid, eid, _RESULT_CODE(res_ptr));

      continue;
    }
  // endregion

    __attribute__((cleanup(DeliveryCleanupFree))) StagedUserDeliveryResult *delivery_results = NULL;
    delivery_results = calloc(staged_message_descriptor.payload.uids_sz, sizeof(StagedUserDeliveryResult));

    CollectionDescriptor delivery_results_collection = {
        .collection             = AS_COLLECTION_TYPE(delivery_results),
        .collection_sz          = staged_message_descriptor.payload.uids_sz,
        .collection_base_offset = sizeof(StagedUserDeliveryResult)
    };

    UFSRVResult *delivery_res_ptr = MessageCommandDeliverToFence(&staged_message_descriptor, &delivery_results_collection);

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

    MessageCommandDeliveryLogAttempt(&staged_message_descriptor, &failed_uids_collection, failure_rescode);

    if (failed_sz > 0 && staged_message_descriptor.attempts + 1 < FENCE_OUT_QUEUE_MAX_ATTEMPTS) {
      StagedMessageDescriptor retry_descriptor = staged_message_descriptor;

      retry_descriptor.attempts              = staged_message_descriptor.attempts + 1;
      retry_descriptor.payload.uids          = batch_retry ? (unsigned long *)staged_message_descriptor.payload.uids : retry_uids;
      retry_descriptor.payload.uids_sz       = failed_sz;

      UFSRVResult *restage_res_ptr = StoreFenceOutQueueMessage(&retry_descriptor);

      if (_RESULT_TYPE_SUCCESS(restage_res_ptr)) {
        MessageCommandDeliverySignal(retry_descriptor.fid, retry_descriptor.eid);
      } else {
        syslog(LOG_DEBUG, "%s {pid:'%lu', fid:'%lu', eid:'%lu', rescode:'%d'}: ERROR: COULD NOT RE-STAGE FAILED TARGETS", __func__, pthread_self(), retry_descriptor.fid, retry_descriptor.eid, _RESULT_CODE(restage_res_ptr));
      }
    }

    if (_RESULT_TYPE_ERROR(delivery_res_ptr)) {
      syslog(LOG_DEBUG, "%s {pid:'%lu', fid:'%lu', eid:'%lu', rescode:'%d'}: ERROR: COULD NOT DELIVER STAGED MESSAGE", __func__, pthread_self(), fid, eid, _RESULT_CODE(delivery_res_ptr));
    } else {
      delivered++;
    }

    DeliveryFreeExtractedRecord(&staged_message_descriptor);

  }
}
