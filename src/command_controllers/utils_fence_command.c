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

#include "utils_fence_command.h"

#include <recycler/recycler.h>

#include "fence_command_controller.h"
#include <ufnetcorelib/ufnet_msgcore/json_attribute_names_fence.h>
#include "inter_broadcast_fence.h"
#include <ufnetcorelib/ufnet_msgcore/fence/fence.h>
#include <ufnetcorelib/ufnet_core/user_prefs/user_prefs_fence.h>
#include <ufnetcorelib/ufnet_msgcore/fence/utils_fence_db.h>
#include <ufnetcorelib/ufnet_msgcore/fence/utils_fence.h>
#include <ufnetcorelib/ufnet_msgcore/fence/fence_permission.h>
#include <ufnetcorelib/ufnet_msgcore/fence/fence_proto.h>
#include <ufnetcorelib/ufnet_msgcore/attachments.h>
#include <ufnetcorelib/ufnet_msgcore/fence/fence_cachbackend_commands_literal.h>
#include <ufsrv_core/cache_backend/redis.h>
#include "broadcast_context_data_fence_invite_type.h"
#include <ufnetcorelib/ufnet_msgcore/session.h>
#include <ufnetcorelib/ufnet_msgcore/type_providers/session_provider.h>
#include <ufnetcorelib/ufnet_msgcore/type_providers/type_provider_fence.h>
#include <ufnetcorelib/ufnet_msgcore/user/user_cachebackend.h>

#include "fence_command_controller_linkjoin.h"
#include "ufsrv_core/system/log_message_literals.h"
#include "ufnet_msgcore/utils_location.h"

extern __thread ThreadContext ufsrv_thread_context;

/**
 * 	@brief: This interprets the result code from a previous join request and marshals the enduser related  response. All model data
 * 	will have been changed by now.  Each result code carries a different meaning so data will be overloaded as necessary.
 *	@param fence_state_ptr: Fence must be wrapped in this, even if there no actual join involved The meaning will be taken from rescode
 * 	@param res_ptr: contains the outcome of a previous test on user's eligibility to joining given fence
 * 	@locked: Fence *: must be locked by the caller, except where res_ptr indicates certain errors
 * 	@unlocks Fence *:
 * 	@returns FenceStateDescriptor *, but can also return Fence *. see context below
 */
UFSRVResult *
HandleJoinFence(InstanceContextForSession *ctx_ptr, InstanceHolderForFenceStateDescriptor *instance_fstate_ptr, WebSocketMessage *wsm_ptr_received, DataMessage *data_msg_ptr_received, EnumFenceJoinType join_type, UFSRVResult *res_ptr)
{
  //>>>>>>>>f_ptr MAY BE LOCKED depending on return value on success only <<<<<<<<<<<
  FenceStateDescriptor *fstate_ptr = (FenceStateDescriptor *)GetInstance(instance_fstate_ptr);
  Fence *f_ptr = FENCESTATE_FENCE(fstate_ptr);
  Session *sesn_ptr = AS_SESSION(ctx_ptr->sesn_ptr);

  switch (res_ptr->result_code)
  {
    case RESCODE_USER_FENCE_FULL:
      _RETURN_RESULT_SESN(sesn_ptr, f_ptr, RESULT_TYPE_NOOP, RESCODE_USER_FENCE_FULL)

      //invite only + user not on invite list
    case RESCODE_FENCE_INVITATION_LIST:
      _RETURN_RESULT_SESN(sesn_ptr, f_ptr, RESULT_TYPE_ERR, RESCODE_FENCE_INVITATION_LIST)

    case RESCODE_USER_FENCE_WRITEOFF:
      _RETURN_RESULT_SESN(sesn_ptr, f_ptr, RESULT_TYPE_NOOP, RESCODE_USER_FENCE_WRITEOFF)

    case RESCODE_USER_FENCE_LOCATION:
      _RETURN_RESULT_SESN(sesn_ptr, f_ptr, RESULT_TYPE_NOOP, RESCODE_USER_FENCE_LOCATION)

    case RESCODE_USER_FENCE_KEY:
      _RETURN_RESULT_SESN(sesn_ptr, f_ptr, RESULT_TYPE_NOOP, RESCODE_USER_FENCE_KEY)

    case RESCODE_USER_FENCE_CANJOIN:
      //TODO: fence_state_ptr is actually the Fence * here... check return
      _RETURN_RESULT_SESN(sesn_ptr, (Fence *)res_ptr->result_user_data, RESULT_TYPE_NOOP, RESCODE_USER_FENCE_CANJOIN)

    case RESCODE_USER_FENCE_ALREADYIN:
      //this could be because the fence's SessionList was pre-loaded from  the backend. We just reannounce that to members
      //no further local state mods are done
#ifdef __UF_TESTING
      syslog (LOG_DEBUG, "%s (pid:'%lu', cid:'%lu', sesn_fence_list_sz:'%d', fence_ulist_sz:'%d'): WARNING: DOUBLE JOIN: 'uid:'%lu' IS ALREADY IN 'bid:%lu'",
					__func__, pthread_self(), SESSION_ID(sesn_ptr), SESSION_FENCE_LIST_SIZE(sesn_ptr), FENCE_SESSIONS_LIST_SIZE(f_ptr), SESSION_USERID(sesn_ptr), FENCE_ID(f_ptr));
#endif

      //TODO: given the potential for state being out for sync for others, we should send fence state synch to the whole fence, not just this user
      MarshalFenceStateSync(ctx_ptr, fstate_ptr, wsm_ptr_received, NULL, CALLFLAGS_EMPTY);

      _RETURN_RESULT_SESN(sesn_ptr, instance_fstate_ptr, RESULT_TYPE_ERR, RESCODE_USER_FENCE_ALREADYIN)

    case RESCODE_USER_FENCE_JOINED:
      if (_RESULT_TYPE_SUCCESS(res_ptr)) {
        //this is the ideal case
#ifdef __UF_TESTING
        syslog (LOG_DEBUG, "%s (pid:'%lu' cid:'%lu'): SUCCESS: Joining 'uid='%lu' into 'bid=%lu'", __func__, pthread_self(), SESSION_ID(sesn_ptr), UfsrvUidGetSequenceId(&SESSION_UFSRVUIDSTORE(sesn_ptr)), FENCE_ID(f_ptr));
#endif

        //set a copy aside, otherwise it gets overwritten by subsequent invocations on the Session
        //Fence *f_ptr_processed=(Fence *)_RESULT_USERDATA(res_ptr);
        UFSRVResult res = {.result_user_data=f_ptr, .result_code=res_ptr->result_code, .result_type=res_ptr->result_type};

        if (join_type == JT_GEO_BASED)						MarshalGeoFenceJoinToUser(ctx_ptr, NULL, fstate_ptr,   &((WebSocketMessage){0}), 0);
        else if (join_type == JT_USER_INITIATED || join_type == JT_LINK_BASED)	MarshalFenceJoinToUser(ctx_ptr, fstate_ptr, &((WebSocketMessage){.type=WEB_SOCKET_MESSAGE__TYPE__RESPONSE}),  data_msg_ptr_received);
        else if (join_type == JT_INVITED)					MarshalFenceJoinInvitedToUser(ctx_ptr, fstate_ptr, &((WebSocketMessage){0}),  data_msg_ptr_received);
        MarshalFenceStateSyncForJoin(ctx_ptr, sesn_ptr, FENCESTATE_INSTANCE_HOLDER(fstate_ptr), 0);

        _RETURN_RESULT_SESN(sesn_ptr, instance_fstate_ptr, RESULT_TYPE_SUCCESS, RESCODE_USER_FENCE_JOINED)
      } else {
        //TODO: unsupported condition
        //fence should not need unlocking under error condition
        _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_USER_FENCE_JOINED)
      }
      break;

    case RESCODE_LOGIC_EMPTY_RESOURCE:
    default:

      //fence should be unlocked/or could not be fetched as this is an error condition, for which the
      //called function automatically unlocks if applicable

      syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p', cid:'%lu', fo:'%p', fid:'%lu'):  INVALID RESCODE JOIN REQUEST...", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), f_ptr, FENCE_ID(f_ptr));
  }

  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_PROG_NULL_POINTER)

}

/**
 * 	@brief: Scans the location's geo. components (country, etc...) and infers if new base fences are to be added to the network.
 * 	This designed to work in response to location changes reported by user.
 * 	Current implementation joins user automatically to the lowest locality component, including all necessary fence creation.
 *
 * 	@locks RW f_ptr: For each fence that corresponds with a given location component
 * 	@unlocks f_ptr:
 *
 * 	@dynamic_memory: IMPORTS a 'char *' canonical_fence_name which is freed herein
 *
 * 	@returns RESCODE_USER_FENCE_JOINED: when a user is joined to fence
 * 	@returns FenceStatedescriptor * where user was joined, or already in
 */
UFSRVResult *
IsUserAllowedToJoinGeoFence(InstanceHolderForSession *instance_sesn_ptr, LocationDescription *ld_ptr)
{
  unsigned count_of_new_base_fences = 0;

  Session *sesn_ptr = SessionOffInstanceHolder(instance_sesn_ptr);

  ////create Intra fences as necessary
  count_of_new_base_fences = _CheckIntraBaseFencesAndAdd(sesn_ptr, ld_ptr, 0);

#ifdef __UF_TESTING
  syslog(LOG_DEBUG, "%s (pid:'%lu' cid:'%lu'): CREATED '%u' new IntraBaseFence", __func__, pthread_self(), sesn_ptr->ufsrv_session.session_id, count_of_new_base_fences );
#endif

  bool 				fence_already_locked			=	false;
  size_t cname_sz = SizeofCanonicalFenceName(sesn_ptr, NULL);
  char canonical_fence_name[cname_sz]; memset(canonical_fence_name, 0, sizeof(canonical_fence_name));

  Fence	*f_ptr								          = NULL;
  InstanceHolder  *instance_f_ptr       = NULL,
          *instance_fstate_ptr  = NULL;

  if (IS_EMPTY(_MakeCanonicalFenceNameForTriggeredZone(ld_ptr, SESSION_USERID(sesn_ptr), NULL, USER_ATTRIBUTE_IS_SET(sesn_ptr, USERATTRIBUTE_DEFINES_USERZONE), canonical_fence_name))) {
    syslog(LOG_ERR, "%s (pid:'%lu', o:'%p', cid:'%lu'): COULD NOT FORM CANONICAL NAME...", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr));

    _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_NOOP, RESCODE_PROG_NULL_POINTER)
  }

  //only local search
  if ((instance_f_ptr = FindBaseFenceByCanonicalName(sesn_ptr, canonical_fence_name, &fence_already_locked, FENCE_CALLFLAG_KEEP_FENCE_LOCKED))) {
    f_ptr = (Fence *)GetInstance(instance_f_ptr);

    UFSRVResult *res_ptr = _ProcessUserIsAllowedToJoinFence(instance_sesn_ptr, instance_f_ptr, NULL, FENCE_CALLFLAG_JOIN);

    //in case of logical error, we take care of unlocking
    if (_RESULT_TYPE_ERROR(res_ptr) && _RESULT_CODE_EQUAL(res_ptr, RESCODE_PROG_NULL_POINTER))	goto exit_no_result;

    HandleJoinFence(&(InstanceContextForSession){instance_sesn_ptr, AS_UFSRV_SESSION(sesn_ptr)}, (InstanceHolderForFenceStateDescriptor *)_RESULT_USERDATA(res_ptr), &(WebSocketMessage){.type=WEB_SOCKET_MESSAGE__TYPE__REQUEST, .request=NULL}, NULL, JT_GEO_BASED, res_ptr);
    if (SESSION_RESULT_CODE_EQUAL(sesn_ptr, RESCODE_USER_FENCE_ALREADYIN) || SESSION_RESULT_CODE_EQUAL(sesn_ptr, RESCODE_USER_FENCE_JOINED)) {
      instance_fstate_ptr = (InstanceHolder *) SESSION_RESULT_USERDATA(sesn_ptr);
      f_ptr = FENCESTATE_FENCE(((FenceStateDescriptor *)GetInstance(instance_fstate_ptr)));
    }
    else
      f_ptr = (Fence *)SESSION_RESULT_USERDATA(sesn_ptr);

    if (IS_PRESENT(f_ptr) && !fence_already_locked)	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, THREAD_CONTEXT_UFSRV_RESULT(THREAD_CONTEXT));
    return SESSION_RESULT_PTR(sesn_ptr);

    exit_no_result:
    if (!fence_already_locked)	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, SESSION_RESULT_PTR(sesn_ptr));
    _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_NOOP, RESCODE_PROG_NULL_POINTER)
  }

  //NOT RELEVANT ANYMORE //brand new base Fence. rare condition as intra above catches most
  syslog(LOG_ERR, "%s (pid:'%lu', o:'%p', cid:'%lu'): ERROR: FindBaseFenceByCanonicalName() returned NULL: something is seriously wrong", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr));

  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_PROG_NULL_POINTER)

}

/**
 * 	@brief: Resolve rules around user's ability to receive invitation to join a Geofence.
 * 	@param sesn_ptr_inviter: Could be another user, or a system user. Call flag will spcify
 * 	@param sesn_ptr: invited user
 *
 * 	@locked sesn_ptr: must be locked by the user
 * 	@locks f_ptr:
 * 	@unlocks fptr:
 *
 *	@returns Fence *:
 *
 * 	@call_flag FENCE_CALLFLAG_ROAMING_GEOFENCE: Operation as a result or roaming changes. Inviter is ufsrvsystem user
 */
UFSRVResult *
IsUserAllowedGeoFenceInvite(InstanceHolderForSession *instance_sesn_ptr, LocationDescription *location_description_ptr, Session *sesn_ptr_inviter, unsigned long fence_call_flags)
{
  bool 				fence_already_locked			=	false;
  Fence				*f_ptr										= NULL;
  InstanceHolderForFence *instance_f_ptr    = NULL;
  UFSRVResult *res_ptr									=	NULL;

  Session *sesn_ptr = SessionOffInstanceHolder(instance_sesn_ptr);

  ////create Intra fences as necessary
  _CheckIntraBaseFencesAndAdd(sesn_ptr, location_description_ptr, 0);

  size_t cname_sz = SizeofCanonicalFenceName(sesn_ptr, NULL);
  char canonical_fence_name[cname_sz]; memset (canonical_fence_name, 0, sizeof(canonical_fence_name));

  _MakeCanonicalFenceName(location_description_ptr, SESSION_USERID(sesn_ptr), NULL, USER_ATTRIBUTE_IS_SET(sesn_ptr, USERATTRIBUTE_DEFINES_USERZONE), canonical_fence_name);

  //only local search, as _CheckIntraBaseFencesAndAdd should have added fences as necessary
  if ((instance_f_ptr = FindBaseFenceByCanonicalName(sesn_ptr, canonical_fence_name, &fence_already_locked, FENCE_CALLFLAG_KEEP_FENCE_LOCKED))) {
    f_ptr = FenceOffInstanceHolder(instance_f_ptr);
    res_ptr = AddMemberToInvitedFenceList(instance_sesn_ptr, instance_f_ptr, sesn_ptr_inviter, FENCE_CALLFLAG_WRITEBACK_DATA_TO_BACKEND);
    if (_RESULT_TYPE_SUCCESS(res_ptr)) {
      unsigned long invited_eids[1];
      CollectionDescriptor invited_eids_collection = {(collection_t **)invited_eids, 1};
      invited_eids[0] = (uintptr_t)_RESULT_USERDATA(res_ptr);

      res_ptr = MarshalFenceInvitation(&(InstanceContextForSession){instance_sesn_ptr, AS_UFSRV_SESSION(sesn_ptr)}, instance_f_ptr, &((WebSocketMessage){{0}}), NULL, &invited_eids_collection, NULL, 0);


      if (!fence_already_locked)	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, SESSION_RESULT_PTR(sesn_ptr));

      if (_RESULT_TYPE_SUCCESS(res_ptr)) {_RETURN_RESULT_SESN(sesn_ptr, instance_f_ptr, RESULT_TYPE_SUCCESS, RESCODE_PROG_NULL_POINTER);}
      else goto exit_error;
    }

    if (!fence_already_locked)	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, SESSION_RESULT_PTR(sesn_ptr));
  }

  exit_error:
  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_PROG_NULL_POINTER)

}

/**
 * 	@brief: Main interface function for changing fence display name across all data stores
 * 	@dynamic_memory cname_new: INSTANTIATED and used by reference when successful, otherwise deallocated
 * 	@locked sesn_ptr: by caller
 * 	@locks Fence *: if FENCE_CALLFLAG_LOCK_FENCE is set
 * 	@unlocks Fence *: UNLESS  FENCE_CALLFLAG_KEEP_FENCE_LOCKED IS SET
 */
UFSRVResult *
IsUserAllowedToChangeFenceName(Session *sesn_ptr, FenceStateDescriptor *fence_state_ptr, const char *fname_new, unsigned long fence_call_flags, FenceEvent *fence_event_ptr_out)
{
  unsigned rescode;

  if (fence_call_flags&FENCE_CALLFLAG_LOCK_FENCE) {
    FenceEventsLockRWCtx(THREAD_CONTEXT_PTR, FENCESTATE_FENCE(fence_state_ptr), _LOCK_TRY_FLAG_FALSE, SESSION_RESULT_PTR(sesn_ptr), __func__);
    if (SESSION_RESULT_TYPE_EQUAL(sesn_ptr, RESULT_TYPE_ERR))	goto exit_lock_error;
  }

  bool fence_lock_already_owned = (SESSION_RESULT_CODE_EQUAL(sesn_ptr, RESCODE_PROG_LOCKED_BY_THIS_THREAD));

  if ((strncasecmp(fname_new, FENCE_DNAME(FENCESTATE_FENCE(fence_state_ptr)), CONFIG_MAX_FENCE_DESCRIPTION_SIZE) == 0))	goto exit_fname_identical;

  CheckFenceNameForValidity(sesn_ptr, FENCESTATE_FENCE(fence_state_ptr), fname_new);

  if (unlikely(SESSION_RESULT_TYPE_ERROR(sesn_ptr)))	goto exit_naming_error;

  FenceEvent *fence_event_ptr = RegisterFenceEvent(FENCESTATE_FENCE(fence_state_ptr), EVENT_TYPE_FENCE_DNAME,  NULL, 0/*LOCK_FLAG*/, fence_event_ptr_out);
  if (IS_PRESENT(fence_event_ptr)) {
#define REDIS_COMMAND_ARGV_SIZE 	6 //first 2 are for command and arg then for each attr/value pair
#define REDIS_COMMAND_ATTR_SIZE 	2
#define REDIS_COMMAND_VALUE_SIZE 	2

    fence_event_ptr->originator_ptr         = &SESSION_UFSRVUIDSTORE(sesn_ptr);
    fence_event_ptr->session_id	            =	SESSION_ID(sesn_ptr);
    char 	*cname_new;
    const char 	*attributes[REDIS_COMMAND_ATTR_SIZE] __unused, //fixed elements in redis
    *values[REDIS_COMMAND_VALUE_SIZE] __unused;		//actual new values

    const char	*combined[REDIS_COMMAND_ARGV_SIZE];		//actual storage pool for all the elements and values converted to str
    size_t 			combined_len[REDIS_COMMAND_ARGV_SIZE]	=	{0};	//actual storage pool for len of each converted str

    CollectionDescriptorPair collection_argv_argvlen	=	{
            .first={(collection_t **)combined, 			REDIS_COMMAND_ARGV_SIZE},
            .second={(collection_t **)combined_len, REDIS_COMMAND_ARGV_SIZE}
    };

    asprintf(&cname_new, "%s%s", FENCE_BASELOC(FENCESTATE_FENCE(fence_state_ptr)), fname_new);

    attributes[0] = "dname"; 		attributes[1] = "cname";
    values[0]		  = fname_new;	values[1]		  =	cname_new;

    CacheBackendUpdateFenceRegistry(sesn_ptr, FENCESTATE_FENCE(fence_state_ptr), fname_new, cname_new);
//Currently not in use because the above function takes care of all relevant cachebackend updates as one transaction
//so this is hold over just to demo how to use the CacheBackendSetFenceAttributesByCollection function as it has useful technique
//		CacheBackendSetFenceAttributesByCollection(sesn_ptr, FENCE_ID(FENCESTATE_FENCE(fence_state_ptr)),
//																							&((CollectionDescriptor){(collection_t **)attributes, REDIS_COMMAND_ATTR_SIZE}),
//																							&((CollectionDescriptor){(collection_t**)values, 			REDIS_COMMAND_VALUE_SIZE}),
//																							&collection_argv_argvlen);

    if (unlikely(SESSION_RESULT_TYPE_ERROR(sesn_ptr)))	{free (cname_new);	goto exit_backend_update_error;}

    exit_success:
    //update memory store. note the 'false' flag -> 'cname_new' ownership transferred, fname_new will be duplicated
    UpdateFenceNameAssignment(sesn_ptr, FENCESTATE_INSTANCE_HOLDER(fence_state_ptr), fname_new, cname_new, false, CALLFLAGS_EMPTY);

    DbBackendInsertUfsrvEvent((UfsrvEvent *)fence_event_ptr);

    //TODO: we my need to have a flag on this in case we dont want to broadcast
    InterBroadcastFenceDnameMessage(sesn_ptr, (ClientContextData *)FENCESTATE_FENCE(fence_state_ptr), fence_event_ptr, COMMAND_ARGS__UPDATED);

    if (fence_call_flags&FENCE_CALLFLAG_LOCK_FENCE) {
      if (!(fence_call_flags&FENCE_CALLFLAG_KEEP_FENCE_LOCKED))	if (!fence_lock_already_owned)	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, FENCESTATE_FENCE(fence_state_ptr), SESSION_RESULT_PTR(sesn_ptr));
    }

    //TODO: LOST LOCK OWNERSHIP
    _RETURN_RESULT_SESN(sesn_ptr, fence_event_ptr, RESULT_TYPE_SUCCESS, RESCODE_BACKEND_RESOURCE_UPDATED)
  }
  else	goto event_generation_error;


  exit_backend_update_error:
  rescode=SESSION_RESULT_CODE(sesn_ptr);
  goto exit_unlock;

  event_generation_error:
  rescode = SESSION_RESULT_CODE(sesn_ptr);
  goto exit_unlock;

  exit_naming_error:
  rescode = SESSION_RESULT_CODE(sesn_ptr);
  goto exit_unlock;

  exit_fname_identical:
  rescode = RESCODE_FENCE_IDENTICAL;
  goto exit_unlock;

  exit_unlock:
  if (fence_call_flags&FENCE_CALLFLAG_LOCK_FENCE) {
    if (!(fence_call_flags&FENCE_CALLFLAG_KEEP_FENCE_LOCKED))	if (!fence_lock_already_owned)	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, FENCESTATE_FENCE(fence_state_ptr), SESSION_RESULT_PTR(sesn_ptr));
  }
  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, rescode)

  exit_lock_error:
  rescode = SESSION_RESULT_CODE(sesn_ptr);
  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, rescode)

#undef REDIS_COMMAND_ARGV_SIZE
}

/**
 * 	@brief: Main interface function for changing fence message expiry across all model stores.
 * 	@locked sesn_ptr: by caller
 * 	@locks Fence *: if FENCE_CALLFLAG_LOCK_FENCE is set
 * 	@unlocks Fence *: UNLESS FENCE_CALLFLAG_KEEP_FENCE_LOCKED IS SET. Done regardless of error.
 */
