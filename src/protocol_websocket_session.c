/**
 * Copyright (C) 2015-2026 unfacd works
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_UFSRVWEBSOCK_H
#include "config_ufsrvwebsock.h"
#endif

#include <uflib/recycler/recycler_type.h>
#include <ufnetcorelib/ufnet_msgcore/session.h>
#include "command_broadcast/command_broadcast_session.h"
#include "nportredird.h"
#include "protocol_websocket.h"
#include <ufsrv_core/cache_backend/redis.h>
#include <ufnetcorelib/ufnet_msgcore/user/user_backend.h>
#include <ufsrv_core/delegator_sessions/sessions_delegator_type.h>
#include "command_controllers/fence_command_controller.h"
#include "protocol_websocket_session.h"
#include "command_controllers/user_command_controller.h"
#include <uflib/ufsrvuid.h>
#include <ufnetcorelib/ufnet_msgcore/fence/fence_cachbackend_commands_literal.h>
#include <ufnetcorelib/ufnet_msgcore/fence/fence.h>

#include "utils_fence_command.h"

extern __thread ThreadContext     ufsrv_thread_context;

/**
 * 	@brief: Invalidating a session is a terminal action, whereby a session is withdrawn from the system and all associated fences
 * 	are disengaged. The Session could be in any mode: remote, connected etc...
 * 	We propagate the event to all fences accordingly  for which we hold connected sessions
 *
 * 	@locked sesn_ptr: by the caller environment
 * 	@unlocks sesn_ptr: as we are in the process of destorying the Session, we ough to unlock it
 *
 */
#if 0
inline static UFSRVResult *
_InvalidateLocalSessionReference (Session sesn_ptr, unsigned long call_flags)
{

	time_t time_now=time(NULL);

	ListEntry *eptr;
	for (eptr=SESSION_FENCE_LIST(sesn_ptr).head; eptr; eptr=eptr->next)
	{
		FenceStateDescriptor *fence_state_descriptor_ptr=(FenceStateDescriptor *)eptr->whatever;
		//FENCESTATE_FENCE(fence_state_descriptor);
		if (IsSessionConnected(sesn_ptr_aux, time_now))
		{

		}
	}
}
#endif

#include <uflib/adt/adt_distinct_array.h>

static char *
_UfsrvIdItemPrinter(ItemContainer *item_container)
{
  UfsrvUid *ufsrv_uid_ptr = AS_UFSRVUID(item_container);
  char ufsrvuid_serialised[CONFIG_MAX_UFSRV_ID_ENCODED_SZ + 1]; memset(ufsrvuid_serialised, '\0', CONFIG_MAX_UFSRV_ID_ENCODED_SZ + 1);

  UfsrvUidConvertSerialise(ufsrv_uid_ptr, ufsrvuid_serialised);
  syslog(LOG_INFO, "%s: UfsrvUid: '%s'", __func__, ufsrvuid_serialised);

  return NULL;
}

static void
_UfsrvIdItemInvalidator(void *ctx_ptr, uint8_t *item)
{
  InstanceHolderForSession *instance_sesn_ptr = ctx_ptr;

  MarshalUserInvalidated(&(InstanceContextForSession) {instance_sesn_ptr,
                                                       AS_UFSRV_SESSION(SessionOffInstanceHolder(instance_sesn_ptr))}, AS_CLIENT_CONTEXT_DATA(AS_UFSRVUID(item)));
}

DistinctArray *
InitUfsrvUidDistinctArray(DistinctArray * _Nonnull ufsrvuid_hashmap_ptr)
{
  if (HashTableInstantiate(&(ufsrvuid_hashmap_ptr->hashTable), HASHTABLE_ITEM_CONTAINER_OFFSET_ZERO, ufsrvuid_hashmap_ptr->distinct_array_descriptor.block_storage_unit_sz, HASH_ITEM_NOT_PTR_TYPE, "InvalidatedSessionHashMap", HASHTABLE_DEFAULT_EXTRACTOR)) {
    syslog(LOG_INFO, "%s: SUCCESS: InvalidatedSessionHashMap  Instantiated: key_offset: '%ld'. key_size: '%ld'", __func__, ufsrvuid_hashmap_ptr->hashTable.fKeyOffset, ufsrvuid_hashmap_ptr->hashTable.fKeySize);
    ufsrvuid_hashmap_ptr->hashTable.item_pretty_printer_callback = _UfsrvIdItemPrinter;

    return ufsrvuid_hashmap_ptr;
  } else {
    syslog(LOG_ERR, "%s: ERROR (errno: '%d'): COULD NOT INITIALISE InvalidatedSessionHashMap HashTable: TERMINATING...", __func__, errno);
    return NULL;
  }
}

