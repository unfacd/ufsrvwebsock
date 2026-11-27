/*

 Copyright (c) 2015-2026 unfacd works

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 */

#include "command_controllers/utils_user_command.h"
#include "command_controllers/fence_command_controller.h"
#include <ufsrv_core/cache_backend/redis.h>
#include <ufnetcorelib/ufnet_msgcore/fence/fence.h>
#include <ufnetcorelib/ufnet_msgcore/fence/fence_cachbackend_commands_literal.h>
#include <ufnetcorelib/ufnet_core/user_prefs/user_prefs_fence.h>

#include "utils_fence_command.h"
extern __thread ThreadContext ufsrv_thread_context;

static UFSRVResult *_ResetMemberFencesForUser(InstanceHolderForSession *instance_sesn_ptr, unsigned long fence_call_flags);
static UFSRVResult *_ResetInvitedFencesForUser(InstanceHolderForSession *instance_sesn_ptr, unsigned long fence_call_flags);
static UFSRVResult *_CacheBackendCleanUpStaleMemberFenceRecordsForUser(Session *sesn_ptr_carrier, unsigned long userid, CollectionDescriptor *collection_ptr, CollectionDescriptor *by_collection_ptr, unsigned long call_flags_fence);

/**
 * 	@brief: Main interface for reseting user's fence membership with full network visibility, as if the user initiated the command.
 * 	To ensure proper clean up, the backend is queried first and locall memory resident is cross checked at the end.
 */
UFSRVResult *
ResetFencesForUser(InstanceHolderForSession *instance_sesn_ptr, EnumFenceCollectionType collection_type)
{
  switch (collection_type)
  {
    case MEMBER_FENCES:
      return _ResetMemberFencesForUser(instance_sesn_ptr, 0);

    case INVITED_FENCES:
      return _ResetInvitedFencesForUser(instance_sesn_ptr, 0);

    case BLOCKED_FENCES:
    case LIKED_FENCES:
    case FAVED_FENCES:
    case ALL_FENCES:
    default:
      break;
  }

  return SESSION_RESULT_PTR(SessionOffInstanceHolder(instance_sesn_ptr));
}


/**
 * @brief: List processed is based on freshly fetched cachbackend record, but member-comparison is based on memory-loaded fence membership, with
 * the option of loading from caches backend where fence instance is not already loaded. However, existing fences are not reloaded.
 * This limits this function to stateful instances only.
 * 	@stack_overflow: watch out for VLA objects...
 * 	@dynamic_memory fence_collection.collection_t: IMPORTS and DEALLOCATES
 * 	@dynamic_memory byids_collection.collection_t: IMPORTS and DEALLOCATES
 */