UFSRVResult *
IsUserAllowedToChangeFenceMessageExpiry(Session *sesn_ptr, FenceStateDescriptor *fence_state_ptr, time_t msg_expiry_in_seconds, unsigned long fence_call_flags, FenceEvent *fence_event_ptr_out)
{
  unsigned 							rescode;

  if (fence_call_flags&FENCE_CALLFLAG_LOCK_FENCE) {
    FenceEventsLockRWCtx(THREAD_CONTEXT_PTR, FENCESTATE_FENCE(fence_state_ptr), _LOCK_TRY_FLAG_FALSE, SESSION_RESULT_PTR(sesn_ptr), __func__);
    if (SESSION_RESULT_TYPE_EQUAL(sesn_ptr, RESULT_TYPE_ERR))	goto exit_lock_error;
  }

  bool fence_lock_already_owned = (SESSION_RESULT_CODE_EQUAL(sesn_ptr, RESCODE_PROG_LOCKED_BY_THIS_THREAD));

  FenceEvent *fence_event_ptr = RegisterFenceEvent(FENCESTATE_FENCE(fence_state_ptr), EVENT_TYPE_FENCE_EXPIRY,  NULL, 0/*LOCK_FLAG*/, fence_event_ptr_out);
  if (IS_PRESENT(fence_event_ptr)) {
    fence_event_ptr->originator_ptr         = &SESSION_UFSRVUIDSTORE(sesn_ptr);
    fence_event_ptr->session_id	            =	SESSION_ID(sesn_ptr);
    char msg_expiry[sizeof(UINT64_LONGEST_STR) + 2] = {0};
    ultoa(msg_expiry_in_seconds, msg_expiry, 10);

    CacheBackendSetFenceAttribute(sesn_ptr, FENCE_ID(FENCESTATE_FENCE(fence_state_ptr)), "expiry", msg_expiry);

    if (unlikely(SESSION_RESULT_TYPE_ERROR(sesn_ptr)))	goto exit_backend_update_error;

    exit_success:
    //update memory store
    FENCE_MSG_EXPIRY(FENCESTATE_FENCE(fence_state_ptr)) = msg_expiry_in_seconds;
    DbBackendInsertUfsrvEvent((UfsrvEvent *)fence_event_ptr);
    InterBroadcastFenceMsgExpiry(sesn_ptr, &((ContextDataPair){(ClientContextData *)fence_state_ptr, (ClientContextData *)NULL}), fence_event_ptr, COMMAND_ARGS__UPDATED);

    if (fence_call_flags & FENCE_CALLFLAG_LOCK_FENCE) {
      if (!(fence_call_flags & FENCE_CALLFLAG_KEEP_FENCE_LOCKED))	if (!fence_lock_already_owned)	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, FENCESTATE_FENCE(fence_state_ptr), SESSION_RESULT_PTR(sesn_ptr));
    }

    //TODO: LOST LOCK OWNERSHIP
    _RETURN_RESULT_SESN(sesn_ptr, fence_event_ptr, RESULT_TYPE_SUCCESS, RESCODE_BACKEND_RESOURCE_UPDATED)
  }
  else	goto event_generation_error;

  exit_backend_update_error:
  rescode = SESSION_RESULT_CODE(sesn_ptr);
  goto exit_unlock;

  event_generation_error:
  rescode = SESSION_RESULT_CODE(sesn_ptr);
  goto exit_unlock;

  exit_naming_error:
  rescode = SESSION_RESULT_CODE(sesn_ptr);
  goto exit_unlock;

  exit_validity_error:
  rescode = SESSION_RESULT_CODE(sesn_ptr);
  goto exit_unlock;

  exit_unlock:
  if (fence_call_flags & FENCE_CALLFLAG_LOCK_FENCE) {
    if (!(fence_call_flags & FENCE_CALLFLAG_KEEP_FENCE_LOCKED))	if (!fence_lock_already_owned)	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, FENCESTATE_FENCE(fence_state_ptr), SESSION_RESULT_PTR(sesn_ptr));
  }
  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, rescode)

  exit_lock_error:
  rescode = SESSION_RESULT_CODE(sesn_ptr);
  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, rescode)

}

/**
 * @brief Check if new fence description follows naming and presentation conventions.
 * @param sesn_ptr
 * @param fence_state_ptr
 * @param description_new if empty, the description will be desginated as empty
 * @param fence_call_flags
 * @param fence_event_ptr_out
 * @return
 */
UFSRVResult *  __attribute__((nonnull(1, 2, 5)))
IsUserAllowedToChangeFenceDescription(Session *sesn_ptr, FenceStateDescriptor *fence_state_ptr, const char *description_new, unsigned long fence_call_flags, FenceEvent *fence_event_ptr_out)
{
  unsigned rescode;

  if (fence_call_flags&FENCE_CALLFLAG_LOCK_FENCE) {
    FenceEventsLockRWCtx(THREAD_CONTEXT_PTR, FENCESTATE_FENCE(fence_state_ptr), _LOCK_TRY_FLAG_FALSE, SESSION_RESULT_PTR(sesn_ptr), __func__);
    if (SESSION_RESULT_TYPE_EQUAL(sesn_ptr, RESULT_TYPE_ERR))	goto exit_lock_error;
  }

  bool fence_lock_already_owned = (SESSION_RESULT_CODE_EQUAL(sesn_ptr, RESCODE_PROG_LOCKED_BY_THIS_THREAD));

  if (IS_STR_LOADED(description_new)) {
    if  (IS_STR_LOADED(FENCE_DESCRIPTION(FENCESTATE_FENCE(fence_state_ptr)))) {
      if ((strncasecmp(description_new, FENCE_DESCRIPTION(FENCESTATE_FENCE(fence_state_ptr)), CONFIG_MAX_FENCE_DESCRIPTION_SIZE) == 0))
        goto exit_fname_identical;//todo 7/20/24 devops: this doesn't accommodate emojies in description
    }
    size_t description_new_sz = strlen(description_new);
    if (description_new_sz > CONFIG_MAX_FENCE_DESCRIPTION_SIZE) goto exit_length_exceeded;
    if (description_new_sz == 1 && *description_new == CONFIG_DEFAULT_PREFS_CHAR_VALUE) goto exit_invalid_description;
  }

  DbBackendUpdateFenceDataNamedAttribute(FENCE_ID(FENCESTATE_FENCE(fence_state_ptr)), FENCE_JSONATTR_DESCRIPTION, description_new, &(DbOpDescriptor){0}, CALLFLAGS_EMPTY);
  if (THREAD_CONTEXT_UFSRV_RESULT_TYPE_ERR) goto exit_db_update_error;

  FenceEvent *fence_event_ptr = RegisterFenceEvent(FENCESTATE_FENCE(fence_state_ptr), EVENT_TYPE_FENCE_DESCRIPTION,  NULL, 0/*LOCK_FLAG*/, fence_event_ptr_out);
  if (IS_PRESENT(fence_event_ptr)) {
    fence_event_ptr->originator_ptr  = &SESSION_UFSRVUIDSTORE(sesn_ptr);
    fence_event_ptr->session_id	     =	SESSION_ID(sesn_ptr);
    UpdateFenceDescriptionAssignment(sesn_ptr, FENCESTATE_INSTANCE_HOLDER(fence_state_ptr), description_new, CALLFLAGS_EMPTY);

    DbBackendInsertUfsrvEvent((UfsrvEvent *)fence_event_ptr);

    InterBroadcastFenceDescriptionMessage(sesn_ptr, (ClientContextData *)FENCESTATE_FENCE(fence_state_ptr), fence_event_ptr, COMMAND_ARGS__UPDATED);

    if (fence_call_flags&FENCE_CALLFLAG_LOCK_FENCE) {
      if (!(fence_call_flags&FENCE_CALLFLAG_KEEP_FENCE_LOCKED))	if (!fence_lock_already_owned)	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, FENCESTATE_FENCE(fence_state_ptr), SESSION_RESULT_PTR(sesn_ptr));
    }

    //TODO: LOST LOCK OWNERSHIP
    _RETURN_RESULT_SESN(sesn_ptr, fence_event_ptr, RESULT_TYPE_SUCCESS, RESCODE_BACKEND_RESOURCE_UPDATED)
  }
  else	goto event_generation_error;


  exit_backend_update_error:
  rescode  =SESSION_RESULT_CODE(sesn_ptr);
  goto exit_unlock;

  event_generation_error:
  rescode = SESSION_RESULT_CODE(sesn_ptr);
  goto exit_unlock;

  exit_naming_error:
  rescode = SESSION_RESULT_CODE(sesn_ptr);
  goto exit_unlock;

  exit_fname_identical:
  rescode = RESCODE_FENCE_IDENTICAL;
  goto exit_unlock;

  exit_invalid_description:
  exit_length_exceeded:
  rescode = RESCODE_FENCE_NAMING;
  goto exit_unlock;

  exit_db_update_error:
  rescode = RESCODE_BACKEND_DATA;
  goto exit_unlock;

  exit_unlock:
  if (fence_call_flags&FENCE_CALLFLAG_LOCK_FENCE) {
    if (!(fence_call_flags&FENCE_CALLFLAG_KEEP_FENCE_LOCKED))	if (!fence_lock_already_owned)	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, FENCESTATE_FENCE(fence_state_ptr), SESSION_RESULT_PTR(sesn_ptr));
  }
  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, rescode)

  exit_lock_error:
  rescode = SESSION_RESULT_CODE(sesn_ptr);
  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, rescode)

#undef REDIS_COMMAND_ARGV_SIZE
}

static AttachmentDescriptor *_TEMPAttachmentDescriptorGetFromProto(Session *sesn_ptr, AttachmentPointer *attachment_record, size_t eid, AttachmentDescriptor *attachment_descriptor_ptr_in, bool flag_encode_key);

/**
 * 	THIS TEMPORARY READS OF ATTACHMENT FROM ATTACHMENTPOINTER AS OPPOSED TO ATTACHMENTRECORD.
 * 	See AttachmentDescriptorGetFromProto ()
 */
static AttachmentDescriptor *
_TEMPAttachmentDescriptorGetFromProto(Session *sesn_ptr, AttachmentPointer *attachment_record, size_t eid, AttachmentDescriptor *attachment_descriptor_ptr_in, bool flag_encode_key)
{
  if (unlikely(IS_EMPTY(attachment_record)))	return NULL;

  if (unlikely(attachment_record->size <= 0))	return NULL;

  if (unlikely(IS_EMPTY(attachment_record->ufid)))	return NULL;

  AttachmentDescriptor *attachment_ptr = NULL;

  if (!IS_EMPTY(attachment_descriptor_ptr_in))	attachment_ptr = attachment_descriptor_ptr_in;
  else 																					attachment_ptr = calloc(1, sizeof(AttachmentDescriptor));

  strncpy(attachment_ptr->id, attachment_record->ufid, MBUF-1);
  if (attachment_record->has_key) {
    memcpy(attachment_ptr->key, attachment_record->key.data, attachment_record->key.len>MBUF?MBUF:attachment_record->key.len);
    attachment_ptr->key_sz = attachment_record->key.len>MBUF?MBUF:attachment_record->key.len;

    if (flag_encode_key) {
      attachment_ptr->key_encoded = (char *)base64_encode((unsigned char *)attachment_ptr->key, attachment_ptr->key_sz, NULL);
    }
  }

  if (attachment_record->has_digest) {
    memcpy(attachment_ptr->digest, attachment_record->digest.data, attachment_record->digest.len>MBUF?MBUF:attachment_record->digest.len);
    attachment_ptr->digest_sz = attachment_record->digest.len>MBUF?MBUF:attachment_record->digest.len;

    if (flag_encode_key) {
      attachment_ptr->digest_encoded = (char *)base64_encode((unsigned char *)attachment_ptr->digest, attachment_ptr->digest_sz, NULL);
    }
  }

  size_t sz;
  if (IS_STR_LOADED(attachment_record->blurhash) && ((sz = strlen(attachment_record->blurhash)) <= CONFIGDEFAULT_MAX_IMAGE_BLURHASH_SZ)) {
    attachment_ptr->blurhash = strbufdup(attachment_record->blurhash,  sz);
  }

  if (IS_STR_LOADED(attachment_record->caption) && ((sz = strlen(attachment_record->caption)) <= CONFIGDEFAULT_MAX_IMAGE_CAPTION_SZ)) {
    attachment_ptr->caption = strbufdup(attachment_record->caption,  sz);
  }

  if (!IS_EMPTY(attachment_record->contenttype)) {
    size_t contenttype_sz = strlen(attachment_record->contenttype);
    strncpy(attachment_ptr->mime_type, attachment_record->contenttype, contenttype_sz>SBUF-1?SBUF-1:contenttype_sz);
  }

  attachment_ptr->width   = attachment_record->width;
  attachment_ptr->height  = attachment_record->height;
  attachment_ptr->size    = attachment_record->size;
  attachment_ptr->eid     = eid;

#if 1//__UF_TESTING
  syslog (LOG_DEBUG, "%s {pid:'%lu', o:'%p', cid:'%lu', id:'%s', key:'%s', mime:'%s', size:'%lu'}: Attachment... ", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr),
          attachment_ptr->id, attachment_ptr->key_encoded?attachment_ptr->key_encoded:"", attachment_ptr->mime_type, attachment_ptr->size);
#endif

  return attachment_ptr;

}

/**
 * 	@brief: Main interface function for changing fence avatar across all model stores.
 * 	Check UpdateFenceAvatarAssignment()which is invoked at fence creation time.
 * 	TODO: refactor UpdateFenceAvatarAssignment() so they are invoked from one function
 * 	TODO: Try and move away from exposing DataMessage at this level
 * 	@locked sesn_ptr: by caller
 * 	@locks Fence *: if FENCE_CALLFLAG_LOCK_FENCE is set
 * 	@unlocks Fence *: UNLESS FENCE_CALLFLAG_KEEP_FENCE_LOCKED IS SET. Done regardless of error.
 */
UFSRVResult *
IsUserAllowedToChangeFenceAvatar(Session *sesn_ptr, FenceStateDescriptor *fence_state_ptr, DataMessage *data_msg_ptr, unsigned long fence_call_flags, FenceEvent *fence_event_ptr_out)
{
  unsigned 							rescode;
  AttachmentDescriptor 	attachment_descriptor_out	=	{0};

  if (fence_call_flags&FENCE_CALLFLAG_LOCK_FENCE) {
    FenceEventsLockRWCtx(THREAD_CONTEXT_PTR, FENCESTATE_FENCE(fence_state_ptr), _LOCK_TRY_FLAG_FALSE, SESSION_RESULT_PTR(sesn_ptr), __func__);
    if (SESSION_RESULT_TYPE_EQUAL(sesn_ptr, RESULT_TYPE_ERR))	goto exit_lock_error;
  }

  bool fence_lock_already_owned = (SESSION_RESULT_CODE_EQUAL(sesn_ptr, RESCODE_PROG_LOCKED_BY_THIS_THREAD));

  CheckAvatarForValidityFromProto(sesn_ptr, FENCESTATE_FENCE(fence_state_ptr), data_msg_ptr);

  if (unlikely(SESSION_RESULT_TYPE_ERROR(sesn_ptr)))	goto exit_validity_error;

  if (IS_EMPTY(data_msg_ptr->group) || IS_EMPTY(data_msg_ptr->group->avatar)) {
    rescode = RESCODE_PROG_NULL_POINTER;
    goto exit_unlock;
  }

  AttachmentPointer 		*attachment_ptr				=	data_msg_ptr->group->avatar;

  if (IS_PRESENT(GetAttachmentDescriptorEphemeral(sesn_ptr, attachment_ptr->ufid, false, &attachment_descriptor_out))) goto exit_already_exists_error;

  if (_TEMPAttachmentDescriptorGetFromProto(sesn_ptr, attachment_ptr, 0/*eid*/, &attachment_descriptor_out, true/*encode_key*/)) {
    DbAttachmentStore(sesn_ptr, &attachment_descriptor_out, FENCE_ID(FENCESTATE_FENCE(fence_state_ptr)), 1);//ufsrv instance doesn't currently support lru-caching attachments
  }
  else goto exit_data_error;

  FenceEvent *fence_event_ptr = RegisterFenceEvent(FENCESTATE_FENCE(fence_state_ptr), EVENT_TYPE_FENCE_AVATAR,  NULL, 0/*LOCK_FLAG*/, fence_event_ptr_out);
  if (IS_PRESENT(fence_event_ptr)) {
    fence_event_ptr->originator_ptr         = &SESSION_UFSRVUIDSTORE(sesn_ptr);
    fence_event_ptr->session_id	            =	SESSION_ID(sesn_ptr);
    CacheBackendSetFenceAttribute(sesn_ptr, FENCE_ID(FENCESTATE_FENCE(fence_state_ptr)), "avatar", attachment_descriptor_out.id);

    if (unlikely(SESSION_RESULT_TYPE_ERROR(sesn_ptr)))	goto exit_backend_update_error;

    DbBackendInsertUfsrvEvent((UfsrvEvent *)fence_event_ptr);

    exit_success:
    //update memory store
    if (IS_STR_LOADED(FENCE_AVATAR(FENCESTATE_FENCE(fence_state_ptr))))	free(FENCE_AVATAR(FENCESTATE_FENCE(fence_state_ptr)));
    FENCE_AVATAR(FENCESTATE_FENCE(fence_state_ptr)) = strdup(attachment_descriptor_out.id);

    InterBroadcastFenceAvatarMessage(sesn_ptr,
                                     &((ContextDataPair){(ClientContextData *)FENCESTATE_FENCE(fence_state_ptr), (ClientContextData *)&attachment_descriptor_out}),
                                     fence_event_ptr, COMMAND_ARGS__UPDATED);

    if (fence_call_flags&FENCE_CALLFLAG_LOCK_FENCE) {
      if (!(fence_call_flags&FENCE_CALLFLAG_KEEP_FENCE_LOCKED))	if (!fence_lock_already_owned) FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, FENCESTATE_FENCE(fence_state_ptr), SESSION_RESULT_PTR(sesn_ptr));
    }

    //TODO: LOST LOCK OWNERSHIP
    _RETURN_RESULT_SESN(sesn_ptr, fence_event_ptr, RESULT_TYPE_SUCCESS, RESCODE_BACKEND_RESOURCE_UPDATED)
  } else	goto event_generation_error;


  exit_backend_update_error:
  rescode = SESSION_RESULT_CODE(sesn_ptr);
  goto exit_unlock;

  event_generation_error:
  rescode = SESSION_RESULT_CODE(sesn_ptr);
  goto exit_unlock;

  exit_naming_error:
  rescode = SESSION_RESULT_CODE(sesn_ptr);
  goto exit_unlock;

  exit_already_exists_error:
  syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', blocb_id:'%s'}: ERROR: ATTACHMENT ID ALREADY EXISTS", __func__, pthread_self(), sesn_ptr,  attachment_ptr->ufid);
  rescode = RESCODE_LOGIC_IDENTICAL_RESOURCE;
  goto exit_unlock;

  exit_data_error:
  rescode = RESCODE_PROG_INCONSISTENT_DATA;
  goto exit_unlock;

  exit_validity_error:
  rescode = SESSION_RESULT_CODE(sesn_ptr);
  goto exit_unlock;

  exit_unlock:
  if (fence_call_flags&FENCE_CALLFLAG_LOCK_FENCE) {
    if (!(fence_call_flags&FENCE_CALLFLAG_KEEP_FENCE_LOCKED))	if (!fence_lock_already_owned)	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, FENCESTATE_FENCE(fence_state_ptr), SESSION_RESULT_PTR(sesn_ptr));
  }
  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, rescode)

  exit_lock_error:
  rescode = SESSION_RESULT_CODE(sesn_ptr);
  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, rescode)

}
/**
 * 	@brief General population routine for newly created fences. These reflect values that are user-settable
 */

/**
 * 	@brief: Main interface function for changing fence message expiry across all model stores.
 * 	@locked sesn_ptr: by caller
 * 	@locks Fence *: by caller
 * 	@unlocks None
 */
UFSRVResult *
IsUserAllowedToChangeFenceMaxMembers(Session *sesn_ptr, FenceStateDescriptor *fence_state_ptr, int32_t maxmembers, unsigned long fence_call_flags, FenceEvent *fence_event_ptr_out)
{
  unsigned 							rescode;

  FenceEvent *fence_event_ptr = RegisterFenceEvent(FENCESTATE_FENCE(fence_state_ptr), EVENT_TYPE_FENCE_MAXMEMBERS,  NULL, 0/*LOCK_FLAG*/, fence_event_ptr_out);
  if (IS_PRESENT(fence_event_ptr)) {
    fence_event_ptr->originator_ptr         = &SESSION_UFSRVUIDSTORE(sesn_ptr);
    fence_event_ptr->session_id	            =	SESSION_ID(sesn_ptr);
    char maxmembers_setting[sizeof(UINT64_LONGEST_STR) + 2] = {0};
    ultoa(maxmembers, maxmembers_setting, 10);

    CacheBackendSetFenceAttribute(sesn_ptr, FENCE_ID(FENCESTATE_FENCE(fence_state_ptr)), "maxusers", maxmembers_setting);

    if (unlikely(SESSION_RESULT_TYPE_ERROR(sesn_ptr)))	goto exit_backend_update_error;

    exit_success:
    FENCE_MAX_MEMBERS(FENCESTATE_FENCE(fence_state_ptr)) = maxmembers;
    DbBackendInsertUfsrvEvent((UfsrvEvent *)fence_event_ptr);
    InterBroadcastFenceMaxMembers(sesn_ptr,
                                  &((ContextDataPair){(ClientContextData *)fence_state_ptr, (ClientContextData *)NULL}),
                                  fence_event_ptr, COMMAND_ARGS__UPDATED);

    _RETURN_RESULT_SESN(sesn_ptr, fence_event_ptr, RESULT_TYPE_SUCCESS, RESCODE_BACKEND_RESOURCE_UPDATED)
  }
  else	goto event_generation_error;

  exit_backend_update_error:
  rescode=SESSION_RESULT_CODE(sesn_ptr);
  goto exit_error;

  event_generation_error:
  rescode=SESSION_RESULT_CODE(sesn_ptr);
  goto exit_error;

  exit_error:
  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, rescode)

}

UFSRVResult *
IsUserAllowedToChangeFenceDeliveryMode(Session *sesn_ptr, FenceStateDescriptor *fence_state_ptr, int delivery_mode, unsigned long fence_call_flags, FenceEvent *fence_event_ptr_out)
{
  unsigned 							rescode;

  if (isFenceDeliveryModelEquals(FENCESTATE_FENCE(fence_state_ptr), (FenceRecord__DeliveryMode) delivery_mode)) {
    goto exit_same_delivery_mode;
  }

  FenceEvent *fence_event_ptr = RegisterFenceEvent(FENCESTATE_FENCE(fence_state_ptr), EVENT_TYPE_FENCE_DELIVERY_MODE,  NULL, 0/*LOCK_FLAG*/, fence_event_ptr_out);
  if (IS_PRESENT(fence_event_ptr)) {
    fence_event_ptr->originator_ptr         = &SESSION_UFSRVUIDSTORE(sesn_ptr);
    fence_event_ptr->session_id	            =	SESSION_ID(sesn_ptr);
    char attributes_setting[sizeof(UINT64_LONGEST_STR) + 2] = {0};
    uint64_t attributes = FENCE_ATTRIBUTES(FENCESTATE_FENCE(fence_state_ptr));
    SetFenceDeliveryModeFromProto(&attributes, (FenceRecord__DeliveryMode) delivery_mode);

    ultoa(attributes, attributes_setting, 10);

    CacheBackendSetFenceAttribute(sesn_ptr, FENCE_ID(FENCESTATE_FENCE(fence_state_ptr)), "type", attributes_setting);

    if (unlikely(SESSION_RESULT_TYPE_ERROR(sesn_ptr)))	goto exit_backend_update_error;

    exit_success:
    DbBackendInsertUfsrvEvent((UfsrvEvent *)fence_event_ptr);
    UpdateFenceDeliveryModeAssignment(sesn_ptr, FENCESTATE_FENCE(fence_state_ptr), delivery_mode, CALLFLAGS_EMPTY);

    InterBroadcastFenceDeliveryMode(sesn_ptr, &((ContextDataPair){(ClientContextData *)fence_state_ptr, (ClientContextData *)NULL}), fence_event_ptr, COMMAND_ARGS__UPDATED);

    _RETURN_RESULT_SESN(sesn_ptr, fence_event_ptr, RESULT_TYPE_SUCCESS, RESCODE_BACKEND_RESOURCE_UPDATED)
  }
  else	goto event_generation_error;

  exit_same_delivery_mode:
  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_LOGIC_IDENTICAL_RESOURCE)

  exit_backend_update_error:
  rescode=SESSION_RESULT_CODE(sesn_ptr);
  goto exit_error;

  event_generation_error:
  rescode = SESSION_RESULT_CODE(sesn_ptr);
  goto exit_error;

  exit_error:
  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, rescode)

}


/**
 * 	If called from within session worker thread sesn_ptr_this==sesn_ptr_target and should use backend context from session
 * 	if called from within ufsrv worker thread sesn_ptr_this==NULL and should user backend context from ufsrv worker
 *	As this can be called from session of ufsrv worker, sesn_ptr must have access context loaded
 *	@param fe_ptr_out: must be allocated by caller
 *
 * 	@locked: both session and fence events must be locked
 * 	TODO: OPTIMISE: remove conditionals and add add event type callbacks
 */