void
CollectFenceUsers(DistinctArray * _Nonnull ufsrvuid_hashmap_ptr, Fence *f_ptr)
{
  FenceListIterator(&FENCE_USER_SESSION_LIST(f_ptr), ^(void *ctx_ptr){
    Session *sesn_ptr = SessionOffInstanceHolder(ctx_ptr);
      DistinctArrayPut(ufsrvuid_hashmap_ptr, &ufsrvuid_hashmap_ptr->stored_value_idx, (uint8_t **) &ufsrvuid_hashmap_ptr->value_block_storage, (uint8_t *) &SESSION_UFSRVUIDSTORE(sesn_ptr), CONFIG_MAX_UFSRV_ID_SZ);
  });
}


/**
 * 	@brief entry point into session invalidation based on Intra SessionMessage, specifying a collection of fences affected.
 * 	Upon successfully processing sesn_ptr will be sent to recycler. When processing through this proto, we'd be responding to a
 * 	an IntraMessage. <u>In this instance the api backend will have taken care of invalidating the backend cache for this Session.
 * 	So we purely perform local invalidation.</u>
 *
 * 	@sesn_ptr Session loaded in ephemeral mode. Could be connected or remote.
 *	@access_context lready loaded emphemeral
 *	@locking sesn_ptr and fences must be locked
 * 	@locked sesn_ptr: by the caller
 * 	@locks f_ptr each retrieved Fence is locked whilst being operated on
 * 	@unlocks f_ptr each retrieved Fence that was previously locked whilst being operated on gets unlockd
 *
 * 	@worker: UfsrvWorker
 */