static inline UFSRVResult *
_ResetMemberFencesForUser(InstanceHolderForSession *instance_sesn_ptr, unsigned long fence_call_flags)
{
  CollectionDescriptor	fence_collection = {0};
  CollectionDescriptor	byids_collection = {0};
  CollectionDescriptor	*fence_collection_ptr __attribute__((unused));

  Session *sesn_ptr = SessionOffInstanceHolder(instance_sesn_ptr);

  fence_collection_ptr = GetFenceCollectionForUser(sesn_ptr, &fence_collection, &byids_collection, MEMBER_FENCES);

  size_t			i;
  unsigned long	*fence_ids = (unsigned long *)fence_collection.collection;
  unsigned long	*fence_byids = (unsigned long *)byids_collection.collection;

  if (fence_collection.collection_sz == 0) {
    //no need as we get no allocation upon empty set
    //if (!(IS_EMPTY(fence_collection_ptr)))	DestructFenceCollection (&fence_collection, false);

    _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_BACKEND_DATA_EMPTYSET)
  }

  unsigned long fence_call_flags_search =	FENCE_CALLFLAG_SEARCH_BACKEND|FENCE_CALLFLAG_HASH_FENCE_LOCALLY|
                                           FENCE_CALLFLAG_ATTACH_USER_LIST_TO_FENCE|FENCE_CALLFLAG_KEEP_FENCE_LOCKED|FENCE_CALLFLAG_LOCK_FENCE_BLOCKING;

  size_t				fence_ids_orphaned_counter	=	0,
          fences_stale_counter				=	0;

  unsigned long fence_ids_orphaned[fence_collection.collection_sz];//fences for which no entity can be loaded
  unsigned long fences_ids_stale[fence_collection.collection_sz];	//fence standalone entity exists, but not on user's list
  unsigned long fence_byids_stale[fence_collection.collection_sz];	//fence standalone entity exists, but not on user's list

  CollectionDescriptor	fence_ids_orphaned_collection = {.collection=(collection_t **)fence_ids_orphaned, .collection_sz=0};
  CollectionDescriptor	fence_ids_stale_collection 		= {.collection=(collection_t **)fences_ids_stale, .collection_sz=0};
  CollectionDescriptor	fence_byids_collection 				= {.collection=(collection_t **)fence_byids_stale, .collection_sz=0};

  memset (fence_ids_orphaned, 0, sizeof fence_ids_orphaned);
  memset(fences_ids_stale, 0, sizeof fences_ids_stale);
  memset(fence_byids_stale, 0, sizeof fence_byids_stale);

  Fence *f_ptr;
  InstanceHolderForFence *instance_f_ptr;

  for (i=0; i<fence_collection.collection_sz; i++) {
    FindFenceById(sesn_ptr, fence_ids[i], fence_call_flags_search);//blocking lock
    instance_f_ptr = (InstanceHolderForFence *)SESSION_RESULT_USERDATA(sesn_ptr);
    bool fence_lock_already_owned = (SESSION_RESULT_CODE_EQUAL(sesn_ptr, RESCODE_PROG_LOCKED_BY_THIS_THREAD));

    if (unlikely(IS_EMPTY(instance_f_ptr))) {
      //only do this if fence not found. Error could be related to locking, but that is not treated
      if (SESSION_RESULT_CODE_EQUAL(sesn_ptr, RESCODE_FENCE_DOESNT_EXIST)|| SESSION_RESULT_CODE_EQUAL(sesn_ptr, RESCODE_BACKEND_RESOURCE_NULL)) {
        fence_ids_orphaned[fence_ids_orphaned_counter++]	=	fence_ids[i];
        continue;
      } else {
        syslog (LOG_ERR, "%s {pid:'%lu', o:'%p', cid:'%lu', uid:'%lu', fid:'%lu'}: LOCKING ERROR: COULD NOT LOAD FENCE REFERENCED IN USER'S MEMBERS LIST: NOT ADDING TO ORPHAN LIST", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), SESSION_USERID(sesn_ptr), fence_ids[i]);
      }
    }

    //>>> FENCE SHOULD BE IN RW LOCKED STATE
    f_ptr = FenceOffInstanceHolder(instance_f_ptr);

    NetworkRemoveUserFromFence(instance_sesn_ptr, &(InstanceContextForFence){instance_f_ptr, f_ptr}, NULL, LT_USER_INITIATED, CALLFLAGS_EMPTY);
    if (SESSION_RESULT_TYPE_ERROR(sesn_ptr)) {
      fences_ids_stale[fences_stale_counter]	=	FENCE_ID(f_ptr);
      fence_byids_stale[fences_stale_counter]	=	fence_byids[i];
      fences_stale_counter++;
    }

    if (!fence_lock_already_owned)	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, SESSION_RESULT_PTR(sesn_ptr));
  }

  fence_ids_orphaned_collection.collection_sz	=	fence_ids_orphaned_counter;
  if (fence_ids_orphaned_collection.collection_sz > 0) {
    //TODO: clean orphan backend entry
    syslog (LOG_ERR, "%s {pid:'%lu', o:'%p', cid:'%lu', uid:'%lu', count:'%lu'}: DATA ERROR: CLEANING ORPHANED FENCE IDS: NOT IMPLEMENTED", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), SESSION_USERID(sesn_ptr), fence_ids_orphaned_collection.collection_sz);
  }

  fence_ids_stale_collection.collection_sz	=	fences_stale_counter;
  fence_byids_collection.collection_sz			=	fences_stale_counter;
  if (fence_ids_stale_collection.collection_sz > 0) {
    //TODO: clean BACKEND collection for user  UF:<uid>
    syslog (LOG_ERR, "%s {pid:'%lu', o:'%p', cid:'%lu', uid:'%lu', count:'%lu'}: DATA ERROR: CLEANING STALE FENCE COLLECTION", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), SESSION_USERID(sesn_ptr), fence_ids_stale_collection.collection_sz);

    _CacheBackendCleanUpStaleMemberFenceRecordsForUser(sesn_ptr, SESSION_USERID(sesn_ptr), &fence_ids_stale_collection, &fence_byids_collection, FENCE_CALLFLAG_EMPTY);
  }

  DestructFenceCollection(&fence_collection, false);
  DestructFenceCollection(&byids_collection, false);

  return SESSION_RESULT_PTR(sesn_ptr);
}