FenceEvent *
UpdateBackendFenceData(Session *sesn_ptr_target, Fence *f_ptr, void *user_dat, unsigned event_type, FenceEvent *fe_ptr_out)
{
  FenceEvent 					*fe_ptr							=	NULL;
  PersistenceBackend 	*pers_ptr						= THREAD_CONTEXT_SESSION_CACHEBACKEND;
  FenceCacheBackend		*fence_backend_ptr	=	THREAD_CONTEXT_FENCE_CACHEBACKEND;
  redisReply 					*redis_ptr					= NULL;

  if (event_type == EVENT_TYPE_FENCE_CREATED) {
    if (IS_EMPTY((fe_ptr = RegisterFenceEvent(f_ptr, event_type,  NULL, 0/*LOCK_FLAG*/, fe_ptr_out)))) {
      return NULL;
    }

    fe_ptr->originator_ptr         = &SESSION_UFSRVUIDSTORE(sesn_ptr_target);
    fe_ptr->session_id	            =	SESSION_ID(sesn_ptr_target);
    CacheBackendAddFenceRecord(sesn_ptr_target, f_ptr, FENCE_CALLFLAG_EMPTY);

    DbBackendInsertUfsrvEvent((UfsrvEvent *)fe_ptr);

    //Add detailed event entry
    //TODO: this should be more loged to db backend. No value in keepoing this memory resident
//    redis_ptr = (*pers_ptr->send_command)(sesn_ptr_target, REDIS_CMD_FENCE_EVENTS,
//                                                        FENCE_ID(f_ptr), 1UL,
//                                                        1UL, masterptr->serverid,
//                                                        SESSION_ID(sesn_ptr_target), time(NULL),
//                                                        SESSION_ID(sesn_ptr_target), FENCE_ID(f_ptr),
//                                                        event_type, "event");
//
//    if (redis_ptr)	freeReplyObject(redis_ptr);

    //avoid race condition between fence creation and user joining, as both are broadcast at nearly the same time, causing INETER processing race conditions
    if (false) InterBroadcastFenceMake(sesn_ptr_target, f_ptr, fe_ptr, 0/*enum _CommandArgs*/);

    return fe_ptr;

  } else if (event_type == EVENT_TYPE_FENCE_DESTROYED) {
    //todo implement EVENT_TYPE_FENCE_DESTROYED
  } else if (event_type == EVENT_TYPE_FENCE_USER_JOINED) {
    fe_ptr = RegisterFenceEvent(f_ptr, event_type,  NULL, 0/*LOCK_FLAG*/, fe_ptr_out);

    if (unlikely(IS_EMPTY(fe_ptr)))		return NULL;

    fe_ptr->originator_ptr         = &SESSION_UFSRVUIDSTORE(sesn_ptr_target);
    fe_ptr->session_id	           =	SESSION_ID(sesn_ptr_target);
    FenceStateDescriptor *fence_state_ptr_invite	=	user_dat;
    //fence_state_ptr_invite=IsUserMemberOfThisFence(SESSION_INVITED_FENCE_LIST_PTR(sesn_ptr_target), f_ptr, false/*FLAG_FENCE_LOCK_FALSE*/);

    time_t time_now = time(NULL);

    //ZADD  FUSERS:<bid>  timestamp> <uid>
    redis_ptr  = (*pers_ptr->send_command)(sesn_ptr_target, pers_ptr, REDIS_CMD_FENCE_USER_RECORD, FENCE_ID(f_ptr), time_now, SESSION_USERID(sesn_ptr_target));
    if (redis_ptr)	freeReplyObject(redis_ptr);

    unsigned long uid_invited_by = IS_PRESENT(fence_state_ptr_invite) ? UfsrvUidGetSequenceId(&(fence_state_ptr_invite->invited_by)) :
                                   (F_ATTR_IS_SET(f_ptr->attrs, F_ATTR_BASEFENCE)? 1 : 0);
    redis_ptr = (*pers_ptr->send_command)(sesn_ptr_target, pers_ptr, REDIS_CMD_USER_FENCE_LIST_ADD, SESSION_USERID(sesn_ptr_target), time_now, FENCE_ID(f_ptr), uid_invited_by);
    if (redis_ptr)	freeReplyObject(redis_ptr);

    //TODO: store in db backend
    //HMSET FEV:%lu:%lu sid %lu cid %lu when %lu oid %lu tid %lu evt %d ev %s
//		redis_ptr = (*pers_ptr->send_command)(sesn_ptr_target, REDIS_CMD_FENCE_EVENTS,
//																												FENCE_ID(f_ptr),
//																												fe_ptr->eid,
//																												fe_ptr->eid,
//																												masterptr->serverid,
//																												SESSION_ID(sesn_ptr_target),
//																												time(NULL),
//																												SESSION_ID(sesn_ptr_target),
//																												FENCE_ID(f_ptr),
//																												event_type,
//																												"event");
//
//		if (redis_ptr)	freeReplyObject(redis_ptr);

    DbBackendInsertUfsrvEvent((UfsrvEvent *)fe_ptr);

    InterBroadcastFenceJoin(sesn_ptr_target, (ClientContextData *)f_ptr, fe_ptr, 0/*enum _CommandArgs*/);

    return fe_ptr;

  } else if (event_type == EVENT_TYPE_FENCE_USER_PARTED) {
    FenceStateDescriptor *fence_state_for_invited_by_only	=	(FenceStateDescriptor *)user_dat;
    redisReply *redis_ptr;
    redis_ptr = (*pers_ptr->send_command)(sesn_ptr_target, pers_ptr, REDIS_CMD_FENCE_USERS_LIST_REM, FENCE_ID(f_ptr), SESSION_USERID(sesn_ptr_target));

    if (redis_ptr)	freeReplyObject(redis_ptr);

    //REDIS_CMD_USER_FENCE_REM
    redis_ptr=(*pers_ptr->send_command)(sesn_ptr_target, pers_ptr, REDIS_CMD_USER_FENCE_LIST_REM, SESSION_USERID(sesn_ptr_target), FENCE_ID(f_ptr), UfsrvUidGetSequenceId(&(fence_state_for_invited_by_only->invited_by)));

    if (redis_ptr)	freeReplyObject(redis_ptr);

    //this generates event data
    fe_ptr = BackendUpdateFenceEvent(sesn_ptr_target, &((FenceIdentifier){0, f_ptr}), fe_ptr_out, event_type);
    InterBroadcastFenceLeave(sesn_ptr_target, (ClientContextData *)f_ptr, fe_ptr, 0/*enum _CommandArgs*/);

    return fe_ptr;
  } else if (event_type == EVENT_TYPE_FENCE_MEMBERSHIP_REPAIRED) {
    if (IS_EMPTY((fe_ptr = RegisterFenceEvent(f_ptr, event_type,  NULL, 0/*LOCK_FLAG*/, fe_ptr_out)))) {
      return NULL;
    }

    fe_ptr->originator_ptr         = &SESSION_UFSRVUIDSTORE(sesn_ptr_target);
    fe_ptr->session_id	           =	SESSION_ID(sesn_ptr_target);

    redis_ptr = (*pers_ptr->send_command)(sesn_ptr_target,pers_ptr,  REDIS_CMD_FENCE_USERS_LIST_REM, FENCE_ID(f_ptr), (unsigned long)user_dat);

    if (IS_PRESENT(redis_ptr)) {
      freeReplyObject(redis_ptr);
      DbBackendInsertUfsrvEvent((UfsrvEvent *)fe_ptr);
      InterBroadcastFenceReload(sesn_ptr_target, (ClientContextData *)f_ptr, fe_ptr, COMMAND_ARGS__RESYNC);

      return fe_ptr;
    } else return NULL;
  } else {
    syslog(LOG_DEBUG, "%s {pid:'%lu', cid:'%lu', fid:'%lu', event_type:'%d'}: WARNING: Unknown event type",	__func__, pthread_self(), SESSION_ID(sesn_ptr_target), FENCE_ID(f_ptr), event_type);
    return NULL;
  }

  return fe_ptr;

}

/**
 * 	@brief: for each invited user we generate a corresponding unique network wide event.
 * 	Not all event types require generation of event id
 */
FenceEvent *
UpdateBackendFenceInvitedData(Session *sesn_ptr_inviter, Session *sesn_ptr_invited, FenceStateDescriptor *fstate_ptr, unsigned event_type, FenceEvent *fe_ptr_out)
{
  Session			*sesn_ptr;
  Fence				*f_ptr		= FenceOffInstanceHolder(fstate_ptr->instance_holder_fence);
  FenceEvent	*fe_ptr		= NULL;

  time_t							time_now;
  PersistenceBackend 	*pers_ptr;
  MessageQueueBackend *mq_ptr __unused;

  //user is being sent a join invitation
  if (event_type == EVENT_TYPE_FENCE_USER_INVITED) {
    sesn_ptr											= sesn_ptr_inviter;
    pers_ptr											= THREAD_CONTEXT_PERSISTENCE_CACHEBACKEND(THREAD_CONTEXT);//THREAD_CONTEXT_SESSION_CACHEBACKEND;
    mq_ptr												= THREAD_CONTEXT_MSGQUEUE_CACHEBACKEND;//THREAD_CONTEXT_MSGQUEUE_CACHEBACKEND;

    if (IS_PRESENT(fe_ptr_out))	fe_ptr = fe_ptr_out;
    else												fe_ptr = calloc(1, sizeof(FenceEvent));

    fe_ptr = RegisterFenceEvent(f_ptr, event_type,  NULL, 0/*LOCK_FLAG*/, fe_ptr);

    if (unlikely(IS_EMPTY(fe_ptr))) {
      if (IS_EMPTY(fe_ptr_out))	free(fe_ptr);
      return NULL;
    }

    fe_ptr->originator_ptr  = &SESSION_UFSRVUIDSTORE(sesn_ptr);
    fe_ptr->session_id	    =	SESSION_ID(sesn_ptr);
    time_now                = fe_ptr->when;

#ifdef __UF_TESTING
    syslog(LOG_DEBUG, "%s {pid:'%lu', cid:'%lu', target_fid:'%lu', eid:'%lu'}: FenceInvited Event ID: Generated", __func__, pthread_self(), fe_ptr->session_id, fe_ptr->ctx_id, fe_ptr->eid);
#endif

    (*pers_ptr->send_command_multi)(sesn_ptr, pers_ptr, "MULTI");

    (*pers_ptr->send_command_multi)(sesn_ptr, pers_ptr, REDIS_CMD_INVITED_FENCES_FOR_USER_ADD, SESSION_USERID(sesn_ptr_invited), time_now, FENCE_ID(f_ptr), SESSION_USERID(sesn_ptr_inviter));
    (*pers_ptr->send_command_multi)(sesn_ptr, pers_ptr, REDIS_CMD_INVITED_USERS_FOR_FERNCE_ADD, FENCE_ID(f_ptr), time_now,  SESSION_USERID(sesn_ptr_invited), SESSION_USERNAME(sesn_ptr_invited), SESSION_USERID(sesn_ptr_inviter));
    (*pers_ptr->send_command_multi)(sesn_ptr, pers_ptr, REDIS_CMD_MY_FENCE_INVITED_USERS_ADD, SESSION_USERID(sesn_ptr_inviter), time_now, FENCE_ID(f_ptr), SESSION_USERID(sesn_ptr_invited));

//		(*pers_ptr->send_command_multi)(sesn_ptr, REDIS_CMD_FENCE_EVENTS, FENCE_ID(f_ptr), fe_ptr->eid, fe_ptr->eid, masterptr->serverid, SESSION_USERID(sesn_ptr_invited), time_now, SESSION_USERID(sesn_ptr_inviter), FENCE_ID(f_ptr), event_type, "event");

    (*pers_ptr->send_command_multi)(sesn_ptr, pers_ptr, "EXEC");

    size_t 		i,
            actually_processed = 5;
    redisReply	*replies[actually_processed];

    //TODO: we need error recover for intermediate errors
    for (i=0; i<actually_processed; i++) {
      if ((RedisGetReply((UfsrvSession *)sesn_ptr, pers_ptr, (void*)&replies[i]) != REDIS_OK)) {
        syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', cid:'%lu', cmd_idx:'%lu', uid_invalid:'%lu'}: ERROR: REDIS COMMAND IN MULTI SET FAILED", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), i, SESSION_USERID(sesn_ptr));

        if ((replies[i] != NULL) && (replies[i]->type != REDIS_REPLY_NIL)) {
          //syslog (LOG_DEBUG, LOGSTR_BACKENDCACHE_SHARED_CONTACTS, __func__, pthread_self(), sesn_ptr, i, replies[i]->str, LOGCODE_BACKENDCACHE_SHARED_CONTACTS_HIT, "Found shared contact token");
        }
      }

      if (!IS_EMPTY(replies[i]))	freeReplyObject(replies[i]);
    }//for

    DbBackendInsertUfsrvEvent((UfsrvEvent *)fe_ptr);

    fence_join_event_broadcast:
    InterBroadcastFenceInvite(sesn_ptr, (ClientContextData *)(&((BroadcastContextDataFenceInvite){sesn_ptr_inviter, sesn_ptr_invited, fstate_ptr})), fe_ptr, FENCE_COMMAND__COMMAND_TYPES__INVITE, COMMAND_ARGS__INVITED);

    return fe_ptr;
  }
  else//user being removed from the invite list
  if ((event_type == EVENT_TYPE_FENCE_USER_UNINVITED) || (event_type == EVENT_TYPE_FENCE_USER_INVITEREJECTED)  || (event_type == EVENT_TYPE_FENCE_USER_LIST_CORRECTED)) {
    sesn_ptr											= sesn_ptr_invited;//note using -> invited
    pers_ptr											= THREAD_CONTEXT_PERSISTENCE_CACHEBACKEND(THREAD_CONTEXT);//THREAD_CONTEXT_SESSION_CACHEBACKEND;
    mq_ptr												= THREAD_CONTEXT_MSGQUEUE_CACHEBACKEND;//THREAD_CONTEXT_MSGQUEUE_CACHEBACKEND;

    if (IS_PRESENT(fe_ptr_out))	fe_ptr = fe_ptr_out;
    else												fe_ptr = calloc(1, sizeof(FenceEvent));

    fe_ptr = RegisterFenceEvent(f_ptr, event_type,  NULL, 0/*LOCK_FLAG*/, fe_ptr);

    if (unlikely(IS_EMPTY(fe_ptr))) {
      if (IS_EMPTY(fe_ptr_out))	free(fe_ptr);
      return NULL;
    }

    fe_ptr->originator_ptr  = &SESSION_UFSRVUIDSTORE(sesn_ptr);
    fe_ptr->session_id	    =	SESSION_ID(sesn_ptr);
    time_now                = fe_ptr->when;

    unsigned long userid_invited_by = UfsrvUidGetSequenceId(&(fstate_ptr->invited_by));

    //IMPORTANT sesn_ptr_inviter IS NULL in this context. fstate_ptr->invited_by conains its uid
    (*pers_ptr->send_command_multi)(sesn_ptr, pers_ptr, "MULTI");

    //remove fence from user's list of invited for which it was invited
    (*pers_ptr->send_command_multi)(sesn_ptr, pers_ptr, REDIS_CMD_INVITED_FENCES_FOR_USER_REM, SESSION_USERID(sesn_ptr_invited), FENCE_ID(f_ptr), userid_invited_by);

    //remove user from fences list of invited users
    (*pers_ptr->send_command_multi)(sesn_ptr, pers_ptr, REDIS_CMD_INVITED_USERS_FOR_FERNCE_REM, FENCE_ID(f_ptr), SESSION_USERID(sesn_ptr_invited), SESSION_USERNAME(sesn_ptr_invited), userid_invited_by);

    //remove user from original inviter's list of ser it previously invited
    (*pers_ptr->send_command_multi)(sesn_ptr, pers_ptr, REDIS_CMD_MY_FENCE_INVITED_USERS_REM, userid_invited_by, FENCE_ID(f_ptr), SESSION_USERID(sesn_ptr_invited));

    (*pers_ptr->send_command_multi)(sesn_ptr, pers_ptr, "EXEC");

    size_t 			i,
            actually_processed = 5;
    redisReply	*replies[actually_processed];

    //TODO: we need error recover for intermediate errors
    for (i=0; i<actually_processed; i++) {
      if ((RedisGetReply((UfsrvSession *)sesn_ptr, pers_ptr, (void*)&replies[i]) != REDIS_OK)) {
        syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', cid:'%lu', cmd_idx:'%lu', uid_invalid:'%lu'}: ERROR: REDIS COMMAND IN MULTI SET FAILED", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), i, SESSION_USERID(sesn_ptr));

        if ((replies[i] != NULL) && (replies[i]->type != REDIS_REPLY_NIL)) {
          //syslog (LOG_DEBUG, LOGSTR_BACKENDCACHE_SHARED_CONTACTS, __func__, pthread_self(), sesn_ptr, i, replies[i]->str, LOGCODE_BACKENDCACHE_SHARED_CONTACTS_HIT, "Found shared contact token");
        }
      }

      if (!IS_EMPTY(replies[i]))	freeReplyObject(replies[i]);
    }//for

    DbBackendInsertUfsrvEvent((UfsrvEvent *)fe_ptr);

    enum _FenceCommand__CommandTypes  command_type;
    enum _CommandArgs                 command_arg;

    fence_uninvite_event_broadcast:
    if (event_type == EVENT_TYPE_FENCE_USER_UNINVITED) {
      command_arg = COMMAND_ARGS__UPDATED;
      command_type= FENCE_COMMAND__COMMAND_TYPES__INVITE_DELETED;
    } else if (event_type == EVENT_TYPE_FENCE_USER_INVITEREJECTED) {
      command_arg = COMMAND_ARGS__UPDATED;
      command_type = FENCE_COMMAND__COMMAND_TYPES__INVITE_REJECTED;
    } else if (event_type == EVENT_TYPE_FENCE_USER_LIST_CORRECTED) {
      command_arg = COMMAND_ARGS__RESYNC;//maintenance event
      command_type = FENCE_COMMAND__COMMAND_TYPES__INVITE_DELETED;
    } else {
      syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', event_type:'%u'}: ERROR: UNSUPPORTED event_type", __func__, pthread_self(), sesn_ptr, event_type);
      goto exit_with_null_free_event_if_necessary;
    }
    InterBroadcastFenceInvite(sesn_ptr, (ClientContextData *)(&((BroadcastContextDataFenceInvite){sesn_ptr_inviter, sesn_ptr_invited, fstate_ptr})), fe_ptr, command_type, command_arg);

  }
  else//user accepted and joined based on prior invitation. We don't generate eid for this, as actual user join will have covered it
  if (event_type == EVENT_TYPE_FENCE_USER_INVITED_JOINED) {
    sesn_ptr											= sesn_ptr_invited;//note invited
    pers_ptr											= THREAD_CONTEXT_PERSISTENCE_CACHEBACKEND(THREAD_CONTEXT);//THREAD_CONTEXT_SESSION_CACHEBACKEND;
    mq_ptr												= THREAD_CONTEXT_MSGQUEUE_CACHEBACKEND;//THREAD_CONTEXT_MSGQUEUE_CACHEBACKEND;

    unsigned long userid_invited_by = UfsrvUidGetSequenceId(&(fstate_ptr->invited_by));

    //IMPORTANT sesn_ptr_inviter IS NULL in this context. fstate_ptr->invited_by conains its uid
    (*pers_ptr->send_command_multi)(sesn_ptr, pers_ptr, "MULTI");

    //remove fence from user's list of invited for which it was invited
    (*pers_ptr->send_command_multi)(sesn_ptr, pers_ptr, REDIS_CMD_INVITED_FENCES_FOR_USER_REM, SESSION_USERID(sesn_ptr_invited), FENCE_ID(f_ptr), userid_invited_by);

    //remove user from fences list of invited users
    (*pers_ptr->send_command_multi)(sesn_ptr, pers_ptr, REDIS_CMD_INVITED_USERS_FOR_FERNCE_REM, FENCE_ID(f_ptr), SESSION_USERID(sesn_ptr_invited), SESSION_USERNAME(sesn_ptr_invited), userid_invited_by);

    //TODO: cannot execute this now as we dont have access to the inviter: enablewhen ready -> INCREASE ACTUALLY_PROCESSED TO 5
    //remove user from original inviter's list of ser it previously invited
    (*pers_ptr->send_command_multi)(sesn_ptr, pers_ptr, REDIS_CMD_MY_FENCE_INVITED_USERS_REM, userid_invited_by, FENCE_ID(f_ptr), SESSION_USERID(sesn_ptr_invited));

    (*pers_ptr->send_command_multi)(sesn_ptr, pers_ptr, "EXEC");

    size_t 			i,
            actually_processed = 5;
    redisReply	*replies[actually_processed];

    //TODO: we need error recover for intermediate errors
    for (i=0; i<actually_processed; i++) {
      if ((RedisGetReply((UfsrvSession *)sesn_ptr, pers_ptr, (void *)&replies[i]) != REDIS_OK)) {
        syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', cid:'%lu', cmd_idx:'%lu', uid_invalid:'%lu'}: ERROR: REDIS COMMAND IN MULTI SET FAILED", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), i, SESSION_USERID(sesn_ptr));

        if ((replies[i] != NULL) && (replies[i]->type != REDIS_REPLY_NIL)) {
          //syslog (LOG_DEBUG, LOGSTR_BACKENDCACHE_SHARED_CONTACTS, __func__, pthread_self(), sesn_ptr, i, replies[i]->str, LOGCODE_BACKENDCACHE_SHARED_CONTACTS_HIT, "Found shared contact token");
        }
      }

      if (!IS_EMPTY(replies[i]))	freeReplyObject(replies[i]);
    }//for

    return NULL;//we dont perform broadcast for that, as the actual join event will cover that off
  } else {
    syslog(LOG_DEBUG, "%s {pid:'%lu', o_inviter:'%p', o_invited:'%p', type:'%d'} Unsupported type received", __func__, pthread_self(), sesn_ptr_inviter?:NULL,sesn_ptr_invited?:NULL, event_type);
  }

  return NULL;

  exit_with_null_free_event_if_necessary:
  if (IS_EMPTY(fe_ptr_out))	free(fe_ptr);
  return NULL;
}

void
UpdateFenceAssignments(Session *sesn_ptr, Fence *f_ptr_processed, ClientContextData *context_ptr)
{
  FenceRecord *fence_record_ptr	=	(FenceRecord *)context_ptr;

  UpdateFenceKeyAssignment(f_ptr_processed, &(ZKGroupMasterKey){.raw.bytes_by_ref=fence_record_ptr->fkey.data}, FENCE_CALLFLAG_EMPTY);
  if (fence_record_ptr->has_fence_type)			UpdateFenceTypeAssignment(sesn_ptr, f_ptr_processed, fence_record_ptr->fence_type, 0);
  if (fence_record_ptr->has_privacy_mode)		UpdateFencePrivacyModeAssignment(sesn_ptr, f_ptr_processed, fence_record_ptr->privacy_mode, 0);
  if (fence_record_ptr->has_delivery_mode)	UpdateFenceDeliveryModeAssignment(sesn_ptr, f_ptr_processed, fence_record_ptr->delivery_mode, 0);
  if (fence_record_ptr->has_join_mode)			UpdateFenceJoinModeAssignment(sesn_ptr, f_ptr_processed, fence_record_ptr->join_mode, 0);
  if (fence_record_ptr->has_maxmembers)			UpdateFenceMaxUsersAssignment(sesn_ptr, f_ptr_processed, fence_record_ptr->maxmembers, 0);
  if (IS_PRESENT(fence_record_ptr->avatar))	UpdateFenceAvatarAssignment(sesn_ptr, f_ptr_processed, fence_record_ptr->avatar, 0);

  bool list_semantic;//false indicates the default "blacklisting"

  if (IS_PRESENT(fence_record_ptr->presentation) && fence_record_ptr->presentation->has_list_semantics) {
    list_semantic=(fence_record_ptr->presentation->list_semantics == FENCE_RECORD__PERMISSION__LIST_SEMANTICS__WHITELIST)?true:false;
    UpdateFencePermissionListSemanticAssignment(sesn_ptr, f_ptr_processed, FENCE_PERMISSIONS_PRESENTATION_PTR(f_ptr_processed), list_semantic, FENCE_CALLFLAG_EMPTY, NULL);
  }

  if (IS_PRESENT(fence_record_ptr->membership) && fence_record_ptr->presentation->has_list_semantics) {
    list_semantic = (fence_record_ptr->membership->list_semantics == FENCE_RECORD__PERMISSION__LIST_SEMANTICS__WHITELIST)?true:false;
    UpdateFencePermissionListSemanticAssignment(sesn_ptr, f_ptr_processed, FENCE_PERMISSIONS_MEMBERSHIP_PTR(f_ptr_processed), list_semantic, FENCE_CALLFLAG_EMPTY, NULL);
  }

  if (IS_PRESENT(fence_record_ptr->messaging) && fence_record_ptr->presentation->has_list_semantics) {
    list_semantic = (fence_record_ptr->messaging->list_semantics == FENCE_RECORD__PERMISSION__LIST_SEMANTICS__WHITELIST)?true:false;
    UpdateFencePermissionListSemanticAssignment(sesn_ptr, f_ptr_processed, FENCE_PERMISSIONS_MESSAGING_PTR(f_ptr_processed), list_semantic, FENCE_CALLFLAG_EMPTY, NULL);
  }

  if (IS_PRESENT(fence_record_ptr->attaching) && fence_record_ptr->presentation->has_list_semantics) {
    list_semantic = (fence_record_ptr->attaching->list_semantics == FENCE_RECORD__PERMISSION__LIST_SEMANTICS__WHITELIST)?true:false;
    UpdateFencePermissionListSemanticAssignment(sesn_ptr, f_ptr_processed, FENCE_PERMISSIONS_ATTACHING_PTR(f_ptr_processed), list_semantic, FENCE_CALLFLAG_EMPTY, NULL);
  }

  if (IS_PRESENT(fence_record_ptr->calling) && fence_record_ptr->presentation->has_list_semantics) {
    list_semantic = (fence_record_ptr->calling->list_semantics == FENCE_RECORD__PERMISSION__LIST_SEMANTICS__WHITELIST)?true:false;
    UpdateFencePermissionListSemanticAssignment(sesn_ptr, f_ptr_processed, FENCE_PERMISSIONS_CALLING_PTR(f_ptr_processed), list_semantic, FENCE_CALLFLAG_EMPTY, NULL);
  }

}