UFSRVResult *
InvalidateLocalSessionReferenceFromProto(InstanceHolderForSession *instance_sesn_ptr, MessageQueueMessage *mqm_ptr, unsigned long call_flags)
{
	time_t 			time_now		= time(NULL);
	SessionMessage	*session_msg	= mqm_ptr->session;
  Session *sesn_ptr = SessionOffInstanceHolder(instance_sesn_ptr);

  DistinctArray ufsrvuid_hashmap = {.distinct_array_descriptor.block_storage_unit_sz=CONFIG_MAX_UFSRV_ID_SZ, .distinct_array_descriptor.storage_slot_offset=offsetof(struct VariableBlock, value)};
  InitUfsrvUidDistinctArray(&ufsrvuid_hashmap);

	syslog (LOG_DEBUG, "%s {pid:'%lu', o:'%p', fences_sz:'%lu'}: Invalidating Session",__func__, pthread_self(), sesn_ptr, session_msg->n_fences);

	if (session_msg->n_fences > 0) {
		size_t i = 0;
		for (; i<session_msg->n_fences; i++) {
			FindFenceById(sesn_ptr, mqm_ptr->session->fences[i]->fid, 0);
			InstanceHolder *instance_f_ptr_hashed = (InstanceHolder *)SESSION_RESULT_USERDATA(sesn_ptr);
			if (!(IS_EMPTY(instance_f_ptr_hashed))) {
			  Fence *f_ptr_hashed = FenceOffInstanceHolder(instance_f_ptr_hashed);

				FenceEventsLockRWCtx(THREAD_CONTEXT_PTR, f_ptr_hashed, _LOCK_TRY_FLAG_FALSE, SESSION_RESULT_PTR(sesn_ptr), __func__);
				if (SESSION_RESULT_TYPE_EQUAL(sesn_ptr, RESULT_TYPE_ERR)) continue;//lock error: this would indicate broader problem
				bool lock_already_owned = (SESSION_RESULT_CODE_EQUAL(sesn_ptr, RESCODE_PROG_LOCKED_BY_THIS_THREAD));

				Fence									*f_ptr_sesn_list	__attribute__((unused));
				InstanceHolderForFenceStateDescriptor *instance_fstate_ptr;

        instance_fstate_ptr = IsUserMemberOfThisFence(&(SESSION_FENCE_LIST(sesn_ptr)), f_ptr_hashed, false/*LOCK_FLAG*/);
				if (IS_EMPTY(instance_fstate_ptr)) {
					syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', fo:'%p', fid:'%lu'}: ERROR: INCONSISTENT FENCE STATE: FENCE NOT FOUND IN SESSION LIST",__func__, pthread_self(), sesn_ptr, f_ptr_hashed, mqm_ptr->session->fences[i]->fid);
					//continue; //we still need to remove the session from Fences list and self-heal
				}

        CollectFenceUsers(&ufsrvuid_hashmap, f_ptr_hashed);

        RemoveUserFromMembersListForFence(instance_sesn_ptr, f_ptr_hashed, CALL_FLAG_DONT_BROADCAST_FENCE_EVENT);//prevents generation of eid +broadcast
				f_ptr_hashed->fence_events.event_id = mqm_ptr->session->fences[i]->eid;
				InstanceContextForSession instance_ctx = {instance_sesn_ptr, AS_UFSRV_SESSION(sesn_ptr)};
				MarshalFenceStateSyncForLeave(&instance_ctx, &instance_ctx, &(InstanceContextForFence){instance_f_ptr_hashed, f_ptr_hashed}, NULL, LT_SESSION_INVALIDATED);

				if (!lock_already_owned)	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr_hashed, SESSION_RESULT_PTR(sesn_ptr));
			} else {
				syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', fid:'%lu'}: NOTICE: INCONSISTENT FENCE STATE: FENCE NOT FOUND IN LOCAL HASH",__func__, pthread_self(), sesn_ptr, mqm_ptr->session->fences[i]->fid);
			}
		}
	} else {
		syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p'}: NOTICE: FENCE LIST WAS EMPTY",__func__, pthread_self(), sesn_ptr);
	}

	if (session_msg->n_fences_invited > 0) {
		size_t i = 0;
		for (; i<session_msg->n_fences_invited; i++) {
			FindFenceById(sesn_ptr, mqm_ptr->session->fences_invited[i]->fid, 0);
      InstanceHolderForFence *instance_f_ptr_hashed = (InstanceHolderForFence *)SESSION_RESULT_USERDATA(sesn_ptr);

			if (!(IS_EMPTY(instance_f_ptr_hashed))) {
				Fence									*f_ptr_sesn_list __attribute__ ((unused));
//				FenceStateDescriptor	*f_state_ptr;

				Fence *f_ptr_hashed = FenceOffInstanceHolder(instance_f_ptr_hashed);

				FenceEventsLockRWCtx(THREAD_CONTEXT_PTR, f_ptr_hashed, _LOCK_TRY_FLAG_FALSE, SESSION_RESULT_PTR(sesn_ptr), __func__);
				bool lock_already_owned = (SESSION_RESULT_CODE_EQUAL(sesn_ptr, RESCODE_PROG_LOCKED_BY_THIS_THREAD));

				if (SESSION_RESULT_TYPE_EQUAL(sesn_ptr, RESULT_TYPE_ERR)) continue; //lock error: this would indicate broader problem

				InstanceHolderForFenceStateDescriptor *instance_fstate_ptr = IsUserMemberOfThisFence(&(SESSION_FENCE_LIST(sesn_ptr)), f_ptr_hashed, 0);
				if (IS_EMPTY(instance_fstate_ptr)) {
					syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', fo:'%p', fid:'%lu'}: ERROR: INCONSISTENT INVITED FENCE STATE: FENCE NOT FOUND IN SESSION LIST",__func__, pthread_self(), sesn_ptr, f_ptr_hashed, mqm_ptr->session->fences_invited[i]->fid);
					//continue; //we still need to remove the session from Fences list and selfheal
				}

        CollectFenceUsers(&ufsrvuid_hashmap, f_ptr_hashed);

        RemoveUserFromMembersListForFence(instance_sesn_ptr, f_ptr_hashed, CALL_FLAG_DONT_BROADCAST_FENCE_EVENT);//prevents generation of eid +broadcast
				f_ptr_hashed->fence_events.event_id = mqm_ptr->session->fences_invited[i]->eid;
        InstanceContextForSession instance_ctx = {instance_sesn_ptr, AS_UFSRV_SESSION(sesn_ptr)};
				MarshalFenceStateSyncForLeave(&instance_ctx, &instance_ctx, &(InstanceContextForFence){instance_f_ptr_hashed, f_ptr_hashed}, NULL, 0);

				if (!lock_already_owned)	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr_hashed, SESSION_RESULT_PTR(sesn_ptr));
			} else {
				syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', fid:'%lu'}: ERROR: INCONSISTENT INVITED FENCE STATE: FENCENOT FOUND IN HASH",__func__, pthread_self(), sesn_ptr, mqm_ptr->session->fences_invited[i]->fid);
			}
		}
	} else {
		syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p'}: NOTICE: INVITE LIST WAS EMPTY",__func__, pthread_self(), sesn_ptr);
	}

  DistinctArrayIterate(&ufsrvuid_hashmap, _UfsrvIdItemInvalidator, instance_sesn_ptr);

	//sesn_ptr already loaded in ephemeral mode with access context no need for CALL_FLAG_TRANSFER_WORKER_ACCESS_CONTEXT
	ClearLocalSessionCache(instance_sesn_ptr, CALL_FLAG_DONT_BROADCAST_FENCE_EVENT|CALL_FLAG_UNLOCK_SESSION);

  DistinctArrayDestruct(&ufsrvuid_hashmap);

	_RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_SUCCESS, RESCODE_SESN_INVALIDATED)
}