static inline UFSRVResult *
_ResetInvitedFencesForUser(InstanceHolderForSession *instance_sesn_ptr, unsigned long fence_call_flags)
{
  CollectionDescriptor	fence_collection = {0};
  CollectionDescriptor	*fence_collection_ptr __attribute__((unused));

  Session *sesn_ptr = SessionOffInstanceHolder(instance_sesn_ptr);

  fence_collection_ptr = GetFenceCollectionForUser(sesn_ptr, &fence_collection, NULL, INVITED_FENCES);

  size_t			i;
  unsigned long	*fence_ids = (unsigned long *)fence_collection.collection;

  if (fence_collection.collection_sz == 0) {
    //no need as we get no allocation upon empty set
    //if (!(IS_EMPTY(fence_collection_ptr)))	DestructFenceCollection (&fence_collection, false);

    _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_BACKEND_DATA_EMPTYSET)
  }

  unsigned long fence_call_flags_search =	FENCE_CALLFLAG_SEARCH_BACKEND|FENCE_CALLFLAG_HASH_FENCE_LOCALLY|
                                           FENCE_CALLFLAG_ATTACH_USER_LIST_TO_FENCE|FENCE_CALLFLAG_KEEP_FENCE_LOCKED|FENCE_CALLFLAG_LOCK_FENCE_BLOCKING;

  size_t				fence_ids_orphaned_counter	=	0;
  unsigned long fence_ids_orphaned[fence_collection.collection_sz];
  CollectionDescriptor	fence_ids_orphaned_collection = {.collection=(collection_t **)fence_ids_orphaned, .collection_sz=0};

  size_t				sesn_fence_list_orphaned_counter	=	0;
  unsigned long sesn_fence_list_ids_orphaned[fence_collection.collection_sz];
  CollectionDescriptor	sesn_fence_list_ids_orphaned_collection = {.collection=(collection_t **)sesn_fence_list_ids_orphaned, .collection_sz=0};

  Fence *f_ptr;
  FenceStateDescriptor *fence_state_ptr;
  InstanceHolderForFence *instance_f_ptr;
  InstanceHolderForFenceStateDescriptor *instance_fstate_ptr;

  for (i=0; i<fence_collection.collection_sz; i++) {
    FindFenceById(sesn_ptr, fence_ids[i], fence_call_flags_search);//blocking lock requested
    instance_f_ptr = (InstanceHolderForFence *)SESSION_RESULT_USERDATA(sesn_ptr);
    bool lock_already_owned = (SESSION_RESULT_CODE_EQUAL(sesn_ptr, RESCODE_PROG_LOCKED_BY_THIS_THREAD));

    if (unlikely(IS_EMPTY(instance_f_ptr))) {
      //only do this if fence not found. Error could be related to locking, but that is not treated
      if (SESSION_RESULT_CODE_EQUAL(sesn_ptr, RESCODE_FENCE_DOESNT_EXIST) || SESSION_RESULT_CODE_EQUAL(sesn_ptr, RESCODE_BACKEND_RESOURCE_NULL)) {
        fence_ids_orphaned[fence_ids_orphaned_counter++]	=	fence_ids[i];

        continue;
      } else {
        syslog (LOG_ERR, "%s {pid:'%lu', o:'%p', cid:'%lu', uid:'%lu', fid:'%lu'}: LOCKING ERROR: COULD NOT LOAD FENCE REFERENCED IN USER'S MEMBERS LIST: NOT ADDING TO ORPHAN LIST", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), SESSION_USERID(sesn_ptr), fence_ids[i]);
      }
    }

    //>>> FENCE SHOULD BE IN RW LOCKED STATE

    f_ptr = FenceOffInstanceHolder(instance_f_ptr);

    if (!IS_PRESENT((instance_fstate_ptr = FindFenceStateInSessionFenceListByFenceId(sesn_ptr, SESSION_INVITED_FENCE_LIST_PTR(sesn_ptr), FENCE_ID(f_ptr))))) {
#ifdef __UF_TESTING
      syslog (LOG_ERR, "%s {pid:'%lu', o:'%p', cid:'%lu', uid:'%lu', fid:'%lu'}: DATA ERROR: COULD NOT LOAD FENCE STATE REFERENCED IN USER'S MEMBERS LIST", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), SESSION_USERID(sesn_ptr), fence_ids[i]);