/**
 * 	@brief Localises the process for updating memory-resident data model for fence cname
 * 	param flag_build_cname: if true, cname_new should passed as NULL
 * 	@param cname_new: preallocated, except when flag_build_cname is set
 * 	@locked f_ptr:
 * 	@dynamic_memory EXPORTS/IMPORTS:
 * 	@dynamic_memory:	DEALLOCATES previously allocated strings for fence names
 */
bool
UpdateFenceNameAssignment(Session *sesn_ptr, InstanceHolderForFence *instance_f_ptr, const char *fname_new, char *cname_new, bool flag_build_cname, unsigned long fence_call_flags)
{
  char *cname_new_final = NULL;
  Fence *f_ptr = FenceOffInstanceHolder(instance_f_ptr);

  if (flag_build_cname) {
    asprintf(&cname_new_final, "%s%s", FENCE_BASELOC(f_ptr), fname_new);
  } else	cname_new_final = cname_new;

  free(FENCE_DNAME(f_ptr));
  FENCE_DNAME(f_ptr) = strdup(fname_new);

  RemoveFromHash(FenceRegistryCanonicalNameHashTableProvide(), (void *)(InstanceHolderForFence *)instance_f_ptr);
  free(FENCE_CNAME(f_ptr));

  FENCE_CNAME(f_ptr) = cname_new_final;

  /* The record is claimed for this mutation and the backend has already been written, so a local
   * incumbent for this name is stale: it gives way rather than refusing the rename. */
  void *replaced_ptr = NULL;

  if (AddToHashReplacing(FenceRegistryCanonicalNameHashTableProvide(), (void *)(InstanceHolderForFence *)instance_f_ptr, &replaced_ptr) == (void *)instance_f_ptr) {
    if (IS_PRESENT(replaced_ptr) && replaced_ptr != (void *)instance_f_ptr) {
      Fence *replaced_f_ptr = FenceOffInstanceHolder((InstanceHolderForFence *)replaced_ptr);

      if (IS_PRESENT(replaced_f_ptr)) {
        syslog(LOG_NOTICE, "%s {pid:'%lu', o:'%p', cname:'%s'}: NOTICE: NAME REASSIGNED AWAY FROM LOCAL INSTANCE:'%p' fid:'%lu': MARKED FOR RE-INSTATEMENT", __func__, pthread_self(), instance_f_ptr, FENCE_CNAME(f_ptr), replaced_ptr, FENCE_ID(replaced_f_ptr));
      }
    }

    return true;
  }

  syslog(LOG_ERR, "%s {pid:'%lu', o:'%p', cid:'%lu', fo:'%p', fid:'%lu'} ERROR: DATA SPACE IMBALANCE: COULD NOT RE-REGISTER FCNAME", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), f_ptr, FENCE_ID(f_ptr));

  return false;

}

/**
 * @brief Update memory-loaded Fence object with new description value.
 * @note only call this if @param description_new has been checked for description name validity
 * @param description_new new description value to ber assigned
 * @param fence_call_flags
 * @return false on error
 */
bool  __attribute__((nonnull(1, 2)))
UpdateFenceDescriptionAssignment(Session *sesn_ptr, InstanceHolderForFence *instance_f_ptr, const char *description_new, unsigned long fence_call_flags)
{
  Fence *f_ptr = FenceOffInstanceHolder(instance_f_ptr);

  if (IS_STR_LOADED(FENCE_DESCRIPTION(f_ptr))) free(FENCE_DESCRIPTION(f_ptr));
  if (IS_STR_LOADED(description_new))  FENCE_DESCRIPTION(f_ptr) = strdup(description_new);
  else FENCE_DESCRIPTION(f_ptr) = NULL;//designated as empty

  return true;

}

UFSRVResult * __attribute__((nonnull(1, 2)))
UpdateFenceKeyAssignment(Fence * restrict f_ptr, ZKGroupMasterKey * restrict group_key, unsigned long fence_call_flags)
{
  memcpy(FENCE_KEY(f_ptr), group_key->raw.bytes_by_ref, GROUP_MASTER_KEY_SIZE);

  if (fence_call_flags&FENCE_CALLFLAG_WRITEBACK_DATA_TO_BACKEND) {
    //TODO: also implement INTERBROADCAST
  }

  THREAD_CONTEXT_RETURN_RESULT_SUCCESS(NULL, RECODE_NONE)
}

/**
 *	@locked f_ptr:
 *	@locked sesn_ptr:
 *	@unlocks: none
 */
UFSRVResult *
UpdateFenceTypeAssignment(Session *sesn_ptr, Fence *f_ptr, FenceRecord__FenceType fence_type, unsigned long fence_call_flags)
{
  switch (fence_type)
  {
    case FENCE_RECORD__FENCE_TYPE__GEO:
      F_ATTR_SET(f_ptr->attrs, F_ATTR_BASEFENCE);
      break;

    case FENCE_RECORD__FENCE_TYPE__USER:
      F_ATTR_SET(f_ptr->attrs, F_ATTR_USERFENCE);
      break;

    default:
      syslog(LOG_ERR, "%s {pid:'%lu', o:'%p', cid:'%lu', fo:'%p', fid:'%lu', fence_type:'%d'} ERROR: UNKNOWN FENCE TYPE SEPCIFIED: DEFAULTING TO USERFENCE", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), f_ptr, FENCE_ID(f_ptr), fence_type);
      F_ATTR_SET(f_ptr->attrs, F_ATTR_USERFENCE);
      _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_FENCE_UNKNOWN_TYPE);
  }

  if (fence_call_flags&FENCE_CALLFLAG_WRITEBACK_DATA_TO_BACKEND) {
    //TODO: also implement INTERBROADCAST
  }

  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_SUCCESS, RESCODE_FENCE_DATA_UPDATED);
}

UFSRVResult *
UpdateFenceMaxUsersAssignment(Session *sesn_ptr, Fence *f_ptr, int maxusers, unsigned long fence_call_flags)
{
  FENCE_MAX_MEMBERS(f_ptr) = maxusers;

  if (fence_call_flags&FENCE_CALLFLAG_WRITEBACK_DATA_TO_BACKEND) {
    //TODO: also implement INTERBROADCAST
  }

  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_SUCCESS, RESCODE_FENCE_DATA_UPDATED);
}

UFSRVResult *
UpdateFencePrivacyModeAssignment(Session *sesn_ptr, Fence *f_ptr, FenceRecord__PrivacyMode privacy_mode, unsigned long fence_call_flags)
{
  switch (privacy_mode)
  {
    case FENCE_RECORD__PRIVACY_MODE__PRIVATE:
      F_ATTR_SET(f_ptr->attrs, F_ATTR_PRIVATE);
      break;

    case FENCE_RECORD__PRIVACY_MODE__PUBLIC:
      F_ATTR_SET(f_ptr->attrs, F_ATTR_PUBLIC);
      break;

    default:
      syslog(LOG_ERR, "%s {pid:'%lu', o:'%p', cid:'%lu', fo:'%p', fid:'%lu', fence_type:'%d'} ERROR: UNKNOWN FENCE PRIVACY MODE SEPCIFIED: DEFAULTING TO PUBLIC", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), f_ptr, FENCE_ID(f_ptr), privacy_mode);
      F_ATTR_SET(f_ptr->attrs, F_ATTR_PUBLIC);
      _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_FENCE_UNKNOWN_TYPE);
  }

  if (fence_call_flags&FENCE_CALLFLAG_WRITEBACK_DATA_TO_BACKEND) {
    //TODO: also implement INTERBROADCAST
  }

  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_SUCCESS, RESCODE_FENCE_DATA_UPDATED);
}

//todo:  also see SetFenceDeliveryModeFromProto (uint64_t *setting, FenceRecord__DeliveryMode delivery_mode)
UFSRVResult *
UpdateFenceDeliveryModeAssignment(Session *sesn_ptr, Fence *f_ptr, FenceRecord__DeliveryMode delivery_mode, unsigned long fence_call_flags)
{
  switch (delivery_mode)
  {
    case FENCE_RECORD__DELIVERY_MODE__BROADCAST:
      F_ATTR_SET(f_ptr->attrs, F_ATTR_BROADCAST);

      F_ATTR_UNSET(f_ptr->attrs, F_ATTR_BROADCAST_ONEWAY);
      F_ATTR_UNSET(f_ptr->attrs, F_ATTR_MANY_TO_MANY);
      break;

    case FENCE_RECORD__DELIVERY_MODE__BROADCAST_ONEWAY:
      F_ATTR_SET(f_ptr->attrs, F_ATTR_BROADCAST_ONEWAY);

      F_ATTR_UNSET(f_ptr->attrs, F_ATTR_BROADCAST);
      F_ATTR_UNSET(f_ptr->attrs, F_ATTR_MANY_TO_MANY);
      break;

    case FENCE_RECORD__DELIVERY_MODE__MANY:
      F_ATTR_SET(f_ptr->attrs, F_ATTR_MANY_TO_MANY);

      F_ATTR_UNSET(f_ptr->attrs, F_ATTR_BROADCAST);
      F_ATTR_UNSET(f_ptr->attrs, F_ATTR_BROADCAST_ONEWAY);
      break;

    default:
      syslog(LOG_ERR, "%s {pid:'%lu', o:'%p', cid:'%lu', fo:'%p', fid:'%lu', fence_type:'%d'} ERROR: UNKNOWN FENCE DELIVERY MODE SEPCIFIED: DEFAULTING TO MANY_TO_MANY", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), f_ptr, FENCE_ID(f_ptr), delivery_mode);
      F_ATTR_SET(f_ptr->attrs, F_ATTR_MANY_TO_MANY);
      F_ATTR_UNSET(f_ptr->attrs, F_ATTR_BROADCAST);
      F_ATTR_UNSET(f_ptr->attrs, F_ATTR_BROADCAST_ONEWAY);
      _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_FENCE_UNKNOWN_TYPE);
  }

  if (fence_call_flags&FENCE_CALLFLAG_WRITEBACK_DATA_TO_BACKEND)
  {
    //TODO: also implement INTERBROADCAST <- currently done in IsUserAllowedTo....
  }

  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_SUCCESS, RESCODE_FENCE_DATA_UPDATED);
}

UFSRVResult *
UpdateFenceJoinModeAssignment(Session *sesn_ptr, Fence *f_ptr, FenceRecord__JoinMode join_mode, unsigned long fence_call_flags)
{
  switch (join_mode)
  {
    case FENCE_RECORD__JOIN_MODE__OPEN:
      F_ATTR_SET(f_ptr->attrs, F_ATTR_JOINMODE_OPEN);
      break;

    case FENCE_RECORD__JOIN_MODE__INVITE:
      F_ATTR_SET(f_ptr->attrs, F_ATTR_JOINMODE_INVITE_ONLY);
      break;

    case FENCE_RECORD__JOIN_MODE__OPEN_WITH_KEY:
      F_ATTR_SET(f_ptr->attrs, F_ATTR_JOINMODE_OPEN|F_ATTR_JOINMODE_KEY);
      break;

    case FENCE_RECORD__JOIN_MODE__INVITE_WITH_KEY:
      F_ATTR_SET(f_ptr->attrs, F_ATTR_JOINMODE_INVITE_ONLY|F_ATTR_JOINMODE_KEY);
      break;

    default:
      syslog(LOG_ERR, "%s {pid:'%lu', o:'%p', cid:'%lu', fo:'%p', fid:'%lu', fence_type:'%d'} ERROR: UNKNOWN FENCE JOIN MODE SEPCIFIED: DEFAULTING TO OPEN", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), f_ptr, FENCE_ID(f_ptr), join_mode);
      F_ATTR_SET(f_ptr->attrs, F_ATTR_JOINMODE_OPEN);
      _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_FENCE_UNKNOWN_TYPE);
  }

  if (fence_call_flags&FENCE_CALLFLAG_WRITEBACK_DATA_TO_BACKEND)
  {
    //TODO: also implement INTERBROADCAST
  }

  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_SUCCESS, RESCODE_FENCE_DATA_UPDATED);
}

/**
 * 	@brief: this is designed to assign avatar as part of the fence creation process. It doesnt generate events, or broadcast
 * 	that specific event.
 * 	Check IsUserAllowedToChangeFenceAvatar() for similar treatment but in the context of update event.
 * 	@flags:
 * 	FENCE_CALLFLAG_WRITEBACK_DATA_TO_BACKEND:
 * 	@locked sesn_ptr:
 * 	@locked RW f_ptr:
 */
UFSRVResult *
UpdateFenceAvatarAssignment(Session *sesn_ptr, Fence *f_ptr, AttachmentRecord *record_ptr, unsigned long fence_call_flags)
{
  if (strlen(record_ptr->id) > CONFIG_MAX_FAVATAR_SIZE) {
#ifdef __UF_TESTING
    syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', cid:'%lu', fo:'%p', fid:'%lu'}: ERROR: ATTACHMENT ID TOO LONG", __func__, pthread_self(), sesn_ptr,  SESSION_ID(sesn_ptr), f_ptr, FENCE_ID(f_ptr));
#endif
    _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_FENCE_NAMING)
  }

  AttachmentDescriptor attachment_descriptor_out = {0};
  if (IS_PRESENT(GetAttachmentDescriptorEphemeral(sesn_ptr, record_ptr->id, false, &attachment_descriptor_out))) {
    syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', cid:'%lu', fo:'%p', fid:'%lu', blocb_id:'%s'}: ERROR: ATTACHMENT ID ALREADY EXISTS", __func__, pthread_self(), sesn_ptr,  SESSION_ID(sesn_ptr), f_ptr, FENCE_ID(f_ptr),  record_ptr->id);
    _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_LOGIC_IDENTICAL_RESOURCE)
  }

  if (AttachmentDescriptorGetFromProto(sesn_ptr, record_ptr, 0/*eid*/, &attachment_descriptor_out, true/*encode_key*/)) {
    DbAttachmentStore(sesn_ptr, &attachment_descriptor_out, FENCE_ID(f_ptr), 1);//ufsrv instance doesn't currently support lru-caching attachments

    //this should be removed, because at fence creation time there is no fence cache record yet
    if (fence_call_flags&FENCE_CALLFLAG_WRITEBACK_DATA_TO_BACKEND) {
      CacheBackendSetFenceAttribute(sesn_ptr, FENCE_ID(f_ptr), "avatar", attachment_descriptor_out.id);
    }

    if (unlikely(SESSION_RESULT_TYPE_ERROR(sesn_ptr)))	{_RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_BACKEND_DATA)}//todo: delete db backend record for attachment??

    //update memory store
    if (IS_STR_LOADED(FENCE_AVATAR(f_ptr)))	free (FENCE_AVATAR(f_ptr));
    FENCE_AVATAR(f_ptr) = strdup(attachment_descriptor_out.id);

    AttachmentDescriptorDestruct(&attachment_descriptor_out, true, false);
  }
  else {_RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_PROG_NULL_POINTER)}

  exit_success:
  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_SUCCESS, RESCODE_FENCE_DATA_UPDATED)
}

/**
 *
 * @param sesn_ptr_carrier
 * @param f_ptr
 * @param permission_ptr
 * @param whitelist
 * @param fence_call_flags
 * @return
 */
UFSRVResult *
UpdateFencePermissionListSemanticAssignment(Session *sesn_ptr_carrier, Fence *f_ptr, FencePermission *permission_ptr, bool whitelist, unsigned long fence_call_flags, FenceEvent *event_ptr_out)
{
  permission_ptr->config.whitelist = whitelist;

  if (fence_call_flags&FENCE_CALLFLAG_WRITEBACK_DATA_TO_BACKEND) {
    int lists_semantics = LoadListSemanticsForBackendPersistence(f_ptr);
    char list_semantics_str[sizeof(UINT32_LONGEST_STR) - 1] = {0};
    itoa(list_semantics_str, lists_semantics);

    FenceEvent *fence_event_ptr = RegisterFenceEvent(f_ptr, EVENT_TYPE_FENCE_LIST_SEMNATICS,  NULL, 0/*LOCK_FLAG*/, event_ptr_out);
    if (IS_PRESENT(fence_event_ptr)) {
      fence_event_ptr->originator_ptr = &SESSION_UFSRVUIDSTORE(sesn_ptr_carrier);
      fence_event_ptr->session_id	    =	SESSION_ID(sesn_ptr_carrier);
      CacheBackendSetFenceAttribute(sesn_ptr_carrier, FENCE_ID(f_ptr), "list_semantics", list_semantics_str);

      DbBackendInsertUfsrvEvent((UfsrvEvent *)fence_event_ptr); //todo: is sesn_ptr_carrier actual user? event captures this as originator

      InterBroadcastFenceListSemantics(sesn_ptr_carrier,
                                       &((FencePermissionContextData){.sesn_ptr=sesn_ptr_carrier, .fence.f_ptr=f_ptr, .permission_ptr=permission_ptr}),
                                       fence_event_ptr, COMMAND_ARGS__UPDATED);
    }
  }

  _RETURN_RESULT_SESN(sesn_ptr_carrier, NULL, RESULT_TYPE_SUCCESS, RESCODE_FENCE_DATA_UPDATED)
}

/**
 * 	@locked: f_ptr
 * 	@locked sesn_ptr
 */
UFSRVResult *
AddUserToFencePermissions(InstanceHolderForSession *instance_sesn_ptr, Fence *f_ptr, FencePermission *permission_ptr, unsigned long fence_call_flags, FenceEvent *fence_event_ptr_out)
{
  //TODO: may need to check cachebackend
  Session *sesn_ptr = SessionOffInstanceHolder(instance_sesn_ptr);

  if (IsUserOnFencePermissionList(sesn_ptr, f_ptr, permission_ptr))	goto return_success;

  if ((hopscotch_insert_with_offset(FENCE_PERMISSION_PERMITTED_USERS_PTR(permission_ptr),
                                    PermissionListItemExtractorCallback,
                                    (void *) instance_sesn_ptr,
                                    CONFIG_FENCE_PERMISSIONS_KEYOFFSET(Session, ufnet_session.sservice.user.user_details.user_id))) == 0) {//TODO:  refcount
    if (fence_call_flags&FENCE_CALLFLAG_WRITEBACK_DATA_TO_BACKEND) {
      FenceEvent *fence_event_ptr = RegisterFenceEvent(f_ptr, EVENT_TYPE_FENCE_PERMISSION,  NULL, 0/*LOCK_FLAG*/, fence_event_ptr_out);
      if (IS_PRESENT(fence_event_ptr)) {
        fence_event_ptr->originator_ptr = &SESSION_UFSRVUIDSTORE(sesn_ptr);
        fence_event_ptr->session_id	    =	SESSION_ID(sesn_ptr);
        CacheBackendPermissionMembersSetAdd(f_ptr, permission_ptr, SESSION_USERID(sesn_ptr));
        if (THREAD_CONTEXT_UFSRV_RESULT_TYPE_SUCCESS) {
          DbBackendInsertUfsrvEvent((UfsrvEvent *)fence_event_ptr);

          InterBroadcastFencePermission(sesn_ptr, &((FencePermissionContextData){.sesn_ptr=sesn_ptr, .fence.f_ptr=f_ptr, .permission_ptr=permission_ptr}), fence_event_ptr, COMMAND_ARGS__ADDED);
          goto return_success;
        } else {
          unsigned long userid = UfsrvUidGetSequenceId(&SESSION_UFSRVUIDSTORE(sesn_ptr));
          hopscotch_remove_with_offset(FENCE_PERMISSION_PERMITTED_USERS_PTR(permission_ptr), PermissionListItemExtractorCallback, (uint8_t *) &userid, CONFIG_FENCE_PERMISSIONS_KEYOFFSET(Session, ufnet_session.sservice.user.user_details.user_id));
          goto return_error;
          //TODO: invalidate event
        }
      }
      else goto return_error;
    }

    //this is case for INTER commands, where only internal state is affected
    goto return_success;
  }

  return_error:
  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_PROG_INCONSISTENT_STATE)

  return_success:
  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_SUCCESS, RESCODE_FENCE_PERMISSION_MEMBERSHIP)

}

/**
 *
 * @locked sesn_ptr
 */
UFSRVResult *
RemoveUserFromFencePermissions(InstanceHolderForSession *instance_sesn_ptr, Fence *f_ptr, FencePermission *permission_ptr, unsigned long fence_call_flags, FenceEvent *fence_event_ptr_out)
{
  //called in IsUserWithFencePermission
  //	if (unlikely(!FENCE_PERMISSION_CONFIG_INIT(permission_ptr)))	InitialiseFencePermission (permission_ptr, FENCE_PERMISSION_TYPE(permission_ptr));
  Session *sesn_ptr = SessionOffInstanceHolder(instance_sesn_ptr);

  if (!IsUserOnFencePermissionList(sesn_ptr, f_ptr, permission_ptr))	goto return_error_membership;

  if (fence_call_flags&FENCE_CALLFLAG_WRITEBACK_DATA_TO_BACKEND) {
    FenceEvent *fence_event_ptr = RegisterFenceEvent(f_ptr, EVENT_TYPE_FENCE_PERMISSION,  NULL, 0/*LOCK_FLAG*/, fence_event_ptr_out);
    if (IS_PRESENT(fence_event_ptr)) {
      fence_event_ptr->originator_ptr = &SESSION_UFSRVUIDSTORE(sesn_ptr);
      fence_event_ptr->session_id	    =	SESSION_ID(sesn_ptr);
      CacheBackendPermissionMembersSetRem(sesn_ptr, f_ptr, permission_ptr, SESSION_USERID(sesn_ptr));
      if (SESSION_RESULT_TYPE_SUCCESS(sesn_ptr)) {
        InstanceHolderForSession *instance_sesn_ptr_removed;
        unsigned long userid = UfsrvUidGetSequenceId(&SESSION_UFSRVUIDSTORE(sesn_ptr));

        if (IS_PRESENT((instance_sesn_ptr_removed = hopscotch_remove_with_offset(
                FENCE_PERMISSION_PERMITTED_USERS_PTR(permission_ptr), PermissionListItemExtractorCallback,
                (uint8_t *) &userid,
                CONFIG_FENCE_PERMISSIONS_KEYOFFSET(Session, ufnet_session.sservice.user.user_details.user_id))))) {
          assert(instance_sesn_ptr_removed == instance_sesn_ptr);

          DbBackendInsertUfsrvEvent((UfsrvEvent *)fence_event_ptr);

          InterBroadcastFencePermission(sesn_ptr,
                                        &((FencePermissionContextData){.sesn_ptr=sesn_ptr, .fence.f_ptr=f_ptr, .permission_ptr=permission_ptr}),
                                        fence_event_ptr, COMMAND_ARGS__DELETED);
          goto return_success;
        }
      }
    }

    goto return_error;
  } else {
    InstanceHolderForSession *instance_sesn_ptr_removed;
    unsigned long userid = UfsrvUidGetSequenceId(&SESSION_UFSRVUIDSTORE(sesn_ptr));
    if (IS_PRESENT((instance_sesn_ptr_removed = hopscotch_remove_with_offset(
            FENCE_PERMISSION_PERMITTED_USERS_PTR(permission_ptr), PermissionListItemExtractorCallback,
            (uint8_t *) &userid,
            CONFIG_FENCE_PERMISSIONS_KEYOFFSET(Session, ufnet_session.sservice.user.user_details.user_id)))))
    {
      assert(instance_sesn_ptr_removed == instance_sesn_ptr);
      goto return_success;
    }
    else goto return_error;
  }

  return_error_membership:
  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_FENCE_PERMISSION_MEMBERSHIP)

  return_error:
  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_PROG_INCONSISTENT_STATE)

  return_success:
  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_SUCCESS, RESCODE_FENCE_PERMISSION_MEMBERSHIP)

}

/**
 * 	@brief: This is responsible for updating the model, generating eids,
 * 	construct invited users-list from a given protobuf data. As it originates from the user, this is always reflects the outcome
 * 	of either add/remove/etc  for users on the list.
 * 	@param user_records @ref UserRecord collection originating from protocol model (protobuf)
 * 	@locked: fence_events lock must be in place
 * 	@locks: Session * for invited users
 */
