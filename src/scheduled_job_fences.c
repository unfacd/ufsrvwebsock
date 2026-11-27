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

#ifdef HAVE_CONFIG_UFSRVWEBSOCK_H
#include "config_ufsrvwebsock.h"
#include <uflib/utils_time.h>
#include <ufnetcorelib/ufnet_msgcore/session.h>
#endif

#include "protocol_websocket.h"
#include "main_protocols.h"
#include "scheduled_job_fences.h"
#include <ufsrv_core/system/log_message_literals.h>
#include <uflib/recycler/instance_type.h>
#include <uflib/adt/adt_hashtable.h>
#include <ufsrv_core/ufsrvresult_type.h>
#include <ufnetcorelib/ufnet_msgcore/fence/fence.h>
#include <ufnetcorelib/ufnet_msgcore/fence/typepool_context_data_fence_type.h>
#include <ufsrv_core/delegator_sessions/delegator_session_worker_thread.h>
#include <uflib/scheduled_jobs/scheduled_jobs.h>
#include <ufnetcorelib/ufnet_msgcore/type_providers/type_provider_fence.h>
#include "command_controllers/fence_command_controller.h"
#include <ufnetcorelib/ufnet_msgcore/type_providers/session_provider.h>
#include <ufnetcorelib/nportredird.h>

#include "ufsrv_core/ufsrv_config/generated/ufsrv_config_accessor_decls_generated.h"

extern __thread ThreadContext ufsrv_thread_context;

static UFSRVResult * _HandleOrphanGeoFenceCandidate(InstanceContextForSession *ctx_ptr_carrier, InstanceHolderForFence *, bool fence_lock_already_owned);
static UFSRVResult * _HandleOrphanUserFenceCandidate(InstanceContextForSession *ctx_ptr_carrier, InstanceHolderForFence *, bool fence_lock_already_owned);
static UFSRVResult *_HandleOrphanUserFenceWithInvitedMembers(InstanceContextForSession *ctx_ptr_carrier, InstanceHolderForFence *, bool fence_alreadyLocked);

/**
 * 	@locked f_ptr:
 * 	@unlocks f_ptr: f_ptr lock transferred to the destroying method
 */
static UFSRVResult *
_HandleOrphanGeoFenceCandidate(InstanceContextForSession *ctx_ptr_carrier, InstanceHolderForFence *instance_f_ptr, bool fence_lock_already_owned)
{
  Session *sesn_ptr_carrier = AS_SESSION(ctx_ptr_carrier->sesn_ptr);

  if (IsFenceReferencedByUsers(instance_f_ptr))	goto return_no_action;

  //this is invokes _DestructUserFence
  int rc = FenceReturnToRecycler(instance_f_ptr,
                                 (ContextData *)&((TypePoolContextDataFence){.is_fence_locked=fence_lock_already_owned, .sesn_ptr=sesn_ptr_carrier, .fence_data.instance_f_ptr=instance_f_ptr}),
                                 FENCE_CALLFLAG_WRITEBACK_TO_DBBACKEND|FENCE_CALLFLAG_UNLOCK_FENCE);

  if (likely(rc == 0))	_RETURN_RESULT_SESN(sesn_ptr_carrier, NULL, RESULT_TYPE_SUCCESS, RESCODE_FENCE_DESTRUCTED);

  return_no_action:
  _RETURN_RESULT_SESN(sesn_ptr_carrier, NULL, RESULT_TYPE_SUCCESS, RESCODE_PROG_NULL_POINTER)

}

/**
 * @param sesn_ptr_carrier: this is ufsrv system user preloaded with access context
 * @locked sesn_ptr_carrier:
 * @locked f_ptr:
 * @unlocks f_ptr: f_ptr lock transferred to the destroying method
 */