#endif
      sesn_fence_list_ids_orphaned[sesn_fence_list_orphaned_counter++]	=	fence_ids[i];

      if (!lock_already_owned)	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, SESSION_RESULT_PTR(sesn_ptr));

      continue;
    }

    fence_state_ptr = FenceStateDescriptorOffInstanceHolder(instance_fstate_ptr);
    assert (f_ptr == FENCESTATE_FENCE(fence_state_ptr));

    //this draws an event, unlike when uninvited due to fence fence being joined
    FenceEvent fence_event = {0};	fence_event.event_type = EVENT_TYPE_FENCE_USER_UNINVITED;

    RemoveUserFromInvitedList(instance_sesn_ptr, fence_state_ptr, &fence_event, FENCE_CALLFLAG_WRITEBACK_DATA_TO_BACKEND);
    if (!lock_already_owned)	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, SESSION_RESULT_PTR(sesn_ptr));
  }

  fence_ids_orphaned_collection.collection_sz						=	fence_ids_orphaned_counter;
  sesn_fence_list_ids_orphaned_collection.collection_sz	=	sesn_fence_list_orphaned_counter;

  if (fence_ids_orphaned_collection.collection_sz > 0) {
    //TODO: clean orphan backend entry
    syslog(LOG_ERR, "%s {pid:'%lu', o:'%p', cid:'%lu', uid:'%lu', list_sz:'%lu'}: DATA ERRORS: COULD NOT LOAD FENCES FROM REGISTRY: CLEANUP NOT IMPLEMENTED", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), SESSION_USERID(sesn_ptr), fence_ids_orphaned_collection.collection_sz);
  }

  if (sesn_fence_list_ids_orphaned_collection.collection_sz > 0) {
    //TODO: clean orphan backend entry
    syslog(LOG_ERR, "%s {pid:'%lu', o:'%p', cid:'%lu', uid:'%lu', list_sz:'%lu'}: DATA ERRORS: COULD NOT LOAD FENCE STATES REFERENCED IN USER'S MEMBERS LIST: CLEANUP NOT IMPLEMENTED", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), SESSION_USERID(sesn_ptr), sesn_fence_list_ids_orphaned_collection.collection_sz);
  }

  DestructFenceCollection(&fence_collection, false);

  return SESSION_RESULT_PTR(sesn_ptr);
}