CollectionDescriptor *
AddToFenceInvitedListFromProtoRecord(InstanceHolderForSession *instance_sesn_ptr, InstanceHolderForFence *instance_f_ptr, UserRecord **user_records, size_t invited_members_sz, CollectionDescriptorPair *invited_collections_for_result, bool flag_exclude_self)
{
  if (invited_members_sz < 1)	return NULL;

  size_t								processed													= 0;
  unsigned long 				sesn_call_flags										= 0;
  UserRecord 						*invited_user_record_ptr					= NULL;
  Session 							*sesn_ptr_invited									= NULL;
  CollectionDescriptor 	*invited_events_collection_ptr		= NULL;
  CollectionDescriptor 	*unprocessed_uids_collection_ptr	= NULL;
  Session               *sesn_ptr                         = SessionOffInstanceHolder(instance_sesn_ptr);
  Fence                 *f_ptr                            = FenceOffInstanceHolder(instance_f_ptr);

  sesn_call_flags = (CALL_FLAG_LOCK_SESSION|CALL_FLAG_HASH_SESSION_LOCALLY|CALL_FLAG_HASH_UID_LOCALLY|CALL_FLAG_HASH_USERNAME_LOCALLY|
                     CALL_FLAG_ATTACH_FENCE_LIST_TO_SESSION|CALL_FLAG_REMOTE_SESSION);

  invited_events_collection_ptr		=	&(invited_collections_for_result->first);
  unprocessed_uids_collection_ptr	=	&(invited_collections_for_result->second);

  for (size_t i=0; i<invited_members_sz; i++) {
    invited_user_record_ptr = user_records[i];

    unsigned long userid_invited = UfsrvUidGetSequenceId((const UfsrvUid *)invited_user_record_ptr->ufsrvuid.data);
    assert(userid_invited > 0);

    if (userid_invited == SESSION_USERID(sesn_ptr))	{
      if (flag_exclude_self || IsFenceOnShareListBlockedFence(sesn_ptr, FENCE_ID(f_ptr)))	{
        syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', fid:'%lu', o_invited:'%p', uid_invited:'%lu'}: ERROR: FENCE ON THIS USER's BLOCK LIST", __func__, pthread_self(), sesn_ptr, FENCE_ID(f_ptr), sesn_ptr, userid_invited);
        continue;
      }
    }

    bool lock_already_owned = false;
    GetSessionForThisUserByUserId(sesn_ptr, userid_invited, &lock_already_owned, sesn_call_flags);
    if (THREAD_CONTEXT_UFSRV_RESULT_TYPE_EQUALS(THREAD_CONTEXT, RESULT_TYPE_ERR)) {
      continue;
    }

    InstanceHolderForSession *instance_sesn_ptr_invited = (InstanceHolderForSession *)SESSION_RESULT_USERDATA(sesn_ptr);
    sesn_ptr_invited = SessionOffInstanceHolder(instance_sesn_ptr_invited);

    //>>>>>>> SESSION LOCKED
    if (unlikely(IS_EMPTY(sesn_ptr_invited))) {
      syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', cid:'%lu', fid:'%lu', uid_invited:'%lu'}: ERROR: COULD NOT RETRIEVE SESSION FOR INVITED USER", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), FENCE_ID(f_ptr), userid_invited);
      continue;
    }

    if (IsFenceOnShareListBlockedFence(sesn_ptr_invited,FENCE_ID(f_ptr))) {//todo Inform inviting user
      syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', fid:'%lu', o_invited:'%p', uid_invited:'%lu'}: ERROR: FENCE ON USER's BLOCK LIST", __func__, pthread_self(), sesn_ptr, FENCE_ID(f_ptr), sesn_ptr_invited, userid_invited);
      if (!lock_already_owned)	SessionUnLockCtx(THREAD_CONTEXT_PTR, AS_UFSRV_SESSION(sesn_ptr_invited), __func__);
      continue;
    }

    //todo: check if user already on members list, or ban list (not just on invited list) IsUserMemberOfThisFence()
    if (!IsUserOnFenceInvitedList(f_ptr, SESSION_USERID(sesn_ptr_invited))) {
      FenceEvent fence_event = {0};

      InstanceHolderForFenceStateDescriptor *instance_fstate_ptr = RecyclerGet(FenceStateDescriptorPoolTypeNumber(), (ContextData *)instance_f_ptr, CALLFLAGS_EMPTY);
      if (unlikely(IS_EMPTY(instance_fstate_ptr))) {
        if (IS_PRESENT(sesn_ptr_invited))	if (!lock_already_owned)	SessionUnLockCtx(THREAD_CONTEXT_PTR, AS_UFSRV_SESSION(sesn_ptr_invited), __func__);
        continue;
      }

      FenceStateDescriptor *fence_descriptor = FenceStateDescriptorOffInstanceHolder(instance_fstate_ptr);

      FENCESTATE_INSTANCE_HOLDER(fence_descriptor)  =	instance_f_ptr;
      memcpy(fence_descriptor->invited_by.data, SESSION_UFSRVUID(sesn_ptr), CONFIG_MAX_UFSRV_ID_SZ);

      FenceEvent *fe_ev = UpdateBackendFenceInvitedData(sesn_ptr, sesn_ptr_invited, fence_descriptor, EVENT_TYPE_FENCE_USER_INVITED, &fence_event);
      if (IS_PRESENT(fe_ev)) {
        AddThisToList(SESSION_INVITED_FENCE_LIST_PTR(sesn_ptr_invited), instance_fstate_ptr);
        FenceIncrementReference(fence_descriptor->instance_holder_fence, 1);

        AddThisToList(&f_ptr->fence_user_sessions_invited_list, CLIENT_CTX_DATA(instance_sesn_ptr_invited));
        SessionIncrementReference(instance_sesn_ptr_invited, 1);

        //note the use of array index 'i'. failed events will have eid as '0', but overall we maintain index consistency between array and invitation list provided by user
        ((unsigned long *)invited_events_collection_ptr->collection)[i] = fe_ev->eid;
        fence_descriptor->when_invited = fe_ev->when;
        processed++;
      } else {
        syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', cid:'%lu', fid:'%lu', uid_invited:'%lu'}: ERROR: COULD NOT RETRIEVE FENCE EVENT FOR INVITED USER", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), FENCE_ID(f_ptr), SESSION_USERID(sesn_ptr_invited));
        FenceStateDescriptorReturnToRecycler(instance_fstate_ptr, NULL, 0);
        //TODO: what do we do with the generated event? should mark it as failed
      }
    } else if (IS_PRESENT(unprocessed_uids_collection_ptr))	{
      ((unsigned long *)unprocessed_uids_collection_ptr->collection)[i] = userid_invited;
      unprocessed_uids_collection_ptr->collection_sz++;
      syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', cid:'%lu', fid:'%lu', uid_invited:'%lu'}: ERROR: USER ALREADY ON LIST: COULD NOT ADD USER TO INVITE LIST", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), FENCE_ID(f_ptr), userid_invited);
    }

    if (sesn_ptr_invited != NULL)	if (!lock_already_owned)	SessionUnLockCtx(THREAD_CONTEXT_PTR, AS_UFSRV_SESSION(sesn_ptr_invited), __func__);
  }//for

  invited_events_collection_ptr->collection_sz = processed;//caller will check if processed less than provided list

#ifdef __UF_TESTING
  syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', cname:'%s', recved_list_sz:'%lu', processed_list_sz:'%lu'}: Fence Invitaion List: processed", __func__, pthread_self(), sesn_ptr, FENCE_CNAME(f_ptr), invited_members_sz, processed);
#endif

  return invited_events_collection_ptr;
}

/**
 * 	@brief Main entry point for joining users into EXISTING fences. invoked in the context of a given id supplied by user.
 * 	Caller must pass FENCE_CALLFLAG_JOIN in order to actually join.

 *	@param sesn_ptr: The user for whom command is being executed
 *  @return InstanceHolder of type FenceStateDescriptor
 * 	@call_flags_fence FENCE_CALLFLAG_JOIN: the context of the request is user join
 * 	@call_flags_fence FENCE_CALLFLAG_JOIN_LINKJOIN: the context of the request is user join via a linkjoin

 * 	@locks RW f_ptr: if FENCE_CALLFLAG_JOIN is passed we lock the fence
 * 	@unlocks f_ptr: unless FENCE_CALLFLAG_KEEP_FENCE_LOCKED is passed, unless RESCODE_PROG_NULL_POINTER is returned
 * 	@locked sesn_ptr: must be locked in the caller's environment
 */
UFSRVResult *IsUserAllowedToJoinFenceById(InstanceHolderForSession *instance_sesn_ptr, const unsigned long fence_id, const char *linkjoin_nonce, unsigned long call_flags_fence, bool *fence_lock_state)
{
	Fence			*f_ptr											= NULL;

  unsigned long	call_flags_fence_finding	= 0;
  call_flags_fence_finding = FENCE_CALLFLAG_SEARCH_BACKEND|FENCE_CALLFLAG_HASH_FENCE_LOCALLY|FENCE_CALLFLAG_ATTACH_USER_LIST_TO_FENCE;
	if (call_flags_fence & FENCE_CALLFLAG_KEEP_FENCE_LOCKED) call_flags_fence_finding |= (FENCE_CALLFLAG_KEEP_FENCE_LOCKED | FENCE_CALLFLAG_LOCK_FENCE_BLOCKING);

	Session *sesn_ptr = SessionOffInstanceHolder(instance_sesn_ptr);

  //region fence finding
	FindFenceById(sesn_ptr, fence_id, call_flags_fence_finding);
  InstanceHolderForFence *instance_f_ptr = SESSION_RESULT_USERDATA(sesn_ptr);

  if (IS_EMPTY((instance_f_ptr))) {
		if (SESSION_RESULT_CODE_EQUAL(sesn_ptr, RESCODE_FENCE_DOESNT_EXIST) || SESSION_RESULT_CODE_EQUAL(sesn_ptr, RESCODE_BACKEND_RESOURCE_NULL)) {
#ifdef __UF_TESTING
			syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p', cid:'%lu', fid:'%lu'): COULD NOT FIND FENCE...",		__func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), fence_id);
#endif
			SESSION_RETURN_RESULT(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_FENCE_DOESNT_EXIST)
		}
		else 	{SESSION_RETURN_RESULT(sesn_ptr, NULL, RESULT_TYPE_ERR, SESSION_RESULT_CODE(sesn_ptr))}
	}

  bool fence_lock_already_owned = (SESSION_RESULT_CODE_EQUAL(sesn_ptr, RESCODE_PROG_LOCKED_BY_THIS_THREAD));
  f_ptr = FenceOffInstanceHolder(instance_f_ptr);
  //endregion

	//>>> Fence RW LOCKED

  if (IsFenceKeyEmpty(FENCE_KEY(f_ptr))) {
#ifdef __UF_TESTING
    syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p', fo:'%p', fid:'%lu'): ERROR: FENCE_KEY UNALLOCATED...",		__func__, pthread_self(), sesn_ptr, f_ptr, fence_id);
#endif
    if (!fence_lock_already_owned)	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, SESSION_RESULT_PTR(sesn_ptr));
    SESSION_RETURN_RESULT(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_USER_FENCE_KEY)
  }

	UFSRVResult *res_ptr = _ProcessUserIsAllowedToJoinFence(instance_sesn_ptr, instance_f_ptr, linkjoin_nonce, call_flags_fence);

	//in case of logical error, we take care of unlocking
	if (res_ptr->result_code == RESCODE_PROG_NULL_POINTER)	{
		if (!fence_lock_already_owned)	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, SESSION_RESULT_PTR(sesn_ptr));
	} else {
		if (!(call_flags_fence & FENCE_CALLFLAG_KEEP_FENCE_LOCKED))	if (!fence_lock_already_owned)	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, SESSION_RESULT_PTR(sesn_ptr));
		if (likely(IS_PRESENT(fence_lock_state)))	*fence_lock_state = fence_lock_already_owned;
	}

	return res_ptr;

}

/**
 * 	@brief Resolves rules around user's ability to join an existing fence. Works with two frontend functions above.
 * 	All joins must funnel through this.
 *
 * 	@call_flags FENCE_CALLFLAG_JOIN: we proceed to join the user if allowed so
 *
 *	@returns InstanceHolder of type FenceStateDescripton with rescode RESCODE_USER_FENCE_JOINED and RESCODE_USER_FENCE_ALREADYIN
 *	@returns Fence   with rescode RESCODE_USER_FENCE_CANJOIN
 *
 * 	@locks: NONE
 * 	@unlocks: NONE
 * 	@locked sesn_ptr: must be locked by the caller.
 * 	@locked RW f_ptr: must be locked by the caller.
 */
UFSRVResult *
_ProcessUserIsAllowedToJoinFence(InstanceHolderForSession *instance_sesn_ptr, InstanceHolderForFence *instance_f_ptr, const char *linkjoin_nonce, unsigned long call_flags_fence)
{
  InstanceHolderForFenceStateDescriptor *instance_fstate_ptr   = NULL;
  Session *sesn_ptr = SessionOffInstanceHolder(instance_sesn_ptr);
  Fence *f_ptr = FenceOffInstanceHolder(instance_f_ptr);

#define FLAG_FENCE_LOCK_FALSE false

	if (IS_PRESENT((instance_fstate_ptr = IsUserMemberOfThisFence(&(SESSION_FENCE_LIST(sesn_ptr)), f_ptr, FLAG_FENCE_LOCK_FALSE)))) {
		//Extra integrity check
		if (FindUserInFenceSessionListByID(&FENCE_USER_SESSION_LIST(f_ptr), f_ptr, SESSION_ID(sesn_ptr))) {
			_RETURN_RESULT_SESN(sesn_ptr, instance_fstate_ptr, RESULT_TYPE_ERR, RESCODE_USER_FENCE_ALREADYIN)
		} else {
			syslog (LOG_DEBUG, LOGSTR_FENCE_MISSING_SESSION, __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), f_ptr, FENCE_ID(f_ptr), LOGCODE_FENCE_MISSING_SESSION); //data integrity issue
			_RETURN_RESULT_SESN(sesn_ptr, instance_f_ptr, RESULT_TYPE_ERR, RESCODE_FENCE_SESSION_INTEGRITY)
		}
	}

  //todo Why is this repeated?
/*	if (FindUserInFenceSessionListByID(&FENCE_USER_SESSION_LIST(f_ptr), f_ptr, SESSION_ID(sesn_ptr))) {
		syslog (LOG_DEBUG, LOGSTR_SESSION_MISSING_FENCE, __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), f_ptr, FENCE_ID(f_ptr), LOGCODE_SESSION_MISSING_FENCE);
		_RETURN_RESULT_SESN(sesn_ptr, instance_f_ptr, RESULT_TYPE_ERR, RESCODE_SESSION_FENCE_INTEGRITY)
	}*/

//Everything else equal, if we reached max capacity already, nobody can get in unless it is the owner
  if (FENCE_HAS_MAX_MEMBERS_SIZE_SET(f_ptr)) {
    if (FENCE_HAS_REACHED_MAX_MEMBERS_SIZE(f_ptr)) {
#ifdef __UF_TESTING
      syslog (LOG_DEBUG, "%s {pid:'%lu', o:'%p', fid:'%lu'}: REACHED MAXIMUM USER COUNT: configured max users: '%lu'. Current user count: '%d'", __func__, pthread_self(), sesn_ptr, FENCE_ID(f_ptr), f_ptr->max_users, f_ptr->fence_user_sessions_list.nEntries);
#endif

      //todo allow anyway if owner
      _RETURN_RESULT_SESN(sesn_ptr, f_ptr, RESULT_TYPE_ERR, RESCODE_USER_FENCE_FULL)
    }
  }

  if (call_flags_fence & FENCE_CALLFLAG_JOIN_LINKJOIN) {
    if (!IS_STR_LOADED(linkjoin_nonce)) {
#ifdef __UF_TESTING
      syslog (LOG_DEBUG, "%s {pid:'%lu', o:'%p', fid:'%lu'}: LINKJOIN WITHOUT NONCE", __func__, pthread_self(), sesn_ptr, FENCE_ID(f_ptr));
#endif
      _RETURN_RESULT_SESN(sesn_ptr, f_ptr, RESULT_TYPE_ERR, RESCODE_FENCE_LINKJOIN_NONCE)
    }

    goto allow_user_join;
  }

	//general case for invite only
	if (FENCE_IS_INVITE_ONLY(f_ptr) && !IsUserOnFenceInvitedList(f_ptr, SESSION_USERID(sesn_ptr))) {
    if (FENCE_OWNER_UID(f_ptr) == SESSION_USERID(sesn_ptr) && !FENCE_HAS_REACHED_MAX_MEMBERS_SIZE(f_ptr)) {
#ifdef __UF_TESTING
      syslog (LOG_DEBUG, "%s {pid:'%lu', o:'%p', cid:'%lu', fid:'%lu'}: FENCE INVITE-ONLY: ALLOWING OWNER BACK IN", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), FENCE_ID(f_ptr));
#endif

      goto allow_user_join;
    }

#ifdef __UF_TESTING
	syslog (LOG_DEBUG, "%s {pid:'%lu', o:'%p', cid:'%lu', fid:'%lu'}: FENCE INVITE-ONLY: USER NOT ON INVITED LIST.", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), FENCE_ID(f_ptr));
#endif
			_RETURN_RESULT_SESN(sesn_ptr, f_ptr, RESULT_TYPE_ERR, RESCODE_FENCE_INVITATION_LIST)
	}

	//second check based on WriteoffList
	{
#if 0
		//not implemented yet
		_RETURN_RESULT_SESN(sesn_ptr, f_ptr, RESULT_TYPE_ERR, RESCODE_USER_FENCE_WRITEOFF);
#endif
	}

	//third check on locality test
	{
#if 0
		//not implemented yet
		_RETURN_RESULT_SESN(sesn_ptr, f_ptr, RESULT_TYPE_ERR, RESCODE_USER_FENCE_LOCATION);
#endif
	}

  allow_user_join:
  if (call_flags_fence & FENCE_CALLFLAG_JOIN) {
    InstanceHolder *instance_fstate_ptr = AddUserToThisFenceListWithLinkback(instance_sesn_ptr, instance_f_ptr, SESSION_FENCE_LIST_PTR(sesn_ptr), &(f_ptr->fence_user_sessions_list),  EVENT_TYPE_FENCE_USER_JOINED, CALLFLAGS_EMPTY);

    if (IS_PRESENT(instance_fstate_ptr)) {
      FenceStateDescriptor *fstate_ptr = FenceStateDescriptorOffInstanceHolder(instance_fstate_ptr);

      //TODO: this doesn't work with INVITED EVENT; has to be invoked seperately by the caller
      UpdateBackendFenceData(sesn_ptr, f_ptr, fstate_ptr, EVENT_TYPE_FENCE_USER_JOINED, &((FenceEvent){}));

      if (call_flags_fence & FENCE_CALLFLAG_JOIN_LINKJOIN) {
        ProcessLinkJoinUserActionJoined(instance_sesn_ptr, f_ptr, linkjoin_nonce);
      }

      RemoveUserFromInvitedList(instance_sesn_ptr, fstate_ptr, &((FenceEvent) {.event_type=EVENT_TYPE_FENCE_USER_INVITED_JOINED}), FENCE_CALLFLAG_WRITEBACK_DATA_TO_BACKEND | FENCE_CALLFLAG_TRANSFER_INVITE_CONTEXT);

      _RETURN_RESULT_SESN(sesn_ptr, instance_fstate_ptr, RESULT_TYPE_SUCCESS, RESCODE_USER_FENCE_JOINED)
    }
    _RETURN_RESULT_SESN(sesn_ptr, instance_f_ptr, RESULT_TYPE_ERR, RESCODE_FENCE_SESSION_INTEGRITY)
  }

  _RETURN_RESULT_SESN(sesn_ptr, f_ptr, RESULT_TYPE_SUCCESS, RESCODE_USER_FENCE_CANJOIN)

#undef FLAG_FENCE_LOCK_FALSE
}

/**
 * 	@brief: main  entry point for joining users into EXISTING fences. invoked in the context of a given id supplied by user.
 * 	Caller must pass FENCE_CALLFLAG_JOIN in order to actually join.
 * 	@return InstanceHolder of type FenceStateDescriptor
 * 	@call_flags: FENCE_CALLFLAG_KEEP_FENCE_LOCKED
 * 	@locks RW f_ptr: indirectly via flags
 * 	@unlocks f_ptr: unless FENCE_CALLFLAG_KEEP_FENCE_LOCKED is set by caller
 */
UFSRVResult *
IsUserAllowedToJoinFenceByCanonicalName(InstanceHolderForSession *instance_sesn_ptr_this, const char *fence_canonical_name, unsigned long call_flags)
{
	bool fence_locked_already = false;
	Session *sesn_ptr_this = SessionOffInstanceHolder(instance_sesn_ptr_this);
	//this will lock fence but doesn't  return it in locked state unless KEE_LOCKED flag is set
	InstanceHolderForFence *instance_f_ptr = FindUserFenceByCanonicalName(sesn_ptr_this, fence_canonical_name, &fence_locked_already, FENCE_CALLFLAG_KEEP_FENCE_LOCKED|FENCE_CALLFLAG_HASH_FENCE_LOCALLY|FENCE_CALLFLAG_SEARCH_BACKEND|FENCE_CALLFLAG_ATTACH_USER_LIST_TO_FENCE);
	if (IS_EMPTY(instance_f_ptr)) {
#ifdef __UF_TESTING
		syslog(LOG_DEBUG, "%s (pid='%lu' cid='%lu'): ERROR: COULD FIND FENCE for canonical name '%s'", __func__, pthread_self(), sesn_ptr_this->ufsrv_session.session_id, fence_canonical_name);
#endif
		_RETURN_RESULT_SESN(sesn_ptr_this, NULL, RESULT_TYPE_NOOP, RESCODE_PROG_NULL_POINTER)
	}

  Fence *f_ptr = FenceOffInstanceHolder(instance_f_ptr);
	//f_ptr RW LOCKED

	UFSRVResult *res_ptr = _ProcessUserIsAllowedToJoinFence(instance_sesn_ptr_this, instance_f_ptr, NULL, call_flags);

	//in case of logical error, we take care of unlocking
	if (res_ptr->result_code == RESCODE_PROG_NULL_POINTER) {
		if (!fence_locked_already) FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, SESSION_RESULT_PTR(sesn_ptr_this));
	} else {
		if (!(call_flags&FENCE_CALLFLAG_KEEP_FENCE_LOCKED))	if (!fence_locked_already)	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, SESSION_RESULT_PTR(sesn_ptr_this));
	}

	return res_ptr; //todo: fence already-locked state lost

}