static UFSRVResult *
_HandleOrphanUserFenceCandidate(InstanceContextForSession *ctx_ptr_carrier, InstanceHolderForFence *instance_f_ptr, bool fence_lock_already_owned)
{
  Fence *f_ptr = FenceOffInstanceHolder(instance_f_ptr);

  if (IsFenceReferencedByInviteMembersOnly(f_ptr)) {
    return (_HandleOrphanUserFenceWithInvitedMembers (ctx_ptr_carrier, instance_f_ptr, fence_lock_already_owned));
  }

  Session *sesn_ptr_carrier = AS_SESSION(ctx_ptr_carrier->sesn_ptr);

  if (IsFenceReferencedByUsers(instance_f_ptr))	goto return_no_action;
  if (IsFenceSticky(f_ptr))							goto return_no_action;

  //this invokes _DestructUserFence
  unsigned long fence_call_flags = FENCE_CALLFLAG_EVICT_FROM_CACHEBACKEND|FENCE_CALLFLAG_UNLOCK_FENCE;

  int rc = FenceReturnToRecycler(instance_f_ptr,
                                 (ContextData *)&((TypePoolContextDataFence){.is_fence_locked=fence_lock_already_owned, .sesn_ptr=sesn_ptr_carrier, .fence_data.instance_f_ptr=instance_f_ptr}),
                                 fence_call_flags);

  if (likely(rc == 0))	_RETURN_RESULT_SESN(sesn_ptr_carrier, NULL, RESULT_TYPE_SUCCESS, RESCODE_FENCE_DESTRUCTED)

  return_no_action:
  _RETURN_RESULT_SESN(sesn_ptr_carrier, NULL, RESULT_TYPE_SUCCESS, RESCODE_PROG_NULL_POINTER)
}

/**
 * 	@locks sesn_ptr_invited:
 * 	@unlocks sesn_ptr_invited: but this gets locked again downstream at marshaling
 * 	@unlocks f_ptr: f_ptr lock transferred to the destroying method
 */
static UFSRVResult *
_HandleOrphanUserFenceWithInvitedMembers(InstanceContextForSession *ctx_ptr_carrier, InstanceHolderForFence *instance_f_ptr, bool fence_lock_already_owned)
{
  size_t 			dangling_users_counter  = 0;

  dangling_users_counter = NetworkRemoveUsersFromInviteList(ctx_ptr_carrier, instance_f_ptr);

  Session *sesn_ptr_carrier = AS_SESSION(ctx_ptr_carrier->sesn_ptr);

  //this invokes _DestructUserFence
  if (dangling_users_counter == 0) {
    FenceReturnToRecycler(instance_f_ptr,
                          (ContextData *)&((TypePoolContextDataFence){.is_fence_locked=fence_lock_already_owned, .sesn_ptr=sesn_ptr_carrier, .fence_data.instance_f_ptr=instance_f_ptr}),
                          FENCE_CALLFLAG_EVICT_FROM_CACHEBACKEND|FENCE_CALLFLAG_UNLOCK_FENCE);
    _RETURN_RESULT_SESN(sesn_ptr_carrier, NULL, RESULT_TYPE_SUCCESS, RESCODE_FENCE_DESTRUCTED)
  }

  _RETURN_RESULT_SESN(sesn_ptr_carrier, NULL, RESULT_TYPE_SUCCESS, RESCODE_PROG_NULL_POINTER)
}

static ScheduledJobType * _GetScheduledJobTypeForOrphanedFences(void);

void
InitialiseScheduledJobTypeForOrphanedFences(void)
{
  RegisterScheduledJobType(GetScheduledJobsStore(), _GetScheduledJobTypeForOrphanedFences());
  InsertScheduledJob(GetScheduledJobsStore(), GetScheduledJobForOrphanedFences());
}

__attribute__ ((const)) static ScheduledJobType *
_GetScheduledJobTypeForOrphanedFences(void)
{
  static ScheduledJobType job_type_session_timeout = {
          .type_name				=	"Orphaned Fences",
          .type_id					=	0,//gets assigned by type registry
          .frequency_mode		=	PERIODIC,
          .concurrency_mode	=	SINGLE_INSTANCE,
          .frequency				=	_CONFIGDEFAULT_FENCE_ORPHANED_CHECK_FREQUENCY, //5min
          .callbacks					=	{
                  .on_compare_keys	= (CallbackOnCompareKeys)TimeValueComparator,
                  .on_error				=	NULL,
                  .on_run					=	(CallbackOnRun)CheckOrphanedFences,
                  .on_get_time    = GetTimeNowInMicros
          }
  };

  return &job_type_session_timeout;

}

/**
 * 	@brief: Since this job type does not allow concurrent scheduling, ie one job of this type can ever exist in the scheduler
 * 	we can get away with allocating a single static reference.
 */
ScheduledJob *
GetScheduledJobForOrphanedFences(void)
{
  static ScheduledJob job_orpahned_fences;

  job_orpahned_fences.job_type_ptr = _GetScheduledJobTypeForOrphanedFences();

  return &job_orpahned_fences;
}