/**
 * 	@brief: the main interface for removing user from fence with proper network-wide semantics and broadcasting
 * 	@locked sesn_ptr
 * 	@locked f_ptr
 */
UFSRVResult *
NetworkRemoveUserFromFence(InstanceHolderForSession *instance_sesn_ptr, InstanceContextForFence *ctx_f_ptr, CommandContextData *context_ptr, EnumFenceLeaveType leave_type, unsigned long call_flags_fence)
{
  unsigned long eid;
  Session *sesn_ptr = SessionOffInstanceHolder(instance_sesn_ptr);
  Fence   *f_ptr    = ctx_f_ptr->f_ptr;

  if ((eid = RemoveUserFromMembersListForFence(instance_sesn_ptr, f_ptr, call_flags_fence)) != 0) {
    InstanceContextForSession instance_ctx = {instance_sesn_ptr, AS_UFSRV_SESSION(sesn_ptr)};
    MarshalFenceStateSyncForLeave(&instance_ctx, &instance_ctx, ctx_f_ptr, (DataMessage *)context_ptr, leave_type);
    return SESSION_RESULT_PTR(sesn_ptr);
  } else {
    syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p', fo:'%p', cid:'%lu', cname:'%s'): COULD NOT complete user removal from fence", __func__, pthread_self(), sesn_ptr, f_ptr, SESSION_ID(sesn_ptr), FENCE_CNAME(f_ptr));
  }

  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_FENCE_MEMBERSHIP)
}

/**
 * 	@brief: This performs cache backend cleanup, whilst catering for a specific usecase whereby the cachebackend has fence entries for user under UF:<uid>, but those fences have no
 * 	reciprocal relationships with the user, mostly based on memory based assessment. So, we go ahead and clean up those cachebackend entries
 * 	@param  collection_ptr: collection of fence ids
 */