/**
 * 	@brief:	Main entry point for making _user fence_ : no other function should be called before this one.
 * 	If fence already exists drops user in, otherwise it creates new one.
 * 	we allow fence name collisions as it is up to the user to decide how they want to live with that
 * 	@return InstanceHolderForFenceStateDescriptor when fenced JOINED, MADE, or ALREADY_IN
 * 	@return InstanceHolderForFence
 *
 * 	@call_flags FENCE_CALLFLAG_KEEP_FENCE_LOCKED:  return the fence in locked state for both new and existing fences.
 * 	@call_flags: FENCE_CALLFLAG_JOIN: the context of the request is a user joining/making fence. Affects locking mode.
 *
 * 	@locks WR f_ptr: As a side effect of of searching for Fence *.
 * 	@unlocks f_ptr: unless  FENCE_CALLFLAG_KEEP_FENCE_LOCKED is passed. ALSO automatically unlocks it if some logical/structural error with Fence *
 *
*/
UFSRVResult *
IsUserAllowedToMakeUserFence(InstanceHolderForSession *instance_sesn_ptr, const char *fence_banner, const char *baseloc_prefix, FenceContextDescriptor *fence_context_ptr, bool *fence_lock_state, unsigned long call_flags)
{
  Session *sesn_ptr = SessionOffInstanceHolder(instance_sesn_ptr);
	if (({ FenceRecord *fence_record = (FenceRecord *)fence_context_ptr->context_ptr; (IS_EMPTY(fence_record->fkey.data) || fence_record->fkey.len <= 0); })) {
    syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p', th_ctx:'%p'): ERROR: INVALID FENCE KEY", __func__, pthread_self(), sesn_ptr, THREAD_CONTEXT_PTR);

	  _RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_PROG_NULL_POINTER)
	}

	//TODO: we risk invoking this function yet another time in the else block as we create a new fence as part of new fence instantiation
	//better we pass the canonical name, along with display name

	size_t cname_sz = SizeofCanonicalFenceName(sesn_ptr, fence_banner);
	if (unlikely(cname_sz == 0)) {
		syslog (LOG_ERR, "%s {pid:'%lu', o:'%p', uid:'%lu', baseloc:'%s'}: ERROR: COULD NOT ESTABLISH CNAME SIZE", __func__, pthread_self(), sesn_ptr, SESSION_USERID(sesn_ptr), SESSION_HOMEBASE_GEOLOC_DYNAMIC(sesn_ptr));

		_RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_PROG_NULL_POINTER)
	}

	char userfence_canonical_name[cname_sz]; memset(userfence_canonical_name, 0, sizeof(userfence_canonical_name));

	if (IsUserBaseLocSelfZoned(sesn_ptr)) {
    MakeCanonicalSelfZonedFenceName(SESSION_USERID(sesn_ptr), fence_banner, userfence_canonical_name);
  } else {
    if (IS_STR_LOADED(baseloc_prefix)) {
      if (IsHomebaseGeoLocValid(sesn_ptr, baseloc_prefix)) {
        sprintf(userfence_canonical_name, "%s%s", SESSION_SERVICE(sesn_ptr)->user.user_details.homebase_geoloc_dynamic, fence_banner);
      } else {
        LocationDescription *ld_ptr = GetLocationDescription(sesn_ptr);
        _MakeCanonicalFenceName(ld_ptr, SESSION_USERID(sesn_ptr), fence_banner, USER_ATTRIBUTE_IS_SET(sesn_ptr, USERATTRIBUTE_DEFINES_USERZONE), userfence_canonical_name);
      }
    } else {
      LocationDescription *ld_ptr = GetLocationDescription(sesn_ptr);
      _MakeCanonicalFenceName(ld_ptr, SESSION_USERID(sesn_ptr), fence_banner, USER_ATTRIBUTE_IS_SET(sesn_ptr, USERATTRIBUTE_DEFINES_USERZONE), userfence_canonical_name);
    }
  }

  bool fence_already_locked = false;
	unsigned long fence_call_options = FENCE_CALLFLAG_KEEP_FENCE_LOCKED|FENCE_CALLFLAG_HASH_FENCE_LOCALLY|FENCE_CALLFLAG_SEARCH_BACKEND|FENCE_CALLFLAG_ATTACH_USER_LIST_TO_FENCE;
	InstanceHolderForFence *instance_f_ptr = NULL;

  //this will lock fence but doesn't  return it in locked state unless KEEP_LOCKED flag is set
	if ((instance_f_ptr = FindUserFenceByCanonicalName(sesn_ptr, userfence_canonical_name, &fence_already_locked, fence_call_options))) {
    Fence *f_ptr = FenceOffInstanceHolder(instance_f_ptr);

    UFSRVResult *res_ptr = _ProcessUserIsAllowedToJoinFence(instance_sesn_ptr, instance_f_ptr, NULL, call_flags);

		if (IS_PRESENT(fence_lock_state))	*fence_lock_state = fence_already_locked;

		switch (res_ptr->result_code)
		{
			case RESCODE_USER_FENCE_FULL:
				if (!(call_flags&FENCE_CALLFLAG_KEEP_FENCE_LOCKED))	if (!fence_already_locked) FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, SESSION_RESULT_PTR(sesn_ptr));
				SESSION_RETURN_RESULT(sesn_ptr, instance_f_ptr, RESULT_TYPE_ERR, RESCODE_USER_FENCE_FULL)

			case RESCODE_USER_FENCE_ALREADYIN:
			  //this returns fstate_ptr
				if (!(call_flags&FENCE_CALLFLAG_KEEP_FENCE_LOCKED))	if (!fence_already_locked) FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, SESSION_RESULT_PTR(sesn_ptr));
				SESSION_RETURN_RESULT(sesn_ptr, _RESULT_USERDATA(res_ptr), RESULT_TYPE_ERR, RESCODE_USER_FENCE_ALREADYIN)

			case RESCODE_USER_FENCE_WRITEOFF://user on writeoff list
				if (!(call_flags&FENCE_CALLFLAG_KEEP_FENCE_LOCKED))	if (!fence_already_locked) FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, SESSION_RESULT_PTR(sesn_ptr));
				SESSION_RETURN_RESULT(sesn_ptr, instance_f_ptr, RESULT_TYPE_ERR, RESCODE_USER_FENCE_WRITEOFF)

			case RESCODE_USER_FENCE_LOCATION:
				if (!(call_flags&FENCE_CALLFLAG_KEEP_FENCE_LOCKED))	if (!fence_already_locked) FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, SESSION_RESULT_PTR(sesn_ptr));
				SESSION_RETURN_RESULT(sesn_ptr, instance_f_ptr, RESULT_TYPE_ERR, RESCODE_USER_FENCE_LOCATION)

			case RESCODE_USER_FENCE_KEY:
				if (!(call_flags&FENCE_CALLFLAG_KEEP_FENCE_LOCKED))	if (!fence_already_locked) FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, SESSION_RESULT_PTR(sesn_ptr));
				SESSION_RETURN_RESULT(sesn_ptr, instance_f_ptr, RESULT_TYPE_ERR, RESCODE_USER_FENCE_KEY)

			case RESCODE_USER_FENCE_JOINED:
				//this is ideal case
				syslog (LOG_DEBUG, "%s: SUCESS: FOUND EXISTING Fence with the same name: fcname='%s': DROPPING  user in..", __func__, FENCE_CNAME(f_ptr));

				if (!(call_flags&FENCE_CALLFLAG_KEEP_FENCE_LOCKED))	if (!fence_already_locked) FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, SESSION_RESULT_PTR(sesn_ptr));
        InstanceHolderForFenceStateDescriptor *instance_fstate_ptr = _RESULT_USERDATA(res_ptr);
				SESSION_RETURN_RESULT(sesn_ptr, instance_fstate_ptr, RESULT_TYPE_SUCCESS, RESCODE_USER_FENCE_JOINED)

			default:
				//for this kind of unknown errors we unlock automatically
				if (fence_already_locked) FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, SESSION_RESULT_PTR(sesn_ptr));
				SESSION_RETURN_RESULT(sesn_ptr, instance_f_ptr, RESULT_TYPE_ERR, RESCODE_PROG_NULL_POINTER)
		}
	} else {
			//NO FENCE LOCK YET
		  //passing canonical_name will bypass the invocation of _f_MakeCanonicalFenceName: we shouldn't double handle
			//this is a RW locking function
			InstanceHolderForFenceStateDescriptor *instance_fstate_ptr = CreateUserFenceAndLinkToUser(instance_sesn_ptr, fence_banner, userfence_canonical_name, fence_context_ptr, call_flags&FENCE_CALLFLAG_KEEP_FENCE_LOCKED ? FENCE_CALLFLAG_KEEP_FENCE_LOCKED : 0);
		  if (IS_PRESENT(instance_fstate_ptr)) {
		    FenceStateDescriptor *fstate_ptr = FenceStateDescriptorOffInstanceHolder(instance_fstate_ptr);
        Fence *f_ptr = FenceOffInstanceHolder(FENCESTATE_INSTANCE_HOLDER(fstate_ptr));
		  	//note the fence is fresh out of recycler so no check for lock already exists test
			  if (!(call_flags&FENCE_CALLFLAG_KEEP_FENCE_LOCKED))	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, SESSION_RESULT_PTR(sesn_ptr));

			  if (IS_PRESENT(fence_lock_state))	*fence_lock_state = false;//not relevant

			  SESSION_RETURN_RESULT(sesn_ptr, instance_fstate_ptr, RESULT_TYPE_SUCCESS, RESCODE_USER_FENCE_MADE)
		  } else {
			  syslog (LOG_DEBUG, "%s (pid:'%lu', o:'%p', th_ctx:'%p'): ERROR: COULD NOT CREATE NEW UserFence fcname='%s'", __func__, pthread_self(), sesn_ptr, THREAD_CONTEXT_PTR, userfence_canonical_name);
			  //terminates below no f_ptr to unlock
		  }
	}

	SESSION_RETURN_RESULT(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_PROG_NULL_POINTER)

}


/**
 * 	@brief Main interface for checking whether a user is permitted to change a fence state. Normally used for pre-screening
 * 	requests coming down the wire. As it currently stands, user has to be a member of the fence to begin with, hence why on
 * 	success the FenceStateDescriptor is returned.
 *
 *	@returns NULL, RESULT_TYPE_ERR: if fence doesn't exist
 *	@returns Fence:, RESULT_TYPE_ERR:	if either membership or ownership fails
 *	@returns InstanceHolderForFenceStateDescriptor: if either membership or ownership succeeded
 *
 * 	@locked sesn_ptr: by caller
 * 	@locks f_ptr: by downstream functions and remains locked after this function exists if FENCE_CALLFLAG_KEEP_FENCE_LOCKED is set
 */
UFSRVResult *
IsUserAllowedToChangeFence(Session *sesn_ptr, unsigned long fid, const char *cname, bool *fence_lock_state, unsigned long fence_call_flags)
{
	bool lock_already_owned = false;
	unsigned 	fence_call_flags_final;
	unsigned	rescode;
	Fence 		*f_ptr;
  InstanceHolderForFence *instance_f_ptr = NULL;

	fence_call_flags_final = FENCE_CALLFLAG_SEARCH_BACKEND|FENCE_CALLFLAG_HASH_FENCE_LOCALLY|FENCE_CALLFLAG_ATTACH_USER_LIST_TO_FENCE;
	if (fence_call_flags&FENCE_CALLFLAG_KEEP_FENCE_LOCKED)	fence_call_flags_final |= (FENCE_CALLFLAG_KEEP_FENCE_LOCKED|FENCE_CALLFLAG_LOCK_FENCE_BLOCKING);

	if (fid > 0) {
		FindFenceById(sesn_ptr, fid,	fence_call_flags_final);
    instance_f_ptr = (InstanceHolderForFence *)SESSION_RESULT_USERDATA(sesn_ptr);
		lock_already_owned = (SESSION_RESULT_CODE_EQUAL(sesn_ptr, RESCODE_PROG_LOCKED_BY_THIS_THREAD));
	} else if	(IS_STR_LOADED(cname)) {
		FindFenceByCanonicalName(sesn_ptr, cname, &lock_already_owned, fence_call_flags_final);
    instance_f_ptr = (InstanceHolderForFence *)SESSION_RESULT_USERDATA(sesn_ptr);
	}

	if (IS_EMPTY(instance_f_ptr)) {
#ifdef __UF_TESTING
		syslog (LOG_DEBUG, "%s {pid:'%lu', o:'%p', cid:'%lu', uname:'%s', fid:'%lu'}: FenceStateSync COMMAND IGNORED: COULD NOT LOCATE FENCE", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), SESSION_USERNAME(sesn_ptr), fid);
#endif

		_RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_FENCE_DOESNT_EXIST)
	}

	f_ptr = FenceOffInstanceHolder(instance_f_ptr);

	//FENCE NOW LOCKED if FENCE_CALLFLAG_KEEP_FENCE_LOCKED was set

	rescode = RESCODE_FENCE_MEMBERSHIP;
	InstanceHolderForFenceStateDescriptor *instance_fstate_ptr = IsUserMemberOfThisFence(SESSION_FENCE_LIST_PTR(sesn_ptr), f_ptr, 0/*DONT_LOCK*/);
	if (IS_EMPTY(instance_fstate_ptr))  goto exit_error;

	if (fence_call_flags&FENCE_CALLFLAG_CHECK_FENCEOWNERSHIP) {
    rescode = RESCODE_FENCE_OWNERSHIP;
    if (!IsFenceOwnedByUser(sesn_ptr, f_ptr)) goto exit_error;
  }

  //only one type can be checked
  if (fence_call_flags&FENCE_CALLFLAG_CHECK_PERMESSIONS) {
    rescode = RESCODE_FENCE_PERMISSION_PROHIBITED;
    if (fence_call_flags & FENCE_CALLFLAG_CHECK_PERM_PRESENTATION) {
      if (IsUserWithPermission(sesn_ptr, f_ptr, FENCE_PERMISSIONS_PRESENTATION_PTR(f_ptr))) goto exit_success;
    } else if  (fence_call_flags & FENCE_CALLFLAG_CHECK_PERM_MEMBERSHIP) {
        if (IsUserWithPermission(sesn_ptr, f_ptr, FENCE_PERMISSIONS_MEMBERSHIP_PTR(f_ptr))) goto exit_success;
      }

    goto exit_error;
  }

	//TODO: LOST state of lock_already_owned due to rescode assignment? Is still the case?

	exit_success:
	if (!(fence_call_flags&FENCE_CALLFLAG_KEEP_FENCE_LOCKED))	if (!lock_already_owned)	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, SESSION_RESULT_PTR(sesn_ptr));
	if (likely(IS_PRESENT(fence_lock_state)))	*fence_lock_state = lock_already_owned;
	_RETURN_RESULT_SESN(sesn_ptr, instance_fstate_ptr, RESULT_TYPE_SUCCESS, rescode)

	exit_error:
	if (!(fence_call_flags&FENCE_CALLFLAG_KEEP_FENCE_LOCKED))	if (!lock_already_owned)	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, SESSION_RESULT_PTR(sesn_ptr));
	if (likely(IS_PRESENT(fence_lock_state)))	*fence_lock_state = lock_already_owned;

	if (rescode == RESCODE_FENCE_OWNERSHIP) { //note we return fence state if error was related to ownership
		_RETURN_RESULT_SESN(sesn_ptr, instance_fstate_ptr, RESULT_TYPE_ERR, rescode)
	} else {
		_RETURN_RESULT_SESN(sesn_ptr, instance_f_ptr, RESULT_TYPE_ERR, rescode)
	}
}

/**
 * @brief Qualify an incoming request to reject a previously received fence-joining invitation.
 * @param sesn_ptr User session for the rejecting user
 * @param fid fid identifier
 * @param cname canonical name used as a secondary identifier
 * @param fence_lock_state carrier flag to send back to caller about lock's state of ownership (if set, lock already owned by host thread)
 * @param fence_call_flags FENCE_CALLFLAG_KEEP_FENCE_LOCKED fence will be kept locked upon return to caller
 * @return on success: InstanceHolderForFence *
 * @locks f_ptr (will keep locked if FENCE_CALLFLAG_KEEP_FENCE_LOCKED is provided)
 * @unlocks f_ptr if FENCE_CALLFLAG_KEEP_FENCE_LOCKED is absent
 */
UFSRVResult *
IsUserAllowedToRejectFence(Session *sesn_ptr, unsigned long fid, const char *cname, bool *fence_lock_state, unsigned long fence_call_flags)
{
	bool lock_already_owned = false;
	unsigned 	fence_call_flags_final;
	unsigned	rescode;
	Fence 		*f_ptr;
	InstanceHolderForFence *instance_f_ptr = NULL;

	fence_call_flags_final = FENCE_CALLFLAG_SEARCH_BACKEND|FENCE_CALLFLAG_HASH_FENCE_LOCALLY|FENCE_CALLFLAG_ATTACH_USER_LIST_TO_FENCE;
	if (fence_call_flags&FENCE_CALLFLAG_KEEP_FENCE_LOCKED)	fence_call_flags_final |= (FENCE_CALLFLAG_KEEP_FENCE_LOCKED|FENCE_CALLFLAG_LOCK_FENCE_BLOCKING);

	if (fid > 0) {
		FindFenceById(sesn_ptr, fid,	fence_call_flags_final);
		instance_f_ptr = (InstanceHolderForFence *)SESSION_RESULT_USERDATA(sesn_ptr);
		lock_already_owned = (SESSION_RESULT_CODE_EQUAL(sesn_ptr, RESCODE_PROG_LOCKED_BY_THIS_THREAD));
	} else if	(IS_STR_LOADED(cname)) {
		FindFenceByCanonicalName(sesn_ptr, cname, &lock_already_owned, fence_call_flags_final);
		instance_f_ptr = (InstanceHolderForFence *)SESSION_RESULT_USERDATA(sesn_ptr);
	}

	if (IS_EMPTY(instance_f_ptr)) {
#ifdef __UF_TESTING
		syslog (LOG_DEBUG, "%s {pid:'%lu', o:'%p', cid:'%lu', uname:'%s', fid:'%lu'}: FenceStateSync COMMAND IGNORED: COULD NOT LOCATE FENCE", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), SESSION_USERNAME(sesn_ptr), fid);
#endif

		_RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_FENCE_DOESNT_EXIST)
	}

	f_ptr = FenceOffInstanceHolder(instance_f_ptr);

	//FENCE NOW LOCKED if FENCE_CALLFLAG_KEEP_FENCE_LOCKED was set

	rescode = RESCODE_FENCE_MEMBERSHIP;

	if (IsUserOnFenceInvitedList(f_ptr, SESSION_USERID(sesn_ptr))) {
		goto exit_success;
	} else {
		//todo perhaps also check is user is already on fence list, although not consequential
//		InstanceHolderForFenceStateDescriptor *instance_fstate_ptr = IsUserMemberOfThisFence(SESSION_FENCE_LIST_PTR(sesn_ptr), f_ptr, 0/*DONT_LOCK*/);
//		if (IS_EMPTY(instance_fstate_ptr))  goto exit_error;
		goto exit_error;
	}

	exit_success:
	if (!(fence_call_flags&FENCE_CALLFLAG_KEEP_FENCE_LOCKED))	if (!lock_already_owned)	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, SESSION_RESULT_PTR(sesn_ptr));
	if (likely(IS_PRESENT(fence_lock_state)))	*fence_lock_state = lock_already_owned;
	_RETURN_RESULT_SESN(sesn_ptr, instance_f_ptr, RESULT_TYPE_SUCCESS, rescode)

	exit_error:
	if (!(fence_call_flags&FENCE_CALLFLAG_KEEP_FENCE_LOCKED))	if (!lock_already_owned)	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, SESSION_RESULT_PTR(sesn_ptr));
	if (likely(IS_PRESENT(fence_lock_state)))	*fence_lock_state = lock_already_owned;

	_RETURN_RESULT_SESN(sesn_ptr, instance_f_ptr, RESULT_TYPE_ERR, rescode)
}

UFSRVResult *
IsUserAllowedToRemoveInvitedUser(Session *sesn_ptr, unsigned long fid, InstanceContextForFence *instance_fence_ctx, ContextData *ctx_data, unsigned long fence_call_flags)
{
	bool lock_already_owned = false;
	unsigned 	fence_call_flags_final;
	unsigned	rescode;
	Fence 		*f_ptr;
	InstanceHolderForFence *instance_f_ptr = NULL;
  CollectionDescriptorPair *uninvited_collection = AS_COLLECTION_DESCRIPTOR_PAIR(ctx_data);

	fence_call_flags_final = FENCE_CALLFLAG_SEARCH_BACKEND|FENCE_CALLFLAG_HASH_FENCE_LOCALLY|FENCE_CALLFLAG_ATTACH_USER_LIST_TO_FENCE;
	if (fence_call_flags&FENCE_CALLFLAG_KEEP_FENCE_LOCKED)	fence_call_flags_final |= (FENCE_CALLFLAG_KEEP_FENCE_LOCKED|FENCE_CALLFLAG_LOCK_FENCE_BLOCKING);

	if (fid > 0) {
		FindFenceById(sesn_ptr, fid,	fence_call_flags_final);
		instance_f_ptr = (InstanceHolderForFence *)SESSION_RESULT_USERDATA(sesn_ptr);
		lock_already_owned = (SESSION_RESULT_CODE_EQUAL(sesn_ptr, RESCODE_PROG_LOCKED_BY_THIS_THREAD));
	}

	if (IS_EMPTY(instance_f_ptr)) {
#ifdef __UF_TESTING
		syslog (LOG_DEBUG, "%s {pid:'%lu', o:'%p', cid:'%lu', uname:'%s', fid:'%lu'}: FenceStateSync COMMAND IGNORED: COULD NOT LOCATE FENCE", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), SESSION_USERNAME(sesn_ptr), fid);
#endif

		_RETURN_RESULT_SESN(sesn_ptr, NULL, RESULT_TYPE_ERR, RESCODE_FENCE_DOESNT_EXIST)
	}

	f_ptr = FenceOffInstanceHolder(instance_f_ptr);

	//FENCE NOW LOCKED

  unsigned long sesn_call_flags_uninvited = (CALL_FLAG_HASH_SESSION_LOCALLY | CALL_FLAG_HASH_UID_LOCALLY | CALL_FLAG_HASH_USERNAME_LOCALLY | CALL_FLAG_ATTACH_FENCE_LIST_TO_SESSION | CALL_FLAG_REMOTE_SESSION);
	rescode = RESCODE_FENCE_MEMBERSHIP;
  unsigned long userid_uninvited = 0;
  UserRecord *user_record;
  UfsrvUid *ufsrvuid;
  InstanceHolderForSession *instance_sesn_ptr_target;
  InstanceContextForSession *instance_sesn_ctx;
  for (size_t idx=0; idx<uninvited_collection->source.collection_sz; idx++) {
    user_record = (UserRecord *)uninvited_collection->source.collection[idx];
    UFSRVResult *result = (UFSRVResult *)(uninvited_collection->first.collection + (idx * sizeof(UFSRVResult)));
    if (IS_PRESENT(user_record)) {
      userid_uninvited = UfsrvUidGetSequenceId((const UfsrvUid *)user_record->ufsrvuid.data);
      GetSessionForThisUserByUserId(sesn_ptr, userid_uninvited, NULL, sesn_call_flags_uninvited); //not locking CALL_FLAG_LOCK_SESSION
      if (THREAD_CONTEXT_UFSRV_RESULT_TYPE_EQUALS(THREAD_CONTEXT, RESULT_TYPE_SUCCESS)) {
        instance_sesn_ctx = ((void *)uninvited_collection->second.collection) + (idx * sizeof(InstanceContextForSession));
        instance_sesn_ctx->instance_sesn_ptr = (InstanceHolderForSession *)THREAD_CONTEXT_UFSRV_RESULT_USERDATA;
        if (IsUserOnFenceInvitedList(f_ptr, userid_uninvited)) {
          result->result_type = RESULT_TYPE_SUCCESS;
        } else {
          result->result_type = RESULT_TYPE_ERR; result->result_code = rescode;
        }

        //todo check if user already a member
        continue;
      }
    }

    result->result_type = RESULT_TYPE_ERR; result->result_code = rescode;
    uninvited_collection->second.collection[idx] = user_record;
  }

	if (!(fence_call_flags&FENCE_CALLFLAG_KEEP_FENCE_LOCKED))	{
    if (!lock_already_owned)	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, SESSION_RESULT_PTR(sesn_ptr));
  } else instance_fence_ctx->is_locked      = true;

  instance_fence_ctx->lock_already_owned  = lock_already_owned;
  instance_fence_ctx->f_ptr               = f_ptr;
  instance_fence_ctx->instance_f_ptr      = instance_f_ptr;
	_RETURN_RESULT_SESN(sesn_ptr, instance_f_ptr, RESULT_TYPE_SUCCESS, rescode)

}

/**
 *	@brief: Works for Network BaseFences, no user defined fence name, as opposed to MakeFenceByCanonicalName, unrolls the canonical name and creates
 *	BaseFences for the locations bits that are known. Follows the naming rules used in MakeCanonicalFenceName.
 *	For basefence the selfzone is always empty.
 *
 *	@param ld_ptr: Denotes user's location description (as opposed to fence's)
 *
 *	@returns: the number of intra fences created as an outcome of parsing the components of user'slocation
 *  @worker_thread: THREAD_CONTEXT required
 * 	@locks: NONE
 */