//This is thread-safe as it is only accessed one thread at a time.
static size_t	_store_sz = _CONFIGDEFAULT_HASHTABLE_EXPANSION_SZ * 2;
static InstanceHolderForFence	*_LocalFencesStore[_CONFIGDEFAULT_HASHTABLE_EXPANSION_SZ * 2];
static atomic_bool isCheckOrphanedFencesRunning = ATOMIC_VAR_INIT(false);


/*
 * You don't need atomic_thread_fence() here because your critical sections start with acquire and end with release semantics.
 * Hence, reads within your critical sections can not be reordered prior to the acquire and writes post the release.
 * And this is why volatile is unnecessary here as well.
 */
int
CheckOrphanedFences(ScheduledJob *scheduled_job, void *arg)
{
  bool expected_running_is_false = false;

  if (!atomic_compare_exchange_strong_explicit(&isCheckOrphanedFencesRunning, &expected_running_is_false, true, memory_order_acq_rel, memory_order_relaxed)) {
    syslog(LOG_DEBUG, LOGSTR_UFSRVWORKER_ONETHREADONLY, __func__, pthread_self(), LOGCODE_UFSRVWORKER_ONETHREADONLY, "CheckOrphanedFences");
    return 0;
  }

  size_t	  i;
  int				numResults          = 0;
  bool			is_using_local_store= false;
  InstanceHolderForFence  **current_fences    = NULL;
  HashTable               *hash = FenceRegistryIdHashTableProvide();
  long long timer_start = GetTimeNowInMicros();

  if ((HashTable_RdLock(hash, 1)) != 0) {
    numResults = -1;
    goto return_with_value;
  }

  syslog(LOG_DEBUG, LOGSTR_CACHE_SIZE, __func__, pthread_self(), HASHTABLE_ENTRIES(hash), HASHTABLE_SIZE(hash), LOGCODE_CACHE_SIZE_SESSION, HASHTABLE_NAME(hash));

  statsd_gauge(THREAD_CONTEXT_INSTRUMENTATION_BACKEND, "worker.ufsrv.job.orphaned_fences.fences_global_size", (ssize_t)HASHTABLE_ENTRIES(hash), UfsrvConfigInstrumentationBackendServerAsPtr());

  UfsrvConfigRegisterUfsrverActivity(THREAD_CONTEXT_PERSISTENCE_CACHEBACKEND(THREAD_CONTEXT), time(NULL));

  if (HASHTABLE_SIZE(hash) == 0 || HASHTABLE_ENTRIES(hash) == 0) {
    HashTable_UnLock(hash);

    numResults = 0;
    goto return_with_value;
  }

  //mutate table so as not to hold it locked for too long
  if (HASHTABLE_ENTRIES(hash) < _store_sz) {
    current_fences = _LocalFencesStore;
    is_using_local_store = true;
  } else {
    current_fences = malloc((HASHTABLE_ENTRIES(hash) + 1) * sizeof(Fence *));
    *(current_fences + HASHTABLE_ENTRIES(hash)) = NULL; //terminate last entry for safety
  }

  for (i=0; i<HASHTABLE_SIZE(hash); i++) {
    if (hash->fTable[i] != NULL) {
      if (numResults + 1 > HASHTABLE_ENTRIES(hash)) {
        syslog(LOG_ERR, LOGSTR_CACH_INCONSIZE, __func__, pthread_self(), numResults+1, HASHTABLE_ENTRIES(hash), LOGCODE_CACHE_INCONSISTENT_SIZE, HASHTABLE_NAME(hash));
        break;
      }

      FenceIncrementReference((InstanceHolderForFence *)hash->fTable[i], 1);

      *(current_fences + (numResults++)) = (InstanceHolderForFence *)hash->fTable[i];
    }
  }

  HashTable_UnLock(hash);

  syslog(LOG_DEBUG, LOGSTR_CACHE_EXTRACTEDSET, __func__, pthread_self(), numResults, LOGCODE_CACHE_EXTRACTEDSET_SIZE, HASHTABLE_NAME(hash));

  if (numResults != HASHTABLE_ENTRIES(hash)) {
    syslog(LOG_ERR, "%s (pid:'%lu'): ERROR: DISCREPANCY between SCANNED '%d' and ACTUAL '%lu' FENCES.", __func__, pthread_self(), numResults, HASHTABLE_ENTRIES(hash));
  }

  //pool based allocation
  InstanceHolderForSession	*instance_sesn_ptr_carrier =	InstantiateCarrierSession(NULL, WORKERTYPE_UFSRVWORKER, CALL_FLAG_INSTANTIATE_FROM_SYSTEM_USER, UfsrvWebsockProtocol());
  if (IS_EMPTY(instance_sesn_ptr_carrier)) {
    syslog(LOG_ERR, "%s (pid:'%lu'): SEVERE ERROR: RECYCLER RETURNED NULL CARRIER SESSION OBJECT.", __func__, pthread_self());
    numResults = -2;
    goto return_with_value;
  }

  Session	*sesn_ptr_carrier = SessionOffInstanceHolder(instance_sesn_ptr_carrier);
  {
    int								result_code 				__unused;
    time_t						now								= time(NULL);

    //bool recycle_flag=false;
    statsd_gauge(THREAD_CONTEXT_INSTRUMENTATION_BACKEND, "worker.ufsrv.job.orphaned_fences.fences_collection_size", numResults, UfsrvConfigInstrumentationBackendServerAsPtr());

    bool fence_lock_already_owned = false;

    for (i=0; i<numResults; i++) {
      InstanceHolderForFence *instance_f_ptr = *(current_fences + i);
      Fence *f_ptr = FenceOffInstanceHolder(instance_f_ptr);

      if (IS_EMPTY(f_ptr)) {
        syslog(LOG_ERR, "%s (pid:'%lu', fo_instance:'%p', idx:'%lu'): !!!SEVERE ERROR: INSTANCE HOLDER CONTAINED NULL FENCE INSTANCE REFERENCE", __func__, pthread_self(), instance_f_ptr, i);
        FenceDecrementReference(instance_f_ptr, 1);
        continue;
      }

      FenceEventsLockRWCtx(THREAD_CONTEXT_PTR, f_ptr, _LOCK_TRY_FLAG_TRUE, THREAD_CONTEXT_UFSRV_RESULT(ufsrv_thread_context), __func__);
      if (_RESULT_TYPE_EQUAL(THREAD_CONTEXT_UFSRV_RESULT(ufsrv_thread_context), RESULT_TYPE_ERR)) {
        FenceDecrementReference(instance_f_ptr, 1);
        continue;
      }

      fence_lock_already_owned = (_RESULT_CODE_EQUAL(THREAD_CONTEXT_UFSRV_RESULT(ufsrv_thread_context), RESCODE_PROG_LOCKED_BY_THIS_THREAD));
      //>>>> FENCE NOW LOCKED

      //DO STUFF....
      FenceDecrementReference(instance_f_ptr, 1);
      InstanceContextForSession instance_ctx = {instance_sesn_ptr_carrier, AS_UFSRV_SESSION(sesn_ptr_carrier)};
      if (IsGeoFence(f_ptr))	_HandleOrphanGeoFenceCandidate(&instance_ctx, instance_f_ptr, fence_lock_already_owned);
      else 										_HandleOrphanUserFenceCandidate(&instance_ctx, instance_f_ptr, fence_lock_already_owned);

      result_code = SESSION_RESULT_CODE(sesn_ptr_carrier); //grab it whilst the session is still locked

      //>>>>>>>>>>>>>>>>>>>>>>>
      unlock_fence:
      if (result_code != RESCODE_FENCE_DESTRUCTED) {
        if (!fence_lock_already_owned)	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, THREAD_CONTEXT_UFSRV_RESULT(ufsrv_thread_context));//didn't get into recycler
      }
      //>>>>>>>>>>>>>>>>>>>>>>>
    }
  }

  if (!is_using_local_store)	free(current_fences);

  //statsd_gauge(pthread_getspecific(sessions_delegator_ptr->jobworkers.ufsrv_instrumentation_backend_key), "worker.ufsrv.job.orphaned_fences.fences_collection_size", 0);

  return_deallocate_carrier:
  SessionUnLoadEphemeralMode(sesn_ptr_carrier);
  ResetClonedUfsrvSystemUser(sesn_ptr_carrier, SESSION_CALLFLAGS_EMPTY);
  SessionReturnToRecycler(instance_sesn_ptr_carrier, (ContextData *)NULL, 0);

  return_with_value:
  if (!is_using_local_store)  {
    free(current_fences);
  }
  atomic_store_explicit(&isCheckOrphanedFencesRunning, false, memory_order_release);
  long long timer_end = GetTimeNowInMicros();

  statsd_timing(THREAD_CONTEXT_INSTRUMENTATION_BACKEND, "worker.ufsrv.job.orphaned_fences.elapsed_time", (timer_end-timer_start), UfsrvConfigInstrumentationBackendServerAsPtr());

  return numResults;

}