static UFSRVResult *
_CacheBackendCleanUpStaleMemberFenceRecordsForUser(Session *sesn_ptr_carrier, unsigned long userid, CollectionDescriptor *fence_ids_collection_ptr, CollectionDescriptor *byids_collection_ptr, unsigned long call_flags_fence)
{
  PersistenceBackend	*pers_ptr		=	THREAD_CONTEXT_SESSION_CACHEBACKEND;
  unsigned long 			*fence_ids	= (unsigned long *)fence_ids_collection_ptr->collection_sz;
  unsigned long 			*byids			= (unsigned long *)byids_collection_ptr->collection_sz;
#define COMANDSET_PER_USER	2

  (*pers_ptr->send_command_multi)(sesn_ptr_carrier, pers_ptr, "MULTI");

  for (size_t i =0; i<fence_ids_collection_ptr->collection_sz; i++) {
    (*pers_ptr->send_command_multi)(sesn_ptr_carrier,  pers_ptr, REDIS_CMD_FENCE_USERS_LIST_REM, fence_ids[i], userid);
    (*pers_ptr->send_command_multi)(sesn_ptr_carrier,  pers_ptr, REDIS_CMD_USER_FENCE_LIST_REM, fence_ids[i], userid, byids[i]);
  }

  (*pers_ptr->send_command_multi)(sesn_ptr_carrier,  pers_ptr, "EXEC");

  size_t				i;
  size_t				actually_processed	=	COMANDSET_PER_USER*fence_ids_collection_ptr->collection_sz + 2;
  size_t				commands_successful	=	actually_processed;
  redisReply		*replies[actually_processed];

  for (i=0; i<actually_processed; i++) {
    if ((RedisGetReply((UfsrvSession *)sesn_ptr_carrier, pers_ptr, (void *)&replies[i])) != REDIS_OK) {
      commands_successful--;

      if ((replies[i] != NULL)) {
        syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p', cid:'%lu', idex:'%lu'): ERROR PROCESSING MULTI SET COMMAND. ERROR: '%s'", __func__, pthread_self(), sesn_ptr_carrier, SESSION_ID(sesn_ptr_carrier), i, replies[i]->str);
      } else {
        syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p', cid:'%lu', i:'%lu'): ERROR COULD ISSUE GET COMMAND: BACKEND CONNECTIVITY ERROR", __func__, pthread_self(), sesn_ptr_carrier, SESSION_ID(sesn_ptr_carrier), i);
      }
    }
  }//for

  //diagnostics
  if (commands_successful != actually_processed) {
    for (i=0; i<actually_processed; i++)	if (IS_PRESENT(replies[i]))	freeReplyObject(replies[i]);

    _RETURN_RESULT_SESN(sesn_ptr_carrier, NULL, RESULT_TYPE_ERR, RESCODE_BACKEND_DATA);
  }

  //verification block
  {
//the results are stored at last index EXEC_COMMAND_IDX: array corresponding with command-set size less MULTI/EXEC. Other idx locations are of reply type REDIS_REPLY_STATUS
#define EXEC_COMMAND_IDX actually_processed-1

    for (i=0; i<EXEC_COMMAND_IDX; i++)	if (IS_PRESENT(replies[i]))	freeReplyObject(replies[i]);

    if (unlikely(IS_EMPTY(replies[EXEC_COMMAND_IDX]))) {//idx for EXEC, which is last
      syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p', cid:'%lu'): ERROR: REDIS TRANSCTION ERROR: NULL COMMAND ARRAY RESPONSE", __func__, pthread_self(), sesn_ptr_carrier, SESSION_ID(sesn_ptr_carrier));

      _RETURN_RESULT_SESN(sesn_ptr_carrier, NULL, RESULT_TYPE_ERR, RESCODE_BACKEND_DATA)
    }

    if (replies[EXEC_COMMAND_IDX]->elements == actually_processed-2) {
      for (i=0; i<replies[EXEC_COMMAND_IDX]->elements; i++) {
        redisReply *redis_ptr_reply = replies[EXEC_COMMAND_IDX]->element[i];
        if (!(replies[EXEC_COMMAND_IDX]->element[0]->integer == 1))	syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p', cid:'%lu', fid:'%lu', idx:'%lu', error:'%s'): ERROR: ZREM Failed", __func__, pthread_self(), sesn_ptr_carrier, SESSION_ID(sesn_ptr_carrier), fence_ids[i], i, replies[EXEC_COMMAND_IDX]->element[0]->str);
      }

      freeReplyObject(replies[EXEC_COMMAND_IDX]);
    } else {
      //Only remaining element is EXEC
      syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p', cid:'%lu', dispatched:'%lu', received:'%lu', error:'%s'): ERROR: REDIS TRANSCTION ERROR: DISPATCHED/RECEIVED COMMANDS COUNT MISMATCH", __func__, pthread_self(), sesn_ptr_carrier, SESSION_ID(sesn_ptr_carrier), actually_processed-2, replies[EXEC_COMMAND_IDX]->elements, replies[EXEC_COMMAND_IDX]->str);
      if (IS_PRESENT(replies[EXEC_COMMAND_IDX]))	freeReplyObject(replies[EXEC_COMMAND_IDX]);

      _RETURN_RESULT_SESN(sesn_ptr_carrier, NULL, RESULT_TYPE_ERR, RESCODE_BACKEND_DATA)
    }
  }

  _RETURN_RESULT_SESN(sesn_ptr_carrier, NULL, RESULT_TYPE_SUCCESS, RESCODE_BACKEND_DATA)

#undef EXEC_COMMAND_IDX
#undef COMANDSET_PER_USER
}