unsigned
_CheckIntraBaseFencesAndAdd(Session *sesn_ptr, const LocationDescription *ld_ptr, unsigned long add_flag)
{
	bool  fence_already_locked			= false;
	unsigned int				counter			= 0;
	HttpRequestContext	*http_ptr		=	GetHttpRequestContext(sesn_ptr);

	if (IS_STR_LOADED(ld_ptr->country)) {
		char *cn_country	= NULL;
		Fence *f_ptr		  = NULL;
    InstanceHolder *instance_holder_ptr;

		asprintf(&cn_country, "%s:::0:", ld_ptr->country);

		//country
    instance_holder_ptr = FindBaseFenceByCanonicalName(sesn_ptr, cn_country, &fence_already_locked, FENCE_CALLFLAG_SEARCH_BACKEND|FENCE_CALLFLAG_KEEP_FENCE_LOCKED);
		if (IS_EMPTY(instance_holder_ptr)) {
			instance_holder_ptr = RecyclerGet(FencePoolTypeNumber(), (ContextData *)THREAD_CONTEXT_FENCE_CACHEBACKEND, FENCE_CALLFLAG_BASEFENCE|FENCE_CALLFLAG_GENERATE_ID|FENCE_CALLFLAG_LOCK_FENCE);
			if (unlikely(IS_EMPTY(instance_holder_ptr))) {
				syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', cid:'%lu', name:'%s'}: ERROR: COULD NOT GET FENCE TYPE INSTANCE...", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), cn_country);
				free (cn_country);

				return 0;
			}

			//fence locked
			f_ptr = FenceOffInstanceHolder(instance_holder_ptr);

			counter++;
			UpdateLocationIfDifferent(&f_ptr->fence_location,&((FenceLocationDescription){.display_banner_name=ld_ptr->country, .canonical_name=cn_country, .base_location=cn_country}));

			bool  claim_inserted = false;
			void *claim_resident = NULL;

			if (!AddToHashUniquely(FenceRegistryCanonicalNameHashTableProvide(), instance_holder_ptr, &claim_inserted, &claim_resident) || !claim_inserted) {
				if (IS_PRESENT(claim_resident) && claim_resident != instance_holder_ptr)
					syslog(LOG_ERR, "%s {pid:'%lu', o:'%p', cname:'%s'}: ERROR: FENCE NAME ALREADY CLAIMED BY INSTANCE:'%p'", __func__, pthread_self(), instance_holder_ptr, FENCE_CNAME(f_ptr), claim_resident);

				goto exit_error_country;
			}
			if (!AddToHashUniquely(FenceRegistryIdHashTableProvide(), instance_holder_ptr, &claim_inserted, &claim_resident) || !claim_inserted) {
				RemoveFromHash(FenceRegistryCanonicalNameHashTableProvide(), instance_holder_ptr);

				exit_error_country:
				DestructFenceLocationDescription(&(f_ptr->fence_location));
				free(cn_country);
				RecyclerPut(FencePoolTypeNumber(), instance_holder_ptr, (ContextData *)NULL, FENCE_CALLFLAG_UNLOCK_FENCE);
				return 0;
			}

      GenerateZKGroupParams(f_ptr);

			GeocodeLocation(sesn_ptr, http_ptr, &(f_ptr->fence_location.fence_location), f_ptr->fence_location.display_banner_name);

			UpdateBackendFenceData(sesn_ptr, f_ptr, NULL, EVENT_TYPE_FENCE_CREATED, &((FenceEvent){}));
		}

		//fence locked
		free(cn_country); cn_country = NULL;
		f_ptr = FenceOffInstanceHolder(instance_holder_ptr);
		SESSION_BASEFENCE_LOCAL(sesn_ptr) = FENCE_ID(f_ptr);//gets overwritten successively down to the most specific
    if (!fence_already_locked)  FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, THREAD_CONTEXT_UFSRV_RESULT(THREAD_CONTEXT));
    fence_already_locked = false;

    //......................................................................................
		if (IS_STR_LOADED(ld_ptr->admin_area)) { //'country:admin_area'
			char *cn_country_admin_area = NULL;

			asprintf(&cn_country_admin_area, "%s:%s::0:", ld_ptr->country, ld_ptr->admin_area);

      instance_holder_ptr = FindBaseFenceByCanonicalName(sesn_ptr, cn_country_admin_area, &fence_already_locked, FENCE_CALLFLAG_SEARCH_BACKEND|FENCE_CALLFLAG_KEEP_FENCE_LOCKED);
			if (IS_EMPTY(instance_holder_ptr)) {
        instance_holder_ptr = RecyclerGet(FencePoolTypeNumber(), (ContextData *)THREAD_CONTEXT_FENCE_CACHEBACKEND, FENCE_CALLFLAG_BASEFENCE|FENCE_CALLFLAG_GENERATE_ID|FENCE_CALLFLAG_LOCK_FENCE);
				if (unlikely(IS_EMPTY(instance_holder_ptr))) {
					syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', cid:'%lu', name:'%s'}: ERROR: COULD NOT GET FENCE TYPE INSTANCE...", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), cn_country_admin_area);
					free(cn_country_admin_area);

					return 0;
				}

        f_ptr = FenceOffInstanceHolder(instance_holder_ptr);

				UpdateLocationIfDifferent (&f_ptr->fence_location,
																		&((FenceLocationDescription){.display_banner_name=ld_ptr->admin_area, .canonical_name=cn_country_admin_area, .base_location=cn_country_admin_area}));

				bool  claim_inserted = false;
				void *claim_resident = NULL;

				if (!AddToHashUniquely(FenceRegistryCanonicalNameHashTableProvide(), instance_holder_ptr, &claim_inserted, &claim_resident) || !claim_inserted) {
					if (IS_PRESENT(claim_resident) && claim_resident != instance_holder_ptr)
						syslog(LOG_ERR, "%s {pid:'%lu', o:'%p', cname:'%s'}: ERROR: FENCE NAME ALREADY CLAIMED BY INSTANCE:'%p'", __func__, pthread_self(), instance_holder_ptr, FENCE_CNAME(f_ptr), claim_resident);

					goto exit_error_country_admina_area;
				}
				if (!AddToHashUniquely(FenceRegistryIdHashTableProvide(), instance_holder_ptr, &claim_inserted, &claim_resident) || !claim_inserted) {
					RemoveFromHash(FenceRegistryCanonicalNameHashTableProvide(), instance_holder_ptr);

					exit_error_country_admina_area:
					DestructFenceLocationDescription(&(f_ptr->fence_location));
					free(cn_country_admin_area);
					RecyclerPut(FencePoolTypeNumber(), instance_holder_ptr, (ContextData *)NULL, FENCE_CALLFLAG_UNLOCK_FENCE);

					return 0;
				}

        GenerateZKGroupParams(f_ptr);

				char *s = NULL;
				asprintf(&s, "%s,%s", ld_ptr->country, f_ptr->fence_location.display_banner_name);
				GeocodeLocation(sesn_ptr, http_ptr, &(f_ptr->fence_location.fence_location), s); free(s);

				UpdateBackendFenceData(sesn_ptr, f_ptr, NULL, EVENT_TYPE_FENCE_CREATED, &((FenceEvent){}));

				counter++;
			}

			free(cn_country_admin_area); cn_country_admin_area = NULL;
			f_ptr = FenceOffInstanceHolder(instance_holder_ptr);
			SESSION_BASEFENCE_LOCAL(sesn_ptr) = FENCE_ID(f_ptr);
      if (!fence_already_locked)  FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, THREAD_CONTEXT_UFSRV_RESULT(THREAD_CONTEXT));
      fence_already_locked = false;

      //................................................................................................
			if (IS_STR_LOADED(ld_ptr->locality)) {//ideal case: all components defined
			//country:admin_area:locality
				char *cn_country_admin_area_locality = NULL;

				asprintf(&cn_country_admin_area_locality, "%s:%s:%s:0:", ld_ptr->country, ld_ptr->admin_area, ld_ptr->locality);

        instance_holder_ptr = FindBaseFenceByCanonicalName(sesn_ptr, cn_country_admin_area_locality, &fence_already_locked, FENCE_CALLFLAG_SEARCH_BACKEND|FENCE_CALLFLAG_KEEP_FENCE_LOCKED);
				if (IS_EMPTY(instance_holder_ptr)) {
					instance_holder_ptr = RecyclerGet(FencePoolTypeNumber(), (ContextData *)THREAD_CONTEXT_FENCE_CACHEBACKEND, FENCE_CALLFLAG_BASEFENCE|FENCE_CALLFLAG_GENERATE_ID|FENCE_CALLFLAG_LOCK_FENCE);
					if (unlikely(IS_EMPTY(instance_holder_ptr))) {
						syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', cid:'%lu', name:'%s'}: ERROR: COULD NOT GET FENCE TYPE INSTANCE...", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), cn_country_admin_area_locality);
						free (cn_country_admin_area_locality);

						return 0;
					}

					//fence locked
          f_ptr = FenceOffInstanceHolder(instance_holder_ptr);

					UpdateLocationIfDifferent (&f_ptr->fence_location, &((FenceLocationDescription){.display_banner_name=ld_ptr->locality, .canonical_name=cn_country_admin_area_locality, .base_location=cn_country_admin_area_locality}));

					bool  claim_inserted = false;
					void *claim_resident = NULL;

					if (!AddToHashUniquely(FenceRegistryCanonicalNameHashTableProvide(), instance_holder_ptr, &claim_inserted, &claim_resident) || !claim_inserted) {
						if (IS_PRESENT(claim_resident) && claim_resident != instance_holder_ptr)
							syslog(LOG_ERR, "%s {pid:'%lu', o:'%p', cname:'%s'}: ERROR: FENCE NAME ALREADY CLAIMED BY INSTANCE:'%p'", __func__, pthread_self(), instance_holder_ptr, FENCE_CNAME(f_ptr), claim_resident);

						goto exit_error_country_admina_area_local;
					}
					if (!AddToHashUniquely(FenceRegistryIdHashTableProvide(), instance_holder_ptr, &claim_inserted, &claim_resident) || !claim_inserted) {
						RemoveFromHash(FenceRegistryCanonicalNameHashTableProvide(), instance_holder_ptr);

						exit_error_country_admina_area_local:
						DestructFenceLocationDescription(&(f_ptr->fence_location));
						free(cn_country_admin_area_locality);
						RecyclerPut(FencePoolTypeNumber(), instance_holder_ptr, (ContextData *)NULL, CALLFLAGS_EMPTY|FENCE_CALLFLAG_UNLOCK_FENCE);

						return 0;
					}

          GenerateZKGroupParams(f_ptr);

					char *s = NULL;
					asprintf(&s, "%s,%s,%s", ld_ptr->country, ld_ptr->admin_area, ld_ptr->locality);
					GeocodeLocation(sesn_ptr, http_ptr, &(f_ptr->fence_location.fence_location), s); free(s);

					UpdateBackendFenceData(sesn_ptr, f_ptr, NULL, EVENT_TYPE_FENCE_CREATED, &((FenceEvent){}));

					counter++;
					free(cn_country_admin_area_locality); cn_country_admin_area_locality = NULL;
				}

				//fence locked
				f_ptr = FenceOffInstanceHolder(instance_holder_ptr);
				SESSION_BASEFENCE_LOCAL(sesn_ptr) = FENCE_ID(f_ptr);

        if (!fence_already_locked)  FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, THREAD_CONTEXT_UFSRV_RESULT(THREAD_CONTEXT));
        fence_already_locked = false;
			} else {
			  //country:admin_area:NO_locality
			//circular logic this is covered in the parent case
			}
		} else {
		  //country:NO_admin_area
			if (IS_STR_LOADED(ld_ptr->locality)) {//TODO: we should not allow this at client side
			//country:NO_admin_area:locality
				char *cn_s = NULL;

				asprintf(&cn_s, "%s::%s:0:", ld_ptr->country, ld_ptr->locality);//note display name collision with above

        instance_holder_ptr = FindBaseFenceByCanonicalName(sesn_ptr, cn_s, &fence_already_locked, FENCE_CALLFLAG_SEARCH_BACKEND|FENCE_CALLFLAG_KEEP_FENCE_LOCKED);
				if (IS_EMPTY(instance_holder_ptr)) {
					instance_holder_ptr = RecyclerGet(FencePoolTypeNumber(), (ContextData *)THREAD_CONTEXT_FENCE_CACHEBACKEND, FENCE_CALLFLAG_BASEFENCE|FENCE_CALLFLAG_GENERATE_ID|FENCE_CALLFLAG_LOCK_FENCE);
					if (unlikely(IS_EMPTY(instance_holder_ptr))) {
						syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', cid:'%lu', name:'%s'}: ERROR: COULD NOT GET FENCE TYPE INSTANCE...", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), cn_s);
						free (cn_s);

						return 0;
					}

					//fence locked
          f_ptr = FenceOffInstanceHolder(instance_holder_ptr);

					UpdateLocationIfDifferent(&f_ptr->fence_location, &((FenceLocationDescription){.display_banner_name=ld_ptr->locality, .canonical_name=cn_s, .base_location=cn_s}));

					bool  claim_inserted = false;
					void *claim_resident = NULL;

					if (!AddToHashUniquely(FenceRegistryCanonicalNameHashTableProvide(), instance_holder_ptr, &claim_inserted, &claim_resident) || !claim_inserted) {
						if (IS_PRESENT(claim_resident) && claim_resident != instance_holder_ptr)
							syslog(LOG_ERR, "%s {pid:'%lu', o:'%p', cname:'%s'}: ERROR: FENCE NAME ALREADY CLAIMED BY INSTANCE:'%p'", __func__, pthread_self(), instance_holder_ptr, FENCE_CNAME(f_ptr), claim_resident);

						goto exit_error_cn_s;
					}
					if (!AddToHashUniquely(FenceRegistryIdHashTableProvide(), instance_holder_ptr, &claim_inserted, &claim_resident) || !claim_inserted) {
						//TODO: free f_ptr and dealocate
						RemoveFromHash(FenceRegistryCanonicalNameHashTableProvide(), (void *) instance_holder_ptr);

						exit_error_cn_s:
						DestructFenceLocationDescription(&(f_ptr->fence_location));
						free(cn_s);
						RecyclerPut(FencePoolTypeNumber(), instance_holder_ptr, (ContextData *)NULL, FENCE_CALLFLAG_UNLOCK_FENCE);

						return 0;
					}

          GenerateZKGroupParams(f_ptr);

					char *s = NULL;
					asprintf(&s, "%s,%s", ld_ptr->country, ld_ptr->locality);
					GeocodeLocation(sesn_ptr, http_ptr, &(f_ptr->fence_location.fence_location), s); free(s);

					UpdateBackendFenceData(sesn_ptr, f_ptr, NULL, EVENT_TYPE_FENCE_CREATED, &((FenceEvent){}));

					counter++;
				}

				free(cn_s); cn_s = NULL;
				f_ptr = FenceOffInstanceHolder(instance_holder_ptr);
				SESSION_BASEFENCE_LOCAL(sesn_ptr) = FENCE_ID(f_ptr);

        if (!fence_already_locked)  FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, THREAD_CONTEXT_UFSRV_RESULT(THREAD_CONTEXT));
        fence_already_locked = false;
			} else {
				//country_NO_admin_area:NO_locality
				//circular logic back top most case
				//char *cn_s=asprintf("%s::", ld_ptr->country);
			}
		}
	} else {//no country no go
	//if (ld_ptr->admin_area&&strlen(ld_ptr->admin_area)>0)//no country no go
		//dont allow for "nocountry:admin:locality type"
		syslog(LOG_ERR, "%s {pid:'%lu', o:'%p', cid:'%lu'}: WILL NOT create a BaseFence for NoCountryDefined", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr));

	}

	return counter;

}

//-------------------------- ADD ROUTINES ------------------------

/**
 *
 * @brief: Main entry point for creating new, fully linked up user fences. Contrasts with AddtoExistingFence.
 * IMPORTANT: if using userfence_canonical_name_in must be previously malloced in the calling environment
 * future version should allow passing in of LocationDescription object
 *
 * @param userfence_canonical_name_in: constructed value. A local copy must be made
 *
 * @locks RW f_ptr: newly created f_ptr returned in locked state
 * @locks: indirectly locks associated with type pool are locked/unlocked as instances are fetched from the pool
 * @locked sesn_ptr: must be locked by the caller
 * @call_flags FENCE_CALLFLAG_KEEP_FENCE_LOCKED:	This function can leave the lock on the fence open if so instructed. It only does that if the outcome
 * 										of the operation was successful.
 * @dynamic_memory userfence_canonical_name_in: creates heap copy if successful
 */
InstanceHolderForFenceStateDescriptor *
CreateUserFenceAndLinkToUser(InstanceHolderForSession *instance_sesn_ptr, const char *fence_banner, char *userfence_canonical_name_in, FenceContextDescriptor *fence_context, unsigned long call_flags)
{
	Fence *f_ptr;
	Session *sesn_ptr = SessionOffInstanceHolder(instance_sesn_ptr);

	if (!IS_STR_LOADED(fence_banner)) {
		syslog (LOG_DEBUG, "%s: ERROR: INVALID fence_banner parameter", __func__);
		return NULL;
	}

	if (!IS_STR_LOADED(userfence_canonical_name_in)) {
		syslog (LOG_DEBUG, "%s: ERROR: INVALID userfence_canonical_name_in parameter", __func__);
		return NULL;
	}

	InstanceHolderForFence *instance_fptr = (InstanceHolderForFence *)RecyclerGet(FencePoolTypeNumber(), (ContextData *)THREAD_CONTEXT_FENCE_CACHEBACKEND, FENCE_CALLFLAG_USERFENCE|FENCE_CALLFLAG_GENERATE_ID);

	if (unlikely(IS_EMPTY(instance_fptr)))	return NULL;

	f_ptr = FenceOffInstanceHolder(instance_fptr);

	//TODO: this would be a major stuffup... as fence would have been in the pool before being unlocked
	FenceEventsLockRWCtx(THREAD_CONTEXT_PTR, f_ptr, _LOCK_TRY_FLAG_TRUE, SESSION_RESULT_PTR(sesn_ptr), __func__);

	if (SESSION_RESULT_TYPE_EQUAL(sesn_ptr, RESULT_TYPE_ERR)) {
		syslog (LOG_ERR, "%s {pid:'%lu', o:'%p', cid:'%lu', cname:'%s'}: ERROR: FETCHED LOCKED FENCE FROM POOL", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), userfence_canonical_name_in);
		RecyclerPut(FencePoolTypeNumber(), instance_fptr, (ContextData *)NULL, CALLFLAGS_EMPTY);
		return NULL;
	}

	//not useful? just come out of recycler...
	bool fence_lock_already_owned = (SESSION_RESULT_CODE_EQUAL(sesn_ptr, RESCODE_PROG_LOCKED_BY_THIS_THREAD));

	PopulateFenceData(f_ptr, sesn_ptr, NULL, fence_banner, userfence_canonical_name_in, true);

	bool  claim_inserted = false;
	void *claim_resident = NULL;

	if (!AddToHashUniquely(FenceRegistryCanonicalNameHashTableProvide(), instance_fptr, &claim_inserted, &claim_resident) || !claim_inserted) {
		if (IS_PRESENT(claim_resident) && claim_resident != instance_fptr)
			syslog(LOG_ERR, "%s {pid:'%lu', o:'%p', cname:'%s'}: ERROR: FENCE NAME ALREADY CLAIMED BY INSTANCE:'%p'", __func__, pthread_self(), instance_fptr, FENCE_CNAME(f_ptr), claim_resident);

		DePopulateFenceData(f_ptr, CALLFLAGS_EMPTY);

		if (!fence_lock_already_owned)	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, SESSION_RESULT_PTR(sesn_ptr));

		RecyclerPut(FencePoolTypeNumber(), instance_fptr, (ContextData *)NULL, CALLFLAGS_EMPTY);

		return NULL;
	}

	if (!AddToHashUniquely(FenceRegistryIdHashTableProvide(), instance_fptr, &claim_inserted, &claim_resident) || !claim_inserted) {
		RemoveFromHash(FenceRegistryCanonicalNameHashTableProvide(), (void *) instance_fptr);

		DePopulateFenceData(f_ptr, CALLFLAGS_EMPTY);

		if (!fence_lock_already_owned)	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, SESSION_RESULT_PTR(sesn_ptr));

		RecyclerPut(FencePoolTypeNumber(), instance_fptr, (ContextData *)NULL, CALLFLAGS_EMPTY);

		return NULL;
	}

	if (IS_PRESENT(fence_context))	fence_context->callbacks.callback_update_fence(fence_context->sesn_ptr, f_ptr, fence_context->context_ptr);

	UpdateBackendFenceData(sesn_ptr, f_ptr, NULL, EVENT_TYPE_FENCE_CREATED, &((FenceEvent){}));

	//>>>>>>>>>>>> Fence fully setup
	unsigned cflags = (CALL_FLAG_FENCE_LIST_CHECK_DUP_SESSION	| CALL_FLAG_SESSION_LIST_CHECK_DUP_FENCE);

	InstanceHolderForFenceStateDescriptor *instance_fstate_ptr = AddUserToThisFenceListWithLinkback(instance_sesn_ptr, instance_fptr, SESSION_FENCE_LIST_PTR(sesn_ptr), &(f_ptr->fence_user_sessions_list),  EVENT_TYPE_FENCE_USER_JOINED, cflags);
	if (IS_PRESENT(instance_fstate_ptr)) {
	  //TODO: this doesn't work with INVITED EVENT; has to be invoked seperately by the caller
		UpdateBackendFenceData(sesn_ptr, f_ptr, FenceStateDescriptorOffInstanceHolder(instance_fstate_ptr), EVENT_TYPE_FENCE_USER_JOINED, &((FenceEvent){}));
	} else {
		RemoveFromHash(FenceRegistryCanonicalNameHashTableProvide(), (void *)instance_fptr);
		RemoveFromHash(FenceRegistryIdHashTableProvide(), (void *)instance_fptr);
		DePopulateFenceData(f_ptr, call_flags);

		if (!fence_lock_already_owned)	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, SESSION_RESULT_PTR(sesn_ptr));

		RecyclerPut(FencePoolTypeNumber(), instance_fptr, (ContextData *)NULL, 0);

		return NULL;
	}

	F_ATTR_UNSET(f_ptr->attrs, F_ATTR_SESSNLIST_LAZY);
	F_ATTR_UNSET(f_ptr->attrs, F_ATTR_DIRTY);

	//success. We only look at this flag if operation was successful
	if (!(call_flags&FENCE_CALLFLAG_KEEP_FENCE_LOCKED)) {
		if (!fence_lock_already_owned)	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, SESSION_RESULT_PTR(sesn_ptr));
	}
	//else Fence returned locked to the caller

	return instance_fstate_ptr;

}

/**
 * 	@brief: Add singular user to Invite list.
 *
 * 	@param sesn_ptr_inviter: can be regular user of ufsrv system user
 * 	@returns: event_id
 * 	@locked sesn_ptr_invited: must be locked by the caller
 * 	@locked sesn_ptr_inviter:	must be locked by the caller
 * 	@locked f_ptr:
 *
 */
UFSRVResult *
AddMemberToInvitedFenceList(InstanceHolderForSession *instance_sesn_ptr_invited, InstanceHolderForFence *instance_f_ptr, Session *sesn_ptr_inviter, unsigned long fence_call_flags)
{
  Session *sesn_ptr_invited = SessionOffInstanceHolder(instance_sesn_ptr_invited);
  Fence *f_ptr = FenceOffInstanceHolder(instance_f_ptr);

	if (!IsUserOnFenceInvitedList(f_ptr, SESSION_USERID(sesn_ptr_invited))) {
		FenceEvent fence_event	=	{0};
		FenceEvent *fe_ev				= NULL;

		InstanceHolderForFenceStateDescriptor *instance_fstate_ptr = RecyclerGet(FenceStateDescriptorPoolTypeNumber(), (ContextData *)instance_f_ptr, CALLFLAGS_EMPTY);
		if (unlikely(IS_EMPTY(instance_fstate_ptr))) {
			syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', cid:'%lu', fid:'%lu'}: ERROR: COULD NOT ALLOCATE FenceStateDescriptor type", __func__, pthread_self(), sesn_ptr_invited, SESSION_ID(sesn_ptr_invited), FENCE_ID(f_ptr));
			_RETURN_RESULT_SESN(sesn_ptr_invited, NULL, RESULT_TYPE_ERR, RESCODE_BACKEND_DATA)
		}

    FenceStateDescriptor *fstate_ptr = FenceStateDescriptorOffInstanceHolder(instance_fstate_ptr);

		FENCESTATE_INSTANCE_HOLDER(fstate_ptr)  =	instance_f_ptr;
		fstate_ptr->when_invited      =	time(NULL);
    memcpy(fstate_ptr->invited_by.data, SESSION_UFSRVUID(sesn_ptr_inviter), CONFIG_MAX_UFSRV_ID_SZ);

		if (fence_call_flags&FENCE_CALLFLAG_WRITEBACK_DATA_TO_BACKEND) {
			if (IS_PRESENT(fe_ev)) { //todo: event is unassigned
				AddThisToList(SESSION_INVITED_FENCE_LIST_PTR(sesn_ptr_invited), instance_fstate_ptr);
				FenceIncrementReference(instance_fstate_ptr, 1);

        AddThisToList(&f_ptr->fence_user_sessions_invited_list, CLIENT_CTX_DATA(instance_sesn_ptr_invited));
				SessionIncrementReference(instance_sesn_ptr_invited, 1);

				_RETURN_RESULT_SESN(sesn_ptr_invited, (void *) (uintptr_t) fe_ev->eid, RESULT_TYPE_SUCCESS, RESCODE_BACKEND_DATA)
			} else {
				syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', cid:'%lu', fid:'%lu', uid_invited:'%lu'}: ERROR: COULD NOT RETRIEVE FENCE EVENT FOR INVITED USER", __func__, pthread_self(), sesn_ptr_invited, SESSION_ID(sesn_ptr_invited), FENCE_ID(f_ptr), SESSION_USERID(sesn_ptr_invited));
				FenceStateDescriptorReturnToRecycler(instance_fstate_ptr, NULL, 0);
				//TODO: what do we do with the generated event? should mark it as failed
			}
		} else {
			AddThisToList (SESSION_INVITED_FENCE_LIST_PTR(sesn_ptr_invited), instance_fstate_ptr);
			FenceIncrementReference(instance_f_ptr, 1);

      AddThisToList(&f_ptr->fence_user_sessions_invited_list, CLIENT_CTX_DATA(instance_sesn_ptr_invited));
			SessionIncrementReference(instance_sesn_ptr_invited, 1);

			_RETURN_RESULT_SESN(sesn_ptr_invited, NULL, RESULT_TYPE_SUCCESS, RESCODE_BACKEND_DATA)
		}
	}

	_RETURN_RESULT_SESN(sesn_ptr_invited, NULL, RESULT_TYPE_ERR, RESCODE_BACKEND_DATA)
}

//--------------------- END OF ADD ROUTINES ----------------------------------
static Fence * _RemoveUserFromUserFence(InstanceHolderForSession *instance_sesn_ptr, Fence *f_ptr, unsigned long call_flags);
static Fence * _RemoveUserFromBaseFence(InstanceHolderForSession *instance_sesn_ptr, Fence *f_ptr, unsigned long call_flags);

//
//export function
//destructive
//all fences
//can be used when a user quit or I/O exception
//>>>>> LOCKS f_ptr <<<<<<
//session must be locked
// session for sesn_ptr_target can be different from sesn_ptr_this. For example connected session is used to kill a remote session
//
/*	@brief: this can be called from
 * 	@param sesn_ptr_target: must have full access context.
 * 	@worker: ufsrv, session
 */
unsigned
RemoveUserFromAllFences(InstanceHolderForSession *instance_sesn_ptr_target, unsigned long call_flags)
{
	//we approach it from the User's fence list (which is hybrid User and Base) instead of Searching the
	//Master Registeries for the User: both should
	//yield the same effect due to stringent referencial data integrity checks throughout

	Fence *f_ptr			=	NULL;
	ListEntry *eptr		=	NULL;
	Session *sesn_ptr_target = SessionOffInstanceHolder(instance_sesn_ptr_target);
	SessionService *ss_ptr  = SESSION_SERVICE(sesn_ptr_target);
	int error = 0;

	//NEVER USE FOR LOOP CONSTRUCT FOR ITERATION
	Queue 					tmp_que;
	FenceStateDescriptor	*fence_state_descriptor = NULL;
	InstanceHolderForFenceStateDescriptor *instance_fstate_ptr;

	size_t list_count = SESSION_FENCE_LIST_SIZE(sesn_ptr_target);//prevent going on endless loop where one of the feneces could not be unlocked
	while (SESSION_FENCE_LIST_SIZE(sesn_ptr_target) != 0 && list_count--) {
		eptr = SESSION_FENCE_LIST(sesn_ptr_target).head;
		if (IS_PRESENT(eptr)) {
			fence_state_descriptor = FenceStateDescriptorOffInstanceHolder((InstanceHolderForFenceStateDescriptor *)eptr->whatever);
			f_ptr = FENCESTATE_FENCE(fence_state_descriptor);

			FenceEventsLockRWCtx(THREAD_CONTEXT_PTR, f_ptr, _LOCK_TRY_FLAG_TRUE, SESSION_RESULT_PTR(sesn_ptr_target), __func__);
			if (SESSION_RESULT_TYPE_EQUAL(sesn_ptr_target, RESULT_TYPE_ERR)) {
				error++;
				//TODO: POP the fence off forcibly from user session this can cause endless loop if a fence is locked
				continue;
			}

			bool fence_lock_already_owned = (SESSION_RESULT_CODE_EQUAL(sesn_ptr_target, RESCODE_PROG_LOCKED_BY_THIS_THREAD));

			if (F_ATTR_IS_SET(f_ptr->attrs, F_ATTR_BASEFENCE)) {
#ifdef __UF_FULLDEBUG
				syslog (LOG_DEBUG, "%s: REMOVING (cid='%lu' uname='%s' fence_count='%d') from BaseFence (bid='%lu fcname='%s' user_count='%d')...",
									 __func__, sesn_ptr->ufsrv_session.session_id,
									 ss_ptr->user.user_details.user_name, SESSION_FENCE_LIST_SIZE(sesn_ptr_target),
									 f_ptr->fence_id, f_ptr->fence_location.canonical_name, f_ptr->fence_user_sessions_list.nEntries);
#endif

				_RemoveUserFromBaseFence(instance_sesn_ptr_target, f_ptr, call_flags);
			} else if (F_ATTR_IS_SET(f_ptr->attrs, F_ATTR_USERFENCE)) {
#ifdef __UF_FULLDEBUG
				syslog (LOG_DEBUG, "%s: REMOVING (cid='%lu' uname='%s' fence_count='%d') from UserFence (bid='%lu fcname='%s' user_count='%d')...",
										 __func__, sesn_ptr->ufsrv_session.session_id,
										 ss_ptr->user.user_details.user_name, SESSION_FENCE_LIST_SIZE(sesn_ptr_target),
										 f_ptr->fence_id, f_ptr->fence_location.canonical_name, f_ptr->fence_user_sessions_list.nEntries);
#endif

				_RemoveUserFromUserFence(instance_sesn_ptr_target, f_ptr, call_flags);
			} else {
				//this is a truly sad condition... something is screwed up
#ifdef __UF_TESTING
				syslog (LOG_INFO, "%s: ERROR: COULD NOT DETERMINE FENCE TYPE  (fid='%lu fcname='%s' user_count='%d') FOR (cid='%lu' uname='%s' fence_count='%d')...",
						__func__, f_ptr->fence_id, f_ptr->fence_location.canonical_name, f_ptr->fence_user_sessions_list.nEntries,
						SESSION_ID(sesn_ptr_target),
						ss_ptr->user.user_details.user_name, SESSION_FENCE_LIST_SIZE(sesn_ptr_target));
#endif
				error++;
			}

			if (!fence_lock_already_owned)	FenceEventsUnLockCtx(THREAD_CONTEXT_PTR, f_ptr, SESSION_RESULT_PTR(sesn_ptr_target));
		}
	}//while

	if (error) {
		syslog (LOG_INFO, "%s: ERROR (error_count='%d'): SOME FENCES HAD UNKNOWN TYPE OR COULD NOT BE LOCKED...  (o:'%p', cid:'%lu', fence_count:'%d')...", __func__, error, sesn_ptr_target, SESSION_ID(sesn_ptr_target), SESSION_FENCE_LIST_SIZE(sesn_ptr_target));

		return 0;
	}

	return 1;//all good

}

//
//essentially we should be able to cross reference the user across the fence and fence across the user
//ONE USER_FENCE at a time
//can be used in a loop to iterate over all FENCE-USER twins
//works for both USerFence and BAseFence
//important data integrity check
//bits are set to indicate status of linking
//f_ptr and session must be locked in the calling environment
//
static unsigned
_CrossCheckUserInFenceAndFenceInUser(Fence *f_ptr, Session *sesn_ptr)
{
  unsigned 	score	= 0x1;
  ListEntry 	*eptr	= NULL;
  InstanceHolderForSession *instance_sesn_ptr;

  //first check user in Fence's List
  for (eptr=f_ptr->fence_user_sessions_list.head; eptr; eptr=eptr->next) {
    Session *sesn_ptr_from_list = SessionOffInstanceHolder((InstanceHolder *)eptr->whatever);
    if (SESSION_ID(sesn_ptr_from_list) == SESSION_ID(sesn_ptr)) {
      score |= FENCE_USER_LIST;
      break;
    }
  }

  FenceStateDescriptor *fence_state_descriptor	= NULL;

  //second check fence in Users's List
  for (eptr=SESSION_FENCE_LIST(sesn_ptr).head; eptr; eptr=eptr->next) {

    fence_state_descriptor = FenceStateDescriptorOffInstanceHolder((InstanceHolder *)eptr->whatever);
    if (FENCE_ID(FENCESTATE_FENCE(fence_state_descriptor)) == FENCE_ID(f_ptr)) {
      score |= USER_FENCE_LIST;
      break;
    }
  }

  return score;

}

inline static unsigned _CrossCheckUserInFenceAndFenceInUser(Fence *, Session *);

//
//this a high level removal routine, employing lower level helper routines defined below
//Remove user from ONE UserFence and unlinks as necessary
//Fence will be destroyed if fence stickiness rule is not satisfied. Destruction happens in another helper routine
//SessionService and User are not touched
//a mirror call is required for BaseFences.
//can be used in a loop to iterate over all user fences. perhaps not efficient, but safe and predictable

//
/**
 * 	@brief: A highlevel removal routine for removing and unlinking users from user fence
 * 	@locked f_ptr: must be in RW locked
 * 	@locked sesn_ptr:
 * 	@call_flag CALL_FLAG_DONT_BROADCAST_FENCE_EVENT: if not present, the event will be broadcasted
 */
static Fence *
_RemoveUserFromUserFence(InstanceHolderForSession *instance_sesn_ptr, Fence *f_ptr, unsigned long call_flags)
{
	unsigned score = 0;
	Session *sesn_ptr = SessionOffInstanceHolder(instance_sesn_ptr);

	//integrity check: is this user member of the fence
	if ((score = _CrossCheckUserInFenceAndFenceInUser(f_ptr, sesn_ptr))) {
		FenceEvent fence_event = {0};

		if ((score&USER_FENCE_LIST) && (score&FENCE_USER_LIST)) {
		  InstanceHolderForFenceStateDescriptor *instance_fstate_ptr;
			FenceStateDescriptor *fence_state_ptr_leaving	=	NULL;
			FenceStateDescriptor	fence_state_for_invited_by_only = {0};

			instance_fstate_ptr = IsUserMemberOfThisFence(SESSION_INVITED_FENCE_LIST_PTR(sesn_ptr), f_ptr, false/*FLAG_FENCE_LOCK_FALSE*/);

			if (IS_PRESENT(instance_fstate_ptr)) {
        //we need this value to remove record bewe lose the stateahead of updating the backend
        fence_state_ptr_leaving = FenceStateDescriptorOffInstanceHolder(instance_fstate_ptr);
        fence_state_for_invited_by_only.invited_by = fence_state_ptr_leaving->invited_by;
      }
#if 0
			//is the user owns the user fence
			if (f_ptr->fence_owner_id==ss_ptr->user.user_details.id)
			{
				//TODO: OK we are not processing this condition at this stage
				syslog(LOG_INFO, "%s: Fence owner is leaving", __func__);
			}
#endif
			if (f_ptr->fence_user_sessions_list.nEntries == 1) {
			 //OK last user in Fence we'll destruct the fence unless sticky
				//if (F_ATTR_IS_SET(f_ptr->attrs, F_ATTR_STICKY))//AA 0810
				{//sticky bit is on we'll leave the fence up, but stil unlink user
					syslog(LOG_DEBUG, "%s: STICKY BIT IS ON: Fence owner is leaving: user count will drop to zero.", __func__);
					RemoveUserFromUserFenceAndUnlinkUser(f_ptr, instance_sesn_ptr, false/*dangling_session*/);
				}
			} else {//more users remaining in the fence we just unlink user
				RemoveUserFromUserFenceAndUnlinkUser(f_ptr, instance_sesn_ptr, false/*dangling_session*/);
			}

			if (!(call_flags&CALL_FLAG_DONT_BROADCAST_FENCE_EVENT)) {
				if (unlikely(IS_EMPTY(UpdateBackendFenceData(sesn_ptr, f_ptr, &fence_state_for_invited_by_only, EVENT_TYPE_FENCE_USER_PARTED, &((FenceEvent){})))))
					return NULL;
			}

			return f_ptr;
		} else {//one of the bits is unset
			syslog(LOG_DEBUG, "%s {pid:'%lu'}: one bit is unset", __func__, pthread_self());

			return NULL;
		}
	} else {
		syslog(LOG_DEBUG, "%s {pid:'%lu'}: COULD NOT find user", __func__, pthread_self());

		return NULL;
	}

}

static Fence *_RemoveUserFromBaseFenceAndUnlinkUser(Fence *, InstanceHolderForSession *, bool);

/**
 * 	@brief: non destructive removal of referenced Fence and Session entities from each other
 * 	@locked: both Fence andSession must be locked by the caller
 * 	@locks: Directly done: bu decrementing refrences locks type recycler resources associated with Fence and Session
 */
static Fence *
_RemoveUserFromBaseFenceAndUnlinkUser(Fence *f_ptr, InstanceHolderForSession *instance_sesn_ptr, bool dangling_session_status)
{
  return RemoveUserFromUserFenceAndUnlinkUser(f_ptr, instance_sesn_ptr, dangling_session_status);

}

/**
 * 	@brief: A highlevel removal routine for removing user from base fence. base fences are never destroyed.
 * 	@locked f_ptr: must be in RW locked
 * 	@locked sesn_ptr: target session. Must be fully loaded
 * 	@call_flag CALL_FLAG_DONT_BROADCAST_FENCE_EVENT: if not present, the event will be broadcasted
 */
static Fence *
_RemoveUserFromBaseFence(InstanceHolderForSession *instance_sesn_ptr, Fence *f_ptr, unsigned long call_flags)
{
	unsigned score = 0;
	Session *sesn_ptr = SessionOffInstanceHolder(instance_sesn_ptr);

	//integrity check: is this user member of the fence
	if ((score = _CrossCheckUserInFenceAndFenceInUser(f_ptr, sesn_ptr))) {
		FenceEvent *fe_ptr;

		if ((score&USER_FENCE_LIST) && (score&FENCE_USER_LIST)) {//both bits are set
			FenceStateDescriptor *fence_state_ptr_leaving	=	NULL;
			InstanceHolderForFenceStateDescriptor *instance_fstate_ptr;
			FenceStateDescriptor	fence_state_for_invited_by_only = {0};

			instance_fstate_ptr = IsUserMemberOfThisFence(SESSION_INVITED_FENCE_LIST_PTR(sesn_ptr), f_ptr, false/*FLAG_FENCE_LOCK_FALSE*/);
			if (IS_PRESENT(instance_fstate_ptr)) {
        fence_state_ptr_leaving = FenceStateDescriptorOffInstanceHolder(instance_fstate_ptr);
        //we need this value to remove record bewe lose the stateahead of updating the backend, ALTHOUGH FOR BASEFENCE VALUE IS ALWAY 0
        fence_state_for_invited_by_only.invited_by = fence_state_ptr_leaving->invited_by;
      }

			if (!(call_flags&CALL_FLAG_DONT_BROADCAST_FENCE_EVENT)) {
				FenceEvent fence_event = {0};
				//for remote sessions the above flag prohibits generations of fence event. We also
				//prohibit netwrk broadcast for the same reason below.
				//this function require connected session to intercat with backends and remote sessions dont have that
				FenceEvent *fe_ptr = RegisterFenceEvent(f_ptr, EVENT_TYPE_FENCE_USER_PARTED,  NULL, 0, &fence_event);

				if (unlikely(IS_EMPTY(fe_ptr)))	return NULL;

        fe_ptr->originator_ptr         = &SESSION_UFSRVUIDSTORE(sesn_ptr);
        fe_ptr->session_id	           =	SESSION_ID(sesn_ptr);
        DbBackendInsertUfsrvEvent ((UfsrvEvent *)fe_ptr);
			}
#if 0
			//is the user owns the user fence
			if (f_ptr->fence_owner_id==ss_ptr->user.user_details.id)
			{
				//TODO: OK we are not processing this condition at this stage
				syslog(LOG_DEBUG, "%s: Fence owner is leaving", __func__);
			}
#endif
			if (f_ptr->fence_user_sessions_list.nEntries == 1) {//OK last user in Fence is leaving, but unlike UserFence we never destruct upon this condition
#ifdef __UF_TESTING
				syslog(LOG_DEBUG, "%s: BaseFence count will drop to zero.", __func__);
#endif
				_RemoveUserFromBaseFenceAndUnlinkUser(f_ptr, instance_sesn_ptr, false/*dangling_session*/);
			} else {//more users remaining in the fence we just unlink user
				_RemoveUserFromBaseFenceAndUnlinkUser(f_ptr, instance_sesn_ptr, false/*dangling_session*/);
			}

			if (!(call_flags&CALL_FLAG_DONT_BROADCAST_FENCE_EVENT)) {
				//no need for explicit FenceEvent object
				UpdateBackendFenceData (sesn_ptr, f_ptr, &fence_state_for_invited_by_only, EVENT_TYPE_FENCE_USER_PARTED, &((FenceEvent){}));
			}

			return f_ptr;
		} else {//one of the bits is unset
			syslog(LOG_DEBUG, "%s: one bit is unset", __func__);
			return NULL;
		}
	}//find user
	else {//user was not found
		syslog(LOG_DEBUG, "%s (pid:'%lu'): COULD NOT find user", __func__, pthread_self());

		return NULL;
	}

}

/**
 * 	@brief: Remove user from a Fence based on  fenceid

 * 	@locks f_ptr_in: NONE
 * 	@unlocks f_ptr_in
 * 	@locked RW f_ptr: must be locked by the caller
 * 	@locked RW sesn_ptr_target: must be locked by the caller nd fully loaded with context

 * 	@call_flag CALL_FLAG_DONT_BROADCAST_FENCE_EVENT: if not present, the event will be broadcasted Remote Sessions may set that.
 * 	@call_flag FENCE_CALLFLAG_ROAMING_GEOFENCE: if not present, update the update this property of and broadcast backend change

 * 	@returns: on success the eventid>0
 */
unsigned long
RemoveUserFromMembersListForFence(InstanceHolderForSession *instance_sesn_ptr_target, Fence *f_ptr_in, unsigned long call_flags)
{
	Fence									*f_ptr						= NULL;
	FenceStateDescriptor 	*fence_state_ptr	=	NULL;
	ListEntry							*eptr							= NULL;

	Session *sesn_ptr_target                = SessionOffInstanceHolder(instance_sesn_ptr_target);
	SessionService				*ss_ptr						= &(sesn_ptr_target->ufnet_session.sservice);

	//this is integrity check could be relaxed in the future
  InstanceHolderForFenceStateDescriptor *instance_fstate_ptr = FindFenceInUserListByID(SESSION_FENCE_LIST_PTR(sesn_ptr_target), FENCE_ID(f_ptr_in));
	if (IS_PRESENT(instance_fstate_ptr)) {
	  fence_state_ptr = FenceStateDescriptorOffInstanceHolder(instance_fstate_ptr);

    if (IS_PRESENT((f_ptr = FENCESTATE_FENCE(fence_state_ptr))) &&
        IS_PRESENT(FindUserInFenceSessionListByID(&FENCE_USER_SESSION_LIST(f_ptr_in), f_ptr_in, SESSION_ID(sesn_ptr_target)))) {

      if (F_ATTR_IS_SET(f_ptr->attrs, F_ATTR_BASEFENCE)) {
#ifdef __UF_TESTING
        syslog(LOG_DEBUG,
               "%s: REMOVING (cid:'%lu' uname:'%s' fence_count:'%d') from BaseFence (bid='%lu fcname='%s' user_count='%d')...",
               __func__,
               SESSION_ID(sesn_ptr_target), SESSION_USERNAME(sesn_ptr_target),
               SESSION_FENCE_LIST_SIZE(sesn_ptr_target),
               FENCE_ID(f_ptr), FENCE_CNAME(f_ptr), FENCE_USER_SESSION_LIST_SIZE(f_ptr));
#endif
        _RemoveUserFromBaseFence(instance_sesn_ptr_target, f_ptr, call_flags);

        //
        if (!(call_flags & FENCE_CALLFLAG_ROAMING_GEOFENCE)) {
          if (IS_PRESENT(SESSION_GEOFENCE_CURRENT_INSTANCE(sesn_ptr_target)) &&
              FENCE_ID(f_ptr_in) == FENCE_ID(SESSION_GEOFENCE_CURRENT(sesn_ptr_target))) {
            FenceDecrementReference(SESSION_GEOFENCE_CURRENT_INSTANCE(sesn_ptr_target), 1);
            SESSION_GEOFENCE_CURRENT_INSTANCE(sesn_ptr_target) = NULL;

            if (!(call_flags & CALL_FLAG_DONT_BROADCAST_FENCE_EVENT))
              UpdateBackendSessionGeoJoinData(sesn_ptr_target, NULL, IS_PRESENT(SESSION_GEOFENCE_LAST_INSTANCE(sesn_ptr_target))?SESSION_GEOFENCE_LAST(sesn_ptr_target):NULL);
          }
        }

        return f_ptr->fence_events.event_id;//success case
      } else if (F_ATTR_IS_SET(f_ptr->attrs, F_ATTR_USERFENCE)) {
#ifdef __UF_TESTING
        syslog(LOG_DEBUG,
               "%s: REMOVING (cid:'%lu' uname:'%s' fence_count:'%d') from UserFence (bid='%lu fcname='%s' user_count='%d')...",
               __func__,
               SESSION_ID(sesn_ptr_target),
               SESSION_USERNAME(sesn_ptr_target), SESSION_FENCE_LIST_SIZE(sesn_ptr_target),
               FENCE_ID(f_ptr), FENCE_CNAME(f_ptr), f_ptr->fence_user_sessions_list.nEntries);
#endif

        _RemoveUserFromUserFence(instance_sesn_ptr_target, f_ptr, call_flags);//mark for destruction if count is zero

        return f_ptr->fence_events.event_id;//success case
      } else {
        //this is a truly sad condition... something is screwed up
        syslog(LOG_ERR,
               "%s: ERROR: COULD NOT DETERMINE FENCE TYPE  (bid:'%lu fcname:'%s' user_count:'%d') FOR (cid='%lu' uname='%s' fence_count='%d')...",
               __func__,
               f_ptr->fence_id, f_ptr->fence_location.canonical_name, f_ptr->fence_user_sessions_list.nEntries,
               SESSION_ID(sesn_ptr_target), SESSION_USERNAME(sesn_ptr_target),
               SESSION_FENCE_LIST_SIZE(sesn_ptr_target));
      }

    } else goto return_error_not_found;
  } else {
	  goto return_error_not_found;
	}

	return 0;

  return_error_not_found:
  syslog (LOG_ERR, "%s {pid:'%lu', o:'%p', fo:'%p', cid:'%lu', bid:'%lu', uname:'%s', user_fence_count:'%d', fence_user_count:'%d'}: DATA SYNC ERROR: COULD NOT REMOVE UserFence: NOT IN USER'S FENCE LIST...", __func__,
          pthread_self(), sesn_ptr_target, f_ptr_in, SESSION_ID(sesn_ptr_target),
          FENCE_ID(f_ptr_in),
          SESSION_USERNAME(sesn_ptr_target), SESSION_FENCE_LIST_SIZE(sesn_ptr_target), f_ptr_in->fence_user_sessions_list.nEntries);

  return 0;

}\

/**
 * @brief Remove a fence from user's invited-to fences list
 * 	@param instance_sesn_ptr: Fully loaded Session in connected or Emphemeral mode which is target for removal
 * 	@param fence_state_joined: Fence to which user has invitation. May only be a shell struct with fence reference only known.
 * 	@param fence_call_flags CALL_FLAG_WRITEBACK_FENCE_DATA_TO_BACKEND:
* 	@param fence_call_flags FENCE_CALLFLAG_TRANSFER_INVITE_CONTEXT: indicates invited fence is becoming a joined so need invite context transfered
 * 	@locked sesn_ptr_this, sesn_ptr_target: must be locked by the caller
 * 	@locked WR f_ptr: must be locked in the caller's environment
 * 	@returns: success is where the fence has been removed 2x therefore returning 2. 0 empty list. <0 error
 *
 * 	@worker: ufsrv, io
 */
int
RemoveUserFromInvitedList(InstanceHolderForSession *instance_sesn_ptr, FenceStateDescriptor *fence_state_invited, FenceEvent *fe_ptr_out, unsigned long fence_call_flags)
{
	int		rc			= 0;
  Session *sesn_ptr = SessionOffInstanceHolder(instance_sesn_ptr);
	Fence *f_ptr = FenceOffInstanceHolder(fence_state_invited->instance_holder_fence);

	if (FENCE_INVITED_LIST_EMPTY(f_ptr)) {
		return rc;
	}

	//1) remove fence reference from Session's invite list (referenced as FenceStateDescriptor)
  InstanceHolderForFenceStateDescriptor *instance_fstate_ptr = RemoveFenceFromSessionInvitedFenceList(SESSION_INVITED_FENCE_LIST_PTR(sesn_ptr), FENCE_ID(f_ptr));

	if (IS_PRESENT(instance_fstate_ptr)) {
    FenceStateDescriptor *fence_state_descriptor = FenceStateDescriptorOffInstanceHolder(instance_fstate_ptr);
#ifdef __UF_FULLDEBUG
		syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', fid:'%lu', invite_list_sz:'%u'} Fence removed from Session's InviteList...", __func__, pthread_self(), sesn_ptr, FENCE_ID(f_ptr), SESSION_INVITED_FENCE_LIST_SIZE(sesn_ptr));
#endif

	//2)remove session from Fence's invite list
		if (RemoveThisFromList(&(FENCE_INVITED_USER_SESSIONS_LIST(f_ptr)), instance_sesn_ptr)) {
			SessionDecrementReference(instance_sesn_ptr, 1);
			rc++;
#ifdef __UF_FULLDEBUG
		syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', fid:'%lu', invite_list_sz:'%u'} Session removed from Fences's InviteList...", __func__, pthread_self(), sesn_ptr, FENCE_ID(f_ptr), FENCE_INVITED_USER_SESSIONS_LIST_SIZE(f_ptr));
#endif
		}

		if (fence_call_flags&FENCE_CALLFLAG_TRANSFER_INVITE_CONTEXT) {
			memcpy(&(fence_state_invited->invited_by), &(fence_state_descriptor->invited_by), sizeof(UfsrvUid));
			fence_state_invited->when_invited = fence_state_descriptor->when_invited;
		}

		if (fence_call_flags&FENCE_CALLFLAG_WRITEBACK_DATA_TO_BACKEND) {
			UpdateBackendFenceInvitedData(NULL, sesn_ptr, fence_state_descriptor, fe_ptr_out->event_type, fe_ptr_out);
		}

		FenceDecrementReference(fence_state_invited->instance_holder_fence, 1);
		RecyclerPut(FenceStateDescriptorPoolTypeNumber(), (RecyclerClientData *)instance_fstate_ptr, (ContextData *)NULL, CALLFLAGS_EMPTY);
		rc++;

		return rc;
	}

	syslog(LOG_ERR, "%s {pid:'%lu', o:'%p', fo:'%p'}: FAILED TO ACQUIRE A VALID FenceStateDescriptor..", __func__, pthread_self(), sesn_ptr, f_ptr);

	return 0;
}