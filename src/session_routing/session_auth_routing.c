/**
 * @file session_auth_routing.c
 * @brief Authenticate and route sessions while protecting sign-on claims and holder lifetimes.
 */

#ifdef HAVE_CONFIG_UFSRVWEBSOCK_H
#include "config_ufsrvwebsock.h"
#endif

#include <sys/prctl.h>
#include <session_auth_guard.h>
#include <ufnetcorelib/ufnet_msgcore/type_providers/session_provider.h>
#include <ufsrv_core/delegator_sessions/thread_context_type.h>

// ufsrv_core
#include <ufsrv_core/delegator_sessions/delegator_session_worker_thread.h>
#include <ufsrv_core/cache_backend/persistence.h>
#include <ufsrvcorelib/ufsrv_core/instrumentation/instrumentation_backend.h>

// ufsrv_msgcore
#include <ufnetcorelib/ufnet_msgcore/session.h>
#include <ufnetcorelib/ufnet_core/session/session_type.h>
#include <ufsrv_core/ufsrvresult_type.h>
#include <ufnetcorelib/ufnet_msgcore/attachments.h>
#include <ufnetcorelib/ufnet_msgcore/fence/fence.h>
#include <ufnetcorelib/ufnet_msgcore/user/user_backend.h>
#include <ufnetcorelib/ufnet_msgcore/user/user_profile.h>

// ufsrvwebsock (local — same module)
#include "protocol_websocket.h"
#include "protocol_websocket_io.h"
#include "ufsrvcmd_user_callbacks.h"
#include "command_broadcast/command_broadcast_session.h"
#include <ufsrv_core/protocol/protocol_type.h>

#include <uflib/ufsrvuid.h>

// System
#include <syslog.h>
#include <pthread.h>
#include <poll.h>
#include <sys/socket.h>
#include <uflib/net/utils_net.h>
#include <ufsrv_core/system/log_message_literals.h>

#include <hiredis/hiredis.h>

#include "ufsrv_core/ufsrv_config/generated/ufsrv_config_accessor_decls_generated.h"

// ── Forward declarations (static helpers) ──
static UFSRVResult *_AuthenticateBrandNewSession(InstanceHolderForSession *instance_sesn_ptr);
static UFSRVResult *_AuthenticateUserIdHashedSession(InstanceHolderForSession *instance_sesn_ptr, InstanceHolderForSession *instance_sesn_ptr_uid_hashed, unsigned long);
static UFSRVResult *_AuthenticateDbBackendValidatedCookieSession(InstanceHolderForSession *instance_sesn_ptr, redisReply *redis_ptr, unsigned long);
static UFSRVResult *_InstateAuthenticatedNonLocalUser(InstanceHolderForSession *instance_sesn_ptr_transient, SocketMessage *sock_msg_ptr, redisReply *redis_ptr);
static UFSRVResult *_RefreshAuthenticatedLocalUser(InstanceHolderForSession *instance_sesn_ptr, InstanceHolderForSession *instance_sesn_ptr_hashed, SocketMessage *sock_msg_ptr, redisReply *redis_ptr);
static UFSRVResult *_InstateRemoteSessionAsConnected(InstanceHolderForSession *instance_sesn_ptr, InstanceHolderForSession *instance_sesn_ptr_hashed, redisReply *redis_ptr);
static UFSRVResult *_InstateSuspendedSessionAsConnected(InstanceHolderForSession *instance_sesn_ptr, InstanceHolderForSession *instace_sesn_ptr_hashed, SocketMessage *sock_msg_ptr);
static UFSRVResult  *_DbBackendVerifyAndInitialiseSession(InstanceHolderForSession *instance_sesn_ptr);
static UFSRVResult *_DiscoverSessionWhereabouts(InstanceHolderForSession *instance_sesn_ptr, unsigned long);
static UFSRVResult *_InstateUnconnectedSessionAsConnected(InstanceHolderForSession *instance_sesn_ptr_migrated, InstanceHolderForSession *instance_new_sesn_ptr, unsigned long call_flags, UFSRVResult *res_ptr_in);
static bool _SetupLocalCachingForBrandNewSession(InstanceHolderForSession *instance_sesn_ptr);
static int sIsSocketAlive(int socket);
static bool sShouldDeclineSignon(Session *session);
static  UFSRVResult * _InvokeLifecycleCallbackPostHandshake(InstanceHolderForSession *instance_sesn_ptr, SocketMessage *sock_msg_ptr);
// ── Public entry points ──
static UFSRVResult *_AuthenticateForNonCookieHashedSession(InstanceHolderForSession *instance_sesn_ptr_transient);
static UFSRVResult *_AuthenticateForBackendCookieHashedSession(InstanceHolderForSession *instance_sesn_ptr_transient, UFSRVResult *res_ptr, SocketMessage *sock_msg_ptr);
static UFSRVResult *HandleSessionReturnHandshake(InstanceHolderForSession *instance_sesn_ptr, SocketMessage *sock_msg_ptr, unsigned long sesn_call_flags);

extern __thread ThreadContext ufsrv_thread_context;
extern  const  Protocol *const protocols_registry_ptr;

static SessionAuthClaims sAuthClaims = {.mutex = PTHREAD_MUTEX_INITIALIZER};
static _Thread_local SessionAuthClaim *sAuthClaim;
static _Thread_local InstanceHolderForSession *sAuthLockedTarget;

/**
 * @brief Retain the lookup result while its hash-table lock still protects publication.
 * @param[in,out] holder Holder protected by the hash lookup callback.
 * @param[in] count Number of recycler references to acquire.
 */
static void
sPinAuthHolder(RecyclerClientData *holder, int count)
{
  SessionIncrementReference((InstanceHolderForSession *)holder, count);
}

/**
 * @brief Release a scoped lookup pin on every return path.
 * @param[in,out] holder Address of a nullable lookup holder owned by this scope.
 */
static void
sReleaseAuthHolder(InstanceHolderForSession **holder)
{
  if (*holder) SessionDecrementReference(*holder, 1);
}

/**
 * @brief Bind the active handshake claim before mutating resolved account state.
 * @param[in] uid Verified nonzero account ID.
 * @return true when this thread owns the account claim; false when unavailable.
 */
static bool
sClaimAuthUser(unsigned long uid)
{
  return SessionAuthClaimUser(&sAuthClaims, sAuthClaim, uid);
}

/**
 * @brief Try-lock and revalidate a pinned target, retaining new lock ownership through routing.
 * @param[in] holder Referenced target, or NULL when lookup found nothing.
 * @param[in] uid Expected account ID.
 * @param[in] expected_cid Expected session ID, or zero for a UID-only lookup.
 * @return true when the target is locked and valid; false when the incoming sign-on must be rejected.
 * @note The outer handshake transfers the lock to the returned holder or releases it on failure, including failed suspension.
 */
static bool
sLockAuthTarget(InstanceHolderForSession *holder, unsigned long uid, unsigned long expected_cid)
{
  if (!holder) return false;
  Session *session = SessionOffInstanceHolder(holder);
  SessionLockRWCtx(THREAD_CONTEXT_PTR, AS_UFSRV_SESSION(session), _LOCK_TRY_FLAG_TRUE, __func__);
  if (!THREAD_CONTEXT_UFSRV_RESULT_TYPE_SUCCESS) return false;
  bool owned = _RESULT_CODE_EQUAL(THREAD_CONTEXT_UFSRV_RESULT(THREAD_CONTEXT), RESCODE_PROG_LOCKED_BY_THIS_THREAD);
  if (SESSION_USERID(session) != uid || !SESSION_ID(session) ||
      (expected_cid && SESSION_ID(session) != expected_cid) ||
      SESNSTATUS_IS_SET(session->ufsrv_session.stat, SESNSTATUS_RECYCLED | SESNSTATUS_TRANSIENT) ||
      sShouldDeclineSignon(session)) {
    if (!owned) SessionUnLockCtx(THREAD_CONTEXT_PTR, AS_UFSRV_SESSION(session), __func__);
    return false;
  }
  if (!owned) {
    if (sAuthLockedTarget && sAuthLockedTarget != holder) {
      SessionUnLockCtx(THREAD_CONTEXT_PTR, AS_UFSRV_SESSION(session), __func__);
      return false;
    }
    SessionIncrementReference(holder, 1);
    sAuthLockedTarget = holder;
  }
  if (SESNSTATUS_IS_SET(session->ufsrv_session.stat, SESNSTATUS_CONNECTED) &&
      !SESNSTATUS_IS_SET(session->ufsrv_session.stat, SESNSTATUS_SUSPENDED)) {
    if (!SuspendSession(holder, SOFT_SUSPENSE)) return false;
  }
  return true;
}

//Temporary replacement of uflib's IsSocketAlive()
static int
sIsSocketAlive(int socket)
{
	struct pollfd  pfd   = { .fd = socket, .events = POLLIN|POLLRDHUP, .revents = 0 };
	char           probe = 0;
	int            rc;

	if (socket < 0)	return EBADF;

	if ((rc = poll(&pfd, 1, 0)) < 0)		return errno ? errno : -1;
	if (rc == 0)							return 0;

	if (pfd.revents & POLLNVAL)				return EBADF;
	if (pfd.revents & (POLLERR|POLLHUP|POLLRDHUP))	return ECONNRESET;

	if ((rc = recv(socket, &probe, 1, MSG_PEEK|MSG_DONTWAIT)) == 0)	return ECONNRESET;
	if (rc < 0)		return ((errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR)) ? 0 : errno;

	return 0;
}

/**
 * @brief Preserve a connected session only when socket liveness and service age agree.
 * @param[in] session Target session whose write lock the caller owns.
 * @return true to decline the newcomer; false to permit takeover under the same lock.
 * @note Uses the timer's connected/locationless idle limits and the unfinished-service timeout.
 */
static bool
sShouldDeclineSignon(Session *session)
{
  if (!SESNSTATUS_IS_SET(session->ufsrv_session.stat, SESNSTATUS_CONNECTED) ||
      SESNSTATUS_IS_SET(session->ufsrv_session.stat, SESNSTATUS_SUSPENDED)) return false;
  unsigned long idle_timeout = SessionsDelegatorPtr()->user_timeouts.connected;
  unsigned long locationless_timeout = SessionsDelegatorPtr()->user_timeouts.locationless;
  if (!SESNSTATUS_IS_SET(session->ufsrv_session.stat, SESNSTATUS_LOCATED) && locationless_timeout &&
      (!idle_timeout || locationless_timeout < idle_timeout)) idle_timeout = locationless_timeout;
  return SessionAuthConnectionIsLive(session->ufsrv_session.ssptr && sIsSocketAlive(SESSION_SOCKETFD(session)) == 0,
                                     SESNSTATUS_IS_SET(session->ufsrv_session.stat, SESNSTATUS_INSERVICE), time(NULL),
                                     SESSION_WHEN_SERVICE_STARTED(session), SESSION_WHEN_SERVICED(session),
                                     idle_timeout, CONFIG_SESSION_INSERVICE_MAX_TIMEOUT);
}

UFSRVResult *
AuthenticateForCookieHashedSession(InstanceHolderForSession *instance_sesn_ptr_transient, InstanceHolderForSession *instance_sesn_ptr_hashed, SocketMessage *sock_msg_ptr)
{
	Session     *sesn_ptr_hashed    = SessionOffInstanceHolder(instance_sesn_ptr_hashed);
  Session     *sesn_ptr_transient = SessionOffInstanceHolder(instance_sesn_ptr_transient);

	SessionLockRWCtx(THREAD_CONTEXT_PTR, AS_UFSRV_SESSION(sesn_ptr_hashed), _LOCK_TRY_FLAG_TRUE, __func__);

  //region session is locked and unresponsive
  if (_RESULT_TYPE_EQUAL(THREAD_CONTEXT_UFSRV_RESULT(THREAD_CONTEXT), RESULT_TYPE_ERR)) {

		syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p', o_hashed:'%p', cid:'%lu', cid_hashed:'%lu'): ERROR: THIS COOKIE BELONGS TO AN ACTIVE SESSION/OR IS IN-SERVICE: TERMINATING THIS INCOMING CONNECTION...", __func__, pthread_self(), sesn_ptr_transient, sesn_ptr_hashed, SESSION_ID(sesn_ptr_transient), SESSION_ID(sesn_ptr_hashed));

		_RETURN_RESULT_SESN(sesn_ptr_transient, instance_sesn_ptr_transient, RESULT_TYPE_ERR, RESCODE_USER_LOGINREJECTED)
	}
  //endregion

	bool lock_already_owned = (_RESULT_CODE_EQUAL(THREAD_CONTEXT_UFSRV_RESULT(THREAD_CONTEXT), RESCODE_PROG_LOCKED_BY_THIS_THREAD));
  if (SESNSTATUS_IS_SET(sesn_ptr_hashed->ufsrv_session.stat, SESNSTATUS_TRANSIENT | SESNSTATUS_RECYCLED) ||
      strcmp(SESSION_COOKIE(sesn_ptr_hashed), SESSION_COOKIE(sesn_ptr_transient)) != 0 ||
      !sClaimAuthUser(SESSION_USERID(sesn_ptr_hashed))) {
    if (!lock_already_owned) SessionUnLockCtx(THREAD_CONTEXT_PTR, AS_UFSRV_SESSION(sesn_ptr_hashed), __func__);
    _RETURN_RESULT_SESN(sesn_ptr_transient, instance_sesn_ptr_transient, RESULT_TYPE_ERR, RESCODE_USER_LOGINREJECTED)
  }


	//>>>>>>>>> sesion_ptr_hashed IS NOW LOCKED <<<<<<<<<<<<<<<<

	//the locally hashed session is still marked as connected. The client may have just disconncted/reconnected and we weren't
	//notified in time by the OS, otherwise it may be double signon: same cookie used for simultaneous connections
	if (sesn_ptr_hashed->ufsrv_session.stat&SESNSTATUS_CONNECTED) {

		//haproxy  may still hang on to the connection, even though the client is coming with a fresh connection. Perhaps safer to terminate the connected
		if (sShouldDeclineSignon(sesn_ptr_hashed)) {
#if __UF_TESTING
			syslog(LOG_NOTICE, "%s (pid:'%lu', o:'%p', cid:'%lu'): WARNING: DOUBLE SIGNON ATTEMPT: HASHED SESSION IS CONNECTED, SOCKET LIVE AND SERVICE METRICS FRESH (o_hashed:'%p', cid_hashed:'%lu', socketfd:'%d'): REJECTING INCOMING CONNECTION...",
				__func__, pthread_self(), sesn_ptr_transient, SESSION_ID(sesn_ptr_transient), sesn_ptr_hashed, SESSION_ID(sesn_ptr_hashed), SESSION_SOCKETFD(sesn_ptr_hashed));
#endif

      if (!lock_already_owned) SessionUnLockCtx(THREAD_CONTEXT_PTR, AS_UFSRV_SESSION(sesn_ptr_hashed), __func__);
      _RETURN_RESULT_SESN(sesn_ptr_transient, instance_sesn_ptr_transient, RESULT_TYPE_ERR, RESCODE_USER_LOGINREJECTED)

		} else {
#if __UF_TESTING
			syslog(LOG_NOTICE, "%s (pid:'%lu', o:'%p', cid:'%lu'): WARNING: DOUBLE SIGNON ATTEMPT: HASHED SESSION HAS DEAD SOCKET OR STALE SERVICE METRICS (o:'%p', cid:'%lu', socketfd:'%d'): CLEARING HASHED SESSION...",
				__func__, pthread_self(), sesn_ptr_transient, SESSION_ID(sesn_ptr_transient), sesn_ptr_hashed, SESSION_ID(sesn_ptr_hashed), SESSION_SOCKETFD(sesn_ptr_hashed));
#endif
			//reset it to a sound baseline, before we attempt to reinstate it below
			SuspendSession(instance_sesn_ptr_hashed, SOFT_SUSPENSE);
		}
	}

  CacheBackendGetRawSessionRecordByUserId(SESSION_USERID(sesn_ptr_hashed), CALLFLAGS_EMPTY, THREAD_CONTEXT_UFSRV_RESULT(THREAD_CONTEXT)); //fetch a fresh cache backend record (no db refresh)

	redisReply *redis_ptr = NULL;

	if (THREAD_CONTEXT_UFSRV_RESULT_TYPE_SUCCESS) {
		if (THREAD_CONTEXT_UFSRV_RESULT_CODE_EQUAL(THREAD_CONTEXT, RESCODE_BACKEND_DATA)) {
		  redis_ptr = (redisReply *)THREAD_CONTEXT_UFSRV_RESULT_USERDATA;
			if (SESNSTATUS_IS_SET(sesn_ptr_hashed->ufsrv_session.stat, SESNSTATUS_REMOTE))
				return (_InstateRemoteSessionAsConnected(instance_sesn_ptr_transient, instance_sesn_ptr_hashed, redis_ptr));
		} else if (THREAD_CONTEXT_UFSRV_RESULT_CODE_EQUAL(THREAD_CONTEXT, RESCODE_BACKEND_DATA_EMPTYSET)) {
			syslog(LOG_DEBUG, "%s (pid:'%lu' o:'%p' cid:'%lu'): POSSIBLE CACHING ISSUES: USER LOCAL COOKIE-HASHED, BUT DOESN'T HAVE CACHE BACKEND STATE INFO (BRAND NEW USER?): ALLOWING...", __func__, pthread_self(), sesn_ptr_transient, SESSION_ID(sesn_ptr_transient));

			//CONTINUE THROUGH TO process_locally_hashed_user
		}
	} else {
		//abandon mission. There is no raw redisReply to free
		syslog(LOG_DEBUG, "%s (pid:'%lu' o:'%p', o_hashed:'%p', cid:'%lu'): ERROR: COULD NOT DETERMINE USER NETWORK STATUS: SUSPENDING SESSION...", __func__, pthread_self(), sesn_ptr_transient, sesn_ptr_hashed, SESSION_ID(sesn_ptr_transient));

		SuspendSession(instance_sesn_ptr_transient, SOFT_SUSPENSE);

		if (!lock_already_owned)	SessionUnLockCtx(THREAD_CONTEXT_PTR, AS_UFSRV_SESSION(sesn_ptr_hashed), __func__);

		_RETURN_RESULT_SESN(sesn_ptr_transient, instance_sesn_ptr_transient, RESULT_TYPE_ERR, RESCODE_USER_LOGINREJECTED)
	}

	if (IS_PRESENT(redis_ptr)) freeReplyObject(redis_ptr);

	_instate_suspended_session:
	return (_InstateSuspendedSessionAsConnected(instance_sesn_ptr_transient, instance_sesn_ptr_hashed, sock_msg_ptr));

}

static UFSRVResult *
sHandleClaimedHandshake(InstanceHolderForSession *instance_sesn_ptr_transient, SocketMessage *sock_msg_ptr);

UFSRVResult *
HandlePostSuccessfulIncomingHandshake(InstanceHolderForSession *instance_sesn_ptr_transient, SocketMessage *sock_msg_ptr)
{
  Session *session = SessionOffInstanceHolder(instance_sesn_ptr_transient);
  char cookie[sizeof session->ufsrv_session.session_cookie];
  memcpy(cookie, SESSION_COOKIE(session), sizeof cookie);
  if (!memchr(cookie, '\0', sizeof cookie)) {
    SuspendSession(instance_sesn_ptr_transient, SOFT_SUSPENSE);
    _RETURN_RESULT_SESN(session, instance_sesn_ptr_transient, RESULT_TYPE_ERR, RESCODE_USER_LOGINREJECTED)
  }
  SessionAuthClaim claim;
  if (sAuthClaim || !SessionAuthClaimBegin(&sAuthClaims, &claim, cookie)) {
    SuspendSession(instance_sesn_ptr_transient, SOFT_SUSPENSE);
    _RETURN_RESULT_SESN(session, instance_sesn_ptr_transient, RESULT_TYPE_ERR, RESCODE_USER_LOGINREJECTED)
  }
  sAuthClaim = &claim;
  SessionIncrementReference(instance_sesn_ptr_transient, 1);
  UFSRVResult *result = sHandleClaimedHandshake(instance_sesn_ptr_transient, sock_msg_ptr);
  if (result && _RESULT_TYPE_ERROR(result) && result->result_code == RESCODE_USER_LOGINREJECTED &&
      result->result_user_data == instance_sesn_ptr_transient) {
    UFSRVResult rejected = *result;
    SuspendSession(instance_sesn_ptr_transient, SOFT_SUSPENSE);
    *result = rejected;
  }
  if (sAuthLockedTarget) {
    Session *target = SessionOffInstanceHolder(sAuthLockedTarget);
    if ((!result || result->result_user_data != sAuthLockedTarget) &&
        IsObjectInLockedObjectsStore((THREAD_CONTEXT_PTR)->ht_ptr, (void *)target)) {
      SessionUnLockCtx(THREAD_CONTEXT_PTR, AS_UFSRV_SESSION(target), __func__);
    }
    SessionDecrementReference(sAuthLockedTarget, 1);
    sAuthLockedTarget = NULL;
  }
  SessionDecrementReference(instance_sesn_ptr_transient, 1);
  sAuthClaim = NULL;
  SessionAuthClaimEnd(&sAuthClaims, &claim);
  return result;
}

/**
 * @brief Run authentication and post-handshake callbacks while the outer claim remains active.
 * @param[in] instance_sesn_ptr_transient Referenced incoming holder locked by its I/O worker.
 * @param[in,out] sock_msg_ptr Parsed handshake message.
 * @return Authentication result whose holder can replace the incoming session.
 */
static UFSRVResult *
sHandleClaimedHandshake(InstanceHolderForSession *instance_sesn_ptr_transient, SocketMessage *sock_msg_ptr)
{
  Session *sesn_ptr_transient = SessionOffInstanceHolder(instance_sesn_ptr_transient);

  SESNSTATUS_SET(sesn_ptr_transient->ufsrv_session.stat, SESNSTATUS_TRANSIENT);

  void *cookie_holder = NULL;
  if (!SessionAuthLookupHolder(&(SessionsDelegatorPtr()->hashed_cookies.hashtable), SESSION_COOKIE(sesn_ptr_transient), sPinAuthHolder, &cookie_holder)) {
    _RETURN_RESULT_SESN(sesn_ptr_transient, instance_sesn_ptr_transient, RESULT_TYPE_ERR, RESCODE_USER_LOGINREJECTED)
  }
  InstanceHolderForSession *instance_sesn_ptr_hashed __attribute__((cleanup(sReleaseAuthHolder))) = cookie_holder;

	//this may indicate a concurrent sign on
	if (IS_PRESENT(instance_sesn_ptr_hashed)) {
    UFSRVResult *res_ptr_cookie = AuthenticateForCookieHashedSession(instance_sesn_ptr_transient, instance_sesn_ptr_hashed, sock_msg_ptr);

		if (_RESULT_TYPE_SUCCESS(res_ptr_cookie)) {
			InstanceHolderForSession *instance_sesn_ptr_processed = (InstanceHolderForSession *)_RESULT_USERDATA(res_ptr_cookie);
			return (_InvokeLifecycleCallbackPostHandshake(instance_sesn_ptr_processed, sock_msg_ptr));
		} else if ((_RESULT_TYPE_ERROR(res_ptr_cookie)) && (_RESULT_CODE_EQUAL(res_ptr_cookie, RESCODE_USER_SESN_KILLED))) {
			//falls through to new session block below: recreate session, as hash was invalid
		} else {
		  return	res_ptr_cookie;//user not allowed through
		}
	} else {
#ifdef __UF_TESTING
	  syslog(LOG_DEBUG, "%s (pid:'%lu' o:'%p' cid:'%lu'): COULD NOT FIND SESSION IN LOCAL COOKIE HASH: TRYING BACKEND CACHE...", __func__, pthread_self(), sesn_ptr_transient, SESSION_ID(sesn_ptr_transient));
#endif

		//in combination with TRANSIENT status this will help trap concurrent sign on attempts with the same cookie in the conditional above. cleared downstream
		if (!(AddToHash(&(SessionsDelegatorPtr()->hashed_cookies.hashtable), (void *)instance_sesn_ptr_transient))) {
			_RETURN_RESULT_SESN(sesn_ptr_transient, instance_sesn_ptr_transient, RESULT_TYPE_ERR, RESCODE_PROG_NULL_POINTER)
		}

    CacheBackendGetRawSessionRecordByCookie(SESSION_COOKIE(sesn_ptr_transient), CALLFLAGS_EMPTY);

		if (THREAD_CONTEXT_UFSRV_RESULT_IS_SUCCESS_WITH_BACKEND_DATA) { /*important to check for RESCODE_BACKEND_DATA*/
			UFSRVResult *res_ptr_backend = NULL;
			//user has a valid cookie even though not known locally to this instance: this server could have rebooted, or user coming from another server
			res_ptr_backend = _AuthenticateForBackendCookieHashedSession(instance_sesn_ptr_transient, THREAD_CONTEXT_UFSRV_RESULT(THREAD_CONTEXT), sock_msg_ptr);

			if (_RESULT_TYPE_SUCCESS(res_ptr_backend)) {
				InstanceHolderForSession *instance_sesn_ptr_processed = (InstanceHolderForSession *)_RESULT_USERDATA(res_ptr_backend);
				return (_InvokeLifecycleCallbackPostHandshake(instance_sesn_ptr_processed, sock_msg_ptr));
			} else {
				return res_ptr_backend; //user not allowed through
			}
		}

		//falls through below to new session, as we couldn't find the user on the backend cache with this cookie
	}

#ifdef __UF_TESTING
	syslog(LOG_DEBUG, "%s (pid:'%lu' o:'%p' cid:'%lu'): COULD NOT FIND SESSION IN LOCAL OR BACKEND CACHE: proceeding with New Session Initialisation: Session may still be discoverable in DB BACKEND with this cookie", __func__, pthread_self(), sesn_ptr_transient, SESSION_ID(sesn_ptr_transient));
#endif

  UFSRVResult *res_ptr_new_session = NULL;

  //user still has cookie that is not recorded on the backend cache and not known locally: could be new registration,
  //or user directed to re-authenticate with new cookie. At any rate, the db backend will be the ultimate source
  res_ptr_new_session = _AuthenticateForNonCookieHashedSession(instance_sesn_ptr_transient);

  if (_RESULT_TYPE_SUCCESS(res_ptr_new_session)) {
    InstanceHolderForSession *instance_sesn_ptr_processed = _RESULT_USERDATA(res_ptr_new_session);
    return _InvokeLifecycleCallbackPostHandshake(instance_sesn_ptr_processed, sock_msg_ptr);
  }

  return res_ptr_new_session; //contains error of sorts
}

static  UFSRVResult *
_InvokeLifecycleCallbackPostHandshake(InstanceHolderForSession *instance_sesn_ptr, SocketMessage *sock_msg_ptr)
{
  if (!instance_sesn_ptr) return _ufsrv_result_generic_error;
	Session *sesn_ptr = SessionOffInstanceHolder(instance_sesn_ptr);

	UFSRVResult *res_ptr = ProtocolInvokePostHandshakeCallback(PROTO_PROTOCOL_ID(((Protocol *)SESSION_PROTOCOLTYPE(sesn_ptr))), instance_sesn_ptr, sock_msg_ptr, CALLFLAGS_EMPTY);
	if (IS_PRESENT(res_ptr)) { //todo is this check necessary? trace return upstream
		switch (_RESULT_TYPE(res_ptr)) {
		case RESULT_TYPE_ERR:

			statsd_gauge_inc(THREAD_CONTEXT_INSTRUMENTATION_BACKEND, "worker.work.handshake_failed", 1, UfsrvConfigInstrumentationBackendServerAsPtr());

			SuspendSession(instance_sesn_ptr, SOFT_SUSPENSE);

			_RETURN_RESULT_SESN(sesn_ptr, instance_sesn_ptr, RESULT_TYPE_ERR, RESCODE_PROTOCOL_WSHANDSHAKE)

		default:
			//could have fallen through to _exit_success: below
			_RETURN_RESULT_SESN(sesn_ptr, instance_sesn_ptr, RESULT_TYPE_SUCCESS, RECODE_USER_AUTHENTICATION)
	 }
	}

	exit_success:
	_RETURN_RESULT_SESN(sesn_ptr, instance_sesn_ptr, RESULT_TYPE_SUCCESS, RECODE_USER_AUTHENTICATION)
}

UFSRVResult *
InstateUnconnectedSession(InstanceHolderForSession *instance_sesn_ptr_unconnected, InstanceHolderForSession *instance_sesn_ptr_transient, unsigned long sesn_call_flags)
{
	UFSRVResult res = {0};
  Session     *sesn_ptr_unconnected = SessionOffInstanceHolder(instance_sesn_ptr_unconnected);
  Session     *sesn_ptr_transient   = SessionOffInstanceHolder(instance_sesn_ptr_transient);

	_InstateUnconnectedSessionAsConnected(instance_sesn_ptr_unconnected, instance_sesn_ptr_transient, sesn_call_flags, &res);

	if (res.result_type == RESULT_TYPE_SUCCESS) {
		 //we lost sesn_ptr_transient: we now have a different Session object
		 InstanceHolderForSession *instance_sesn_ptr_instated = (InstanceHolderForSession *)res.result_user_data;

		#if UFSRVWEBSOCK_DEBUG_BUILD
		syslog(LOG_WARNING, "LOCKTRACE wrapper exit='wrapper_success' code='RESCODE_USER_INSTATED' o_transient:'%p' cid_transient:'%lu' o_unconnected:'%p' cid_unconnected:'%lu' tid:'%lu'", sesn_ptr_transient, SESSION_ID(sesn_ptr_transient), sesn_ptr_unconnected, SESSION_ID(sesn_ptr_unconnected), pthread_self());
		#endif
		_RETURN_RESULT_SESN(SessionOffInstanceHolder(instance_sesn_ptr_instated), instance_sesn_ptr_instated, RESULT_TYPE_SUCCESS, RESCODE_USER_INSTATED)
	} else if (res.result_type == RESULT_TYPE_ERR) {
    if (res.result_code == RESCODE_PROG_LOCKED || res.result_code == RESCODE_PROG_HASHED) {
      _RETURN_RESULT_SESN(sesn_ptr_transient, instance_sesn_ptr_transient, RESULT_TYPE_ERR, RESCODE_USER_LOGINREJECTED)
    }

		Session		*sesn_ptr_returned = NULL;
    InstanceHolderForSession *instance_sesn_ptr_returned = NULL;

		if (res.result_code == RESCODE_IO_POLLERROR) {
			//this should be sesn_ptr_transient returned back to us
			instance_sesn_ptr_returned  = (InstanceHolderForSession *)res.result_user_data;
			sesn_ptr_returned           = SessionOffInstanceHolder(instance_sesn_ptr_returned);

			syslog(LOG_NOTICE, "%s (pid:'%lu', o:'%p', cid:'%lu, , o_unconnected:'%p', cid_unconnected:'%lu'): ERROR: COULD NOT INSTATE NEW BACKEND SESSION...", __func__,  pthread_self(), sesn_ptr_returned, SESSION_ID(sesn_ptr_returned), sesn_ptr_unconnected, SESSION_ID(sesn_ptr_unconnected));

			//this will butcher the session back into recycler
			ClearLocalSessionCache(instance_sesn_ptr_unconnected, CALL_FLAG_DONT_BROADCAST_FENCE_EVENT|CALL_FLAG_TRANSFER_SESSION_ACCESS_CONTEXT);

			#if UFSRVWEBSOCK_DEBUG_BUILD
			syslog(LOG_WARNING, "LOCKTRACE wrapper exit='wrapper_poll_error' code='RESCODE_IO_SESSIONSTATE' o_transient:'%p' cid_transient:'%lu' o_unconnected:'%p' cid_unconnected:'%lu' tid:'%lu'", sesn_ptr_transient, SESSION_ID(sesn_ptr_transient), sesn_ptr_unconnected, SESSION_ID(sesn_ptr_unconnected), pthread_self());
			#endif
			_RETURN_RESULT_SESN(sesn_ptr_returned, instance_sesn_ptr_returned, RESULT_TYPE_ERR, RESCODE_IO_SESSIONSTATE)
		} else {
			//NOTICE INVALID REFERENCE TO sesn_ptr: we just doit for debugging to report on object address
			syslog(LOG_ERR, "%s (pid:'%lu', o:'%p', cid:'%lu): SEVERE ERROR: CONFUSED STATE: COULD NOT INSTATE NEW BACKEND SESSION...", __func__, pthread_self(), sesn_ptr_transient, SESSION_ID(sesn_ptr_transient));

			ClearLocalSessionCache(instance_sesn_ptr_unconnected, CALL_FLAG_DONT_BROADCAST_FENCE_EVENT|CALL_FLAG_TRANSFER_SESSION_ACCESS_CONTEXT);
		}

		//statsd_inc(THREAD_CONTEXT_INSTRUMENTATION_BACKEND, "worker.work.handshake_failed", 1.0);
	}

	#if UFSRVWEBSOCK_DEBUG_BUILD
	syslog(LOG_WARNING, "LOCKTRACE wrapper exit='wrapper_fallthrough_error' code='RESCODE_USER_INSTATED' o_transient:'%p' cid_transient:'%lu' o_unconnected:'%p' cid_unconnected:'%lu' tid:'%lu'", sesn_ptr_transient, SESSION_ID(sesn_ptr_transient), sesn_ptr_unconnected, SESSION_ID(sesn_ptr_unconnected), pthread_self());
	#endif
	_RETURN_RESULT_SESN(sesn_ptr_transient, instance_sesn_ptr_transient, RESULT_TYPE_ERR, RESCODE_USER_INSTATED)
}

/**
 * 	@brief	Given a user session found through backend cookie hash (ie do not have a local cookie hash),
 * 	process the semantics around allowing the user through.
 * 	The user could still be known to us (through uid hash eg), but pure cookie hash lookup did not uncover them.
 *	This function processes two states: user has known presence (could be locally connected or remote), or none.
 *
 * 	@note Success: user authenticated
 *
 * 	@param[in] instance_sesn_ptr_transient	current connected session in transient state
 * 	@param[in] res_ptr	Raw redis record of the user UID:%uid
 * @param[in,out] sock_msg_ptr Parsed handshake message forwarded to the selected session.
 * 	@note Ownership: raw redi_ptr is passed through the data object of res_ptr, whihc must be freed here
 *
 * 	@note Required lock: sesn_ptr in main loop
 */
static UFSRVResult *
_AuthenticateForBackendCookieHashedSession(InstanceHolderForSession *instance_sesn_ptr_transient, UFSRVResult *res_ptr, SocketMessage *sock_msg_ptr)
{
	Session *sesn_ptr_backend = NULL;

  Session *sesn_ptr_transient = SessionOffInstanceHolder(instance_sesn_ptr_transient);
	redisReply *redis_ptr = (redisReply *)res_ptr->result_user_data;

	int						status_network	= atoi(redis_ptr->element[REDIS_KEY_USER_STATUS]->str);
	int 					server_id				= atoi(redis_ptr->element[REDIS_KEY_USER_SID]->str);
	unsigned long session_id			= strtoul(redis_ptr->element[REDIS_KEY_USER_CID]->str, NULL, 10);
	UfsrvUid			*uid_ptr				= (UfsrvUid *)redis_ptr->element[REDIS_KEY_USER_UID]->str;
	unsigned long	user_id					= UfsrvUidGetSequenceId(uid_ptr);

  if (!sClaimAuthUser(user_id)) {
    freeReplyObject(redis_ptr);
    _RETURN_RESULT_SESN(sesn_ptr_transient, instance_sesn_ptr_transient, RESULT_TYPE_ERR, RESCODE_USER_LOGINREJECTED)
  }
  void *found_holder = NULL;
  if (!SessionAuthLookupHolder(&(SessionsDelegatorPtr()->hashed_sessions.hashtable), &session_id, sPinAuthHolder, &found_holder)) {
    freeReplyObject(redis_ptr);
    _RETURN_RESULT_SESN(sesn_ptr_transient, instance_sesn_ptr_transient, RESULT_TYPE_ERR, RESCODE_USER_LOGINREJECTED)
  }
  InstanceHolderForSession *instance_sesn_ptr_sesn_hashed __attribute__((cleanup(sReleaseAuthHolder))) = found_holder;
  if (!SessionAuthLookupHolder(&(SessionsDelegatorPtr()->hashed_userids.hashtable), &user_id, sPinAuthHolder, &found_holder)) {
    freeReplyObject(redis_ptr);
    _RETURN_RESULT_SESN(sesn_ptr_transient, instance_sesn_ptr_transient, RESULT_TYPE_ERR, RESCODE_USER_LOGINREJECTED)
  }
  InstanceHolderForSession *instance_sesn_ptr_ui_hashed __attribute__((cleanup(sReleaseAuthHolder))) = found_holder;
  bool conflict;
  InstanceHolderForSession *instance_sesn_ptr_backend =
      SessionAuthSelectHolder(instance_sesn_ptr_sesn_hashed, instance_sesn_ptr_ui_hashed, &conflict);
  if (conflict || (instance_sesn_ptr_backend && !sLockAuthTarget(instance_sesn_ptr_backend, user_id, instance_sesn_ptr_sesn_hashed ? session_id : 0))) {
    freeReplyObject(redis_ptr);
    _RETURN_RESULT_SESN(sesn_ptr_transient, instance_sesn_ptr_transient, RESULT_TYPE_ERR, RESCODE_USER_LOGINREJECTED)
  }
  if (instance_sesn_ptr_backend) sesn_ptr_backend = SessionOffInstanceHolder(instance_sesn_ptr_backend);


	if (IS_EMPTY(sesn_ptr_backend)) {
		//rebuild the session since we have no in-memory data on it
    SESSION_USERID_TEMP(sesn_ptr_transient) = user_id;
		//TODO: SHOULDN'T this be handled same as new user initialisation?
		UFSRVResult *res_ptr_nonlocal = _InstateAuthenticatedNonLocalUser(instance_sesn_ptr_transient, sock_msg_ptr, redis_ptr);
		int rescode = _RESULT_CODE(res_ptr_nonlocal);

		if (_RESULT_TYPE_ERROR(res_ptr_nonlocal)) {
			InstanceHolderForSession *instance_sesn_ptr_processed = (InstanceHolderForSession *)res_ptr_nonlocal->result_user_data;
			Session *sesn_ptr_processed = instance_sesn_ptr_processed ? SessionOffInstanceHolder(instance_sesn_ptr_processed) : NULL;

			if (!IS_EMPTY(instance_sesn_ptr_processed))	{
			  SuspendSession(instance_sesn_ptr_processed, SOFT_SUSPENSE);
			  if (sesn_ptr_processed == sesn_ptr_transient) {
				 RemoveFromHash(&(SessionsDelegatorPtr()->hashed_cookies.hashtable), (void *)instance_sesn_ptr_transient);
			  }
			}

      if (!sesn_ptr_processed) return _ufsrv_result_generic_error;
      _RETURN_RESULT_SESN(sesn_ptr_processed, instance_sesn_ptr_processed, RESULT_TYPE_ERR, rescode)
		}

    return  res_ptr_nonlocal;
	} else {
		//refresh user session from newly fetched backend data
		UFSRVResult *res_ptr_local = _RefreshAuthenticatedLocalUser(instance_sesn_ptr_transient, instance_sesn_ptr_backend, sock_msg_ptr, redis_ptr);
    int rescode = _RESULT_CODE(res_ptr_local);

		if (_RESULT_TYPE_ERROR(res_ptr_local)) {
			InstanceHolderForSession *instance_sesn_ptr_processed = (InstanceHolderForSession *)res_ptr_local->result_user_data;
			Session *sesn_ptr_processed = instance_sesn_ptr_processed ? SessionOffInstanceHolder(instance_sesn_ptr_processed) : NULL;
			if (!IS_EMPTY(instance_sesn_ptr_processed))	{
			  SuspendSession(instance_sesn_ptr_processed, SOFT_SUSPENSE);
        if (sesn_ptr_processed == sesn_ptr_transient) {
          RemoveFromHash(&(SessionsDelegatorPtr()->hashed_cookies.hashtable), (void *)instance_sesn_ptr_transient);
        }
			}
      if (!sesn_ptr_processed) return _ufsrv_result_generic_error;
      _RETURN_RESULT_SESN(sesn_ptr_processed, instance_sesn_ptr_processed, RESULT_TYPE_ERR, rescode)
		}

		return res_ptr_local;
	}

}

/**
 * Authenticate a session that has no locally hashed cookie.
 * The DB backend is the ultimate source of truth for cookie validation.
 */
static UFSRVResult *
_AuthenticateForNonCookieHashedSession(InstanceHolderForSession *instance_sesn_ptr_transient)
{
	__unused unsigned long	session_id_invoked;
	Session 		*sesn_ptr_processed = NULL,
	            *sesn_ptr_transient = SessionOffInstanceHolder(instance_sesn_ptr_transient);
	InstanceHolderForSession *instance_sesn_ptr_processed = NULL;

	//remember current session in case we get a swap
	session_id_invoked = SESSION_ID(sesn_ptr_transient);

	UFSRVResult *res_ptr = _DbBackendVerifyAndInitialiseSession(instance_sesn_ptr_transient);

	if (_RESULT_TYPE_SUCCESS(res_ptr)) {
		switch (res_ptr->result_code) {
			case RESCODE_USER_INSTATED:
				//local unconnected session was instated as connected. we have a different session object
				instance_sesn_ptr_processed = (InstanceHolderForSession *)res_ptr->result_user_data; break;

			case RESCODE_USER_MIGRATED:
				//new session was migrated from the backend
        instance_sesn_ptr_processed = (InstanceHolderForSession *)res_ptr->result_user_data; break;

			case RECODE_USER_AUTHENTICATION:
				//brand new user session
        instance_sesn_ptr_processed = (InstanceHolderForSession *)res_ptr->result_user_data; break;
		}

    if (!instance_sesn_ptr_processed) {
      _RETURN_RESULT_SESN(sesn_ptr_transient, instance_sesn_ptr_transient, RESULT_TYPE_ERR, RESCODE_USER_LOGINREJECTED)
    }
		//suspend on error
		sesn_ptr_processed = SessionOffInstanceHolder(instance_sesn_ptr_processed);
		UFSRVResult *res_ptr_error = HandleSessionReturnHandshake(instance_sesn_ptr_processed, NULL, CALL_FLAG_SUSSPEND_SESSION);
		if (SESSION_RESULT_TYPE_SUCCESS(sesn_ptr_processed)) {
			_RETURN_RESULT_SESN(sesn_ptr_processed, instance_sesn_ptr_processed, RESULT_TYPE_SUCCESS, RESCODE_USER_INITIALISED);
		}
		else return res_ptr_error;//session suspended
	}
	else if (_RESULT_TYPE_ERROR(res_ptr)) {
		UFSRVResult temp_res,
                *temp_res_ptr;

		temp_res_ptr = &temp_res;
		*temp_res_ptr = *res_ptr;

		SuspendSession(instance_sesn_ptr_transient, SOFT_SUSPENSE);//this may invoke routines that modify desired res_ptr result returned above

		*res_ptr = *temp_res_ptr;

		if (_RESULT_CODE_EQUAL(res_ptr, RECODE_USER_AUTHENTICATION)) {
#ifdef __UF_TESTING
			syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p'}: Setting SESNSTATUS_DEFERRED_RECYCLE flag...", __func__, pthread_self(), sesn_ptr_transient);
#endif
			SESNSTATUS_SET(sesn_ptr_transient->ufsrv_session.stat, SESNSTATUS_DEFERRED_RECYCLE);
		}
	}

	//contains error of sorts
	return res_ptr;

}

// ═══════════════════════════════════════════════════════════════
// _DbBackendVerifyAndInitialiseSession (from session.c:1971-2001)
// ═══════════════════════════════════════════════════════════════
static inline UFSRVResult  *
_DbBackendVerifyAndInitialiseSession(InstanceHolderForSession *instance_sesn_ptr_transient)
{
	AuthenticatedAccount 	authenticated_account = {0};
  Session *sesn_ptr_transient = SessionOffInstanceHolder(instance_sesn_ptr_transient);

	//let's make sure we are supposed to be talking to this fellow. We should hold a cookie of sorts if legit user
	UFSRVResult *res_ptr = DbValidateUserSignOnWithCookie(sesn_ptr_transient, SESSION_COOKIE(sesn_ptr_transient), &authenticated_account, NULL);
	if (!_RESULT_TYPE_SUCCESS(res_ptr)) {
		syslog(LOG_DEBUG, LOGSTR_ACCOUNT_INVALID_SIGNON_COOKIE, __func__, pthread_self(), sesn_ptr_transient, SESSION_ID(sesn_ptr_transient), SESSION_COOKIE(sesn_ptr_transient), LOGCODE_ACCOUNT_INVALID_SIGNON_COOKIE);

		ProcessOutgoingWsHandshake(instance_sesn_ptr_transient, NULL);
    UfsrvCommandInvokeUserCommand(&(InstanceContextForSession) {instance_sesn_ptr_transient, AS_UFSRV_SESSION(sesn_ptr_transient)}, NULL, NULL, NULL, NULL, uACCOUNT_VERIFIED_V1_IDX);

    _RETURN_RESULT_SESN(sesn_ptr_transient, instance_sesn_ptr_transient, RESULT_TYPE_ERR, RECODE_USER_AUTHENTICATION)
	}

	//TODO: check for RESCODE_ACCOUNT_DISABLED, as account maybe disabled even if AUTHCOOKIE is valid

	//User's cookie is a valid one.
	//Bootstrap basic account info into Session, bearing in mind the user may still exist somewhere based on uid just retrieved
	//so keep Session population light-on because if we later on uncovered a local Session, we'll refresh it and reuse it
	//and this transient Session will be ditched

	//IMPORTANT: data transfered maybe lost because this session is still in transient mode, it may or may not have a local hashed seession
	//if it has a local session we need to on-transfer this data to that session hence CALL_FLAG_TRANSFER_DB_USERDATA below
  if (!sClaimAuthUser(authenticated_account.userid)) {
    AuthenticatedAccountMemDestruct(&authenticated_account, false);
    _RETURN_RESULT_SESN(sesn_ptr_transient, instance_sesn_ptr_transient, RESULT_TYPE_ERR, RESCODE_USER_LOGINREJECTED)
  }
	TransferBasicSessionDbBackendData(sesn_ptr_transient, &authenticated_account); //cookie is locally hashed for transient. must be removed on success

	return (_DiscoverSessionWhereabouts(instance_sesn_ptr_transient, CALL_FLAG_TRANSFER_DB_USERDATA));

}

// ═══════════════════════════════════════════════════════════════
// _DiscoverSessionWhereabouts (from session.c:2028-2054)
// ═══════════════════════════════════════════════════════════════
 inline static UFSRVResult *
_DiscoverSessionWhereabouts(InstanceHolderForSession *instance_sesn_ptr_transient, unsigned long sesn_call_flags)
{
   Session *sesn_ptr_transient = SessionOffInstanceHolder(instance_sesn_ptr_transient);

	//this maybe a throw away data in case of existing session, as we'll reload from backend if record exists
	//there, but we don't know at this stage: we have to do full initialisation

  unsigned long uid = SESSION_USERID(sesn_ptr_transient);
  void *found_holder = NULL;
  if (!SessionAuthLookupHolder(&(SessionsDelegatorPtr()->hashed_userids.hashtable), &uid, sPinAuthHolder, &found_holder)) {
    _RETURN_RESULT_SESN(sesn_ptr_transient, instance_sesn_ptr_transient, RESULT_TYPE_ERR, RESCODE_USER_LOGINREJECTED)
  }
  InstanceHolderForSession *holder __attribute__((cleanup(sReleaseAuthHolder))) = found_holder;
  if (holder) {
    if (!sLockAuthTarget(holder, uid, 0)) {
      _RETURN_RESULT_SESN(sesn_ptr_transient, instance_sesn_ptr_transient, RESULT_TYPE_ERR, RESCODE_USER_LOGINREJECTED)
    }
    return _AuthenticateUserIdHashedSession(instance_sesn_ptr_transient, holder, sesn_call_flags);
  }
  UFSRVResult backend_result = {0};
  CacheBackendGetRawSessionRecordByUserId(uid, CALL_FLAG_SEARCH_BACKEND, &backend_result);
  if (_RESULT_TYPE_SUCCESS(&backend_result) && backend_result.result_code == RESCODE_BACKEND_DATA) {
    redisReply *redis_ptr = backend_result.result_user_data;
    if (atoi(redis_ptr->element[REDIS_KEY_USER_STATUS]->str) != 0) {
      return _AuthenticateDbBackendValidatedCookieSession(instance_sesn_ptr_transient, redis_ptr, sesn_call_flags);
    }
    freeReplyObject(redis_ptr);
  }

	return (_AuthenticateBrandNewSession(instance_sesn_ptr_transient));

}

// ═══════════════════════════════════════════════════════════════
// _AuthenticateUserIdHashedSession (from session.c:2084-2103)
// ═══════════════════════════════════════════════════════════════
 static inline UFSRVResult *
_AuthenticateUserIdHashedSession(InstanceHolderForSession *instance_sesn_ptr_transient, InstanceHolderForSession *instance_sesn_ptr_uid_hashed, unsigned long sesn_call_flags)
 {
  unsigned long	call_flags_unconnected = sesn_call_flags|CALL_FLAG_SWAP_COOKIE|CALL_FLAG_WRITEBACK_SESSION_DATA_TO_BACKEND;//this will cause the cookie to be hashed
  Session *sesn_ptr_uid_hashed = SessionOffInstanceHolder(instance_sesn_ptr_uid_hashed);
   Session *sesn_ptr_transient = SessionOffInstanceHolder(instance_sesn_ptr_transient);

 	//TODO: this condition may need to be revisited as it may not be always true
 	if (SESNSTATUS_IS_SET(sesn_ptr_uid_hashed->ufsrv_session.stat, SESNSTATUS_REMOTE)) {
 		 return (InstateUnconnectedSession(instance_sesn_ptr_uid_hashed, instance_sesn_ptr_transient, call_flags_unconnected));
 	 } else {
 		 //At this stage we do no different, just mark out rge condition. just noise
 		 syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', o_uid_hashed:'%p', cid:'%lu', uid:'%lu', uid_uid_hashed:'%lu'}: NOTICE: FOUND NON COOKIE-HASED SESSION, UID-HASHED BUT NOT REMOTE: Maybe new signon cookie from a previously connected session",__func__, pthread_self(),
            sesn_ptr_transient, sesn_ptr_uid_hashed, SESSION_ID(sesn_ptr_transient), SESSION_USERID(sesn_ptr_transient), SESSION_USERID(sesn_ptr_uid_hashed));
 		return (InstateUnconnectedSession (instance_sesn_ptr_uid_hashed, instance_sesn_ptr_transient, call_flags_unconnected));
 	 }

 	 _RETURN_RESULT_SESN(sesn_ptr_transient, instance_sesn_ptr_transient, RESULT_TYPE_ERR, RESCODE_USER_INSTATED)

 }

// ═══════════════════════════════════════════════════════════════
// _AuthenticateDbBackendValidatedCookieSession (from session.c:2123-2174)
// ═══════════════════════════════════════════════════════════════
static inline UFSRVResult *
_AuthenticateDbBackendValidatedCookieSession(InstanceHolderForSession *instance_sesn_ptr_transient, redisReply *redis_ptr, unsigned long sesn_call_flags)
 {
 	//connected remote session instance found in the backend: instantiate one locally
 	//this could be hoaxed, as the server may have crashed before updating the true value

 	Session *sesn_ptr_migrated	= NULL;
  Session *sesn_ptr_transient = SessionOffInstanceHolder(instance_sesn_ptr_transient);
 	InstanceHolderForSession *instance_sesn_ptr_migrated;

 	//DON'T LOCK MIGRATED SESSION  THAT GETS AUTOMATICALLY DONE IN InstateUnconnectedSession()
 	//DON'T flag CALL_FLAG_ATTACH_FENCE_LIST_TO_SESSION. See IMPORTANT note above
 	if ((instance_sesn_ptr_migrated = CacheBackendInstantiateRawSessionRecord(sesn_ptr_transient, redis_ptr, (CALL_FLAG_TRANSFER_DB_USERDATA | CALL_FLAG_LOAD_DB_BACKEND_FOR_SESSION), NULL))) {
 	  sesn_ptr_migrated = SessionOffInstanceHolder(instance_sesn_ptr_migrated);
 		unsigned long sesn_call_flags = (CALL_FLAG_SWAP_COOKIE|CALL_FLAG_HASH_USERNAME_LOCALLY|
 																		CALL_FLAG_HASH_UID_LOCALLY|CALL_FLAG_ATTACH_FENCE_LIST_TO_SESSION|
																		CALL_FLAG_WRITEBACK_SESSION_DATA_TO_BACKEND);

 		//locks unconnected session
 		UFSRVResult *res_ptr = InstateUnconnectedSession(instance_sesn_ptr_migrated, instance_sesn_ptr_transient, sesn_call_flags);

 		if (res_ptr->result_type == RESULT_TYPE_SUCCESS) {
 #ifdef __UF_TESTING
 		syslog(LOG_DEBUG, LOGSTR_SESSION_SUCCESS_MIGRATED, __func__, pthread_self(), sesn_ptr_transient, sesn_ptr_migrated, SESSION_ID(sesn_ptr_migrated), LOGCODE_SESSION_SUCCESS_MIGRATED);
 #endif

 			freeReplyObject(redis_ptr);

      //we want full fence list for this session + for each session full list of session
#define ABORT_ON_FAILURE true
      int failed_lists = InstateFenceListsForUser(instance_sesn_ptr_migrated, SESSION_CALLFLAGS_EMPTY, (MEMBER_FENCE|INVITED_FENCE), ABORT_ON_FAILURE);
      if (failed_lists != 0) {
        ClearLocalSessionCache(instance_sesn_ptr_migrated, CALL_FLAG_DONT_BROADCAST_FENCE_EVENT);
        _RETURN_RESULT_SESN(sesn_ptr_transient, NULL, RESULT_TYPE_ERR, RESCODE_FENCE_SESSION_INTEGRITY);
      }
#undef ABORT_ON_FAILURE

 			_RETURN_RESULT_SESN(sesn_ptr_migrated, instance_sesn_ptr_migrated, RESULT_TYPE_SUCCESS, RESCODE_USER_MIGRATED)
 		} else {
 			syslog(LOG_DEBUG, LOGSTR_SESSION_ERROR_MIGRATED, __func__, pthread_self(), sesn_ptr_transient, SESSION_ID(sesn_ptr_transient), LOGCODE_SESSION_ERROR_MIGRATED);

 			freeReplyObject(redis_ptr);

 			return res_ptr;
 		}
 	}

 	freeReplyObject(redis_ptr);

 	_RETURN_RESULT_SESN(sesn_ptr_transient, NULL, RESULT_TYPE_ERR, RESCODE_USERINFO_BAKENDERR)

 }

// ═══════════════════════════════════════════════════════════════
// _AuthenticateBrandNewSession (from session.c:2186-2208)
// ═══════════════════════════════════════════════════════════════
static inline UFSRVResult *
_AuthenticateBrandNewSession(InstanceHolderForSession *instance_sesn_ptr_transient)
 {
   Session *sesn_ptr_transient = SessionOffInstanceHolder(instance_sesn_ptr_transient);

	DbBackendBootstrapIntoBrandNewSession(sesn_ptr_transient, 0UL, true);
	if (SESSION_RESULT_TYPE_SUCCESS(sesn_ptr_transient)) {
		if (!_SetupLocalCachingForBrandNewSession(instance_sesn_ptr_transient))	goto exit_error;

		RefreshBackendCacheForSession(sesn_ptr_transient, NULL, CALLFLAGS_EMPTY);
		if (SESSION_RESULT_TYPE_SUCCESS(sesn_ptr_transient)) {
      SESNSTATUS_SET(SESSION_STATUS(sesn_ptr_transient), SESNSTATUS_AUTHENTICATED);
      SESNSTATUS_UNSET(SESSION_STATUS(sesn_ptr_transient), SESNSTATUS_TRANSIENT);

			_RETURN_RESULT_SESN(sesn_ptr_transient, instance_sesn_ptr_transient, RESULT_TYPE_SUCCESS, RECODE_USER_AUTHENTICATION)
		}
	}

 	exit_error:

 	_RETURN_RESULT_SESN(sesn_ptr_transient, instance_sesn_ptr_transient, RESULT_TYPE_ERR, RECODE_USER_AUTHENTICATION)

 }

// ═══════════════════════════════════════════════════════════════
// _SetupLocalCachingForBrandNewSession (from session.c:2215-2244)
// ═══════════════════════════════════════════════════════════════
static bool
_SetupLocalCachingForBrandNewSession(InstanceHolderForSession *instance_sesn_ptr)
 {
  Session *sesn_ptr = SessionOffInstanceHolder(instance_sesn_ptr);

   if (!SESNSTATUS_IS_SET(SESSION_STATUS(sesn_ptr), SESNSTATUS_TRANSIENT)) {
     if (!(AddToHash(&(SessionsDelegatorPtr()->hashed_cookies.hashtable), (void *) instance_sesn_ptr))) {
       return false;
     }
   }

 	if (!(AddToHash(&(SessionsDelegatorPtr()->hashed_usernames.hashtable), (void *)instance_sesn_ptr))) {
 		goto error_remove_cookies_hash;
 	}

 	if (!(AddToHash(&(SessionsDelegatorPtr()->hashed_userids.hashtable), (void *)instance_sesn_ptr))) {
 		goto error_remove_username_hash;
 	}

 	return true;

 	error_remove_username_hash:
 	RemoveFromHash(SESNDELEGATE_USERNAMECACHE(SessionsDelegatorPtr()), (void *) instance_sesn_ptr);

 	error_remove_cookies_hash:
 	RemoveFromHash(SESNDELEGATE_COOKIECACHE(SessionsDelegatorPtr()), (void *) instance_sesn_ptr);

 	return false;

 }

// ═══════════════════════════════════════════════════════════════
// _InstateAuthenticatedNonLocalUser (from session.c:2478-2524)
// ═══════════════════════════════════════════════════════════════
static inline UFSRVResult *
_InstateAuthenticatedNonLocalUser(InstanceHolderForSession *instance_sesn_ptr_transient, SocketMessage *sock_msg_ptr, redisReply *redis_ptr)
{
	Session *sesn_ptr_backend = NULL;
	InstanceHolderForSession *instance_sesn_ptr_backend;

	Session *sesn_ptr_transient = SessionOffInstanceHolder(instance_sesn_ptr_transient);

	//DOES NOT LOCK sesn_ptr_backend
	unsigned long sesn_call_flags = (CALL_FLAG_LOCK_SESSION| CALL_FLAG_HASH_SESSION_LOCALLY| CALL_FLAG_LOAD_DB_BACKEND_FOR_SESSION| CALL_FLAG_ATTACH_FENCE_LIST_TO_SESSION);
	if ((instance_sesn_ptr_backend = CacheBackendInstantiateRawSessionRecord(sesn_ptr_transient, redis_ptr, sesn_call_flags, NULL))) {
    sesn_ptr_backend = SessionOffInstanceHolder(instance_sesn_ptr_backend);
#ifdef __UF_FULLDEBUG
		syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p' cid:'%lu'): SESSION (o_migrated:'%p', cid_migrated:'%lu') WAS MIGRATED FROM BACKEND", __func__,
			pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr), sesn_ptr_backend, SESSION_ID(sesn_ptr_backend));
#endif
	} else {
		syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p', cid:'%lu'): ERROR: COULD NOT INSTANTIATE SESSION FROM BACKEND OBJECT: TERMINATING USER...", __func__, pthread_self(), sesn_ptr_transient, SESSION_ID(sesn_ptr_transient));

		freeReplyObject(redis_ptr);
     //suspend? or leave for upstream
		_RETURN_RESULT_SESN(sesn_ptr_transient, instance_sesn_ptr_transient, RESULT_TYPE_ERR, RESCODE_USERINFO_BAKENDERR)
	}

	//no more use for this because we have now refreshed the session
	freeReplyObject(redis_ptr);

	//CacheBackendInstantiateRawSessionRecord will have instated cookie value, so we need to hash (not CALL_FLAG_SWAP_COOKIE)
	//LOCKS sesn_ptr_backend. sesn_ptr_transient is  RETURNED TO THE POOL cannot be referenced anymore
	sesn_call_flags = CALL_FLAG_DONT_LOCK_SESSION|CALL_FLAG_HASH_UID_LOCALLY|CALL_FLAG_HASH_USERNAME_LOCALLY|CALL_FLAG_WRITEBACK_SESSION_DATA_TO_BACKEND;
	UFSRVResult *res_ptr_error = InstateUnconnectedSession(instance_sesn_ptr_backend, instance_sesn_ptr_transient, sesn_call_flags);//event broadcasted

	if (SESSION_RESULT_TYPE_SUCCESS(sesn_ptr_backend)) {
		//happy end
		UFSRVResult *res_ptr_error = HandleSessionReturnHandshake(instance_sesn_ptr_backend, sock_msg_ptr, CALL_FLAG_SUSSPEND_SESSION);

		if (SESSION_RESULT_TYPE_SUCCESS(sesn_ptr_backend)) { //todo: this is confusing; clean up and user res_ptr_error
			_RETURN_RESULT_SESN(sesn_ptr_backend, instance_sesn_ptr_backend, RESULT_TYPE_SUCCESS, RESCODE_USER_INITIALISED);
		}

		return res_ptr_error;
	}

	//error of some sort
	return res_ptr_error;

}

// ═══════════════════════════════════════════════════════════════
// _RefreshAuthenticatedLocalUser (from session.c:2545-2589)
// ═══════════════════════════════════════════════════════════════
static inline UFSRVResult *
_RefreshAuthenticatedLocalUser(InstanceHolderForSession *instance_sesn_ptr_transient, InstanceHolderForSession *instance_sesn_ptr_hashed, SocketMessage *sock_msg_ptr, redisReply *redis_ptr)
{
  Session *sesn_ptr_transient = SessionOffInstanceHolder(instance_sesn_ptr_transient),
          *sesn_ptr_hashed    = SessionOffInstanceHolder(instance_sesn_ptr_hashed);

	//DOES NOT LOCK sesn_ptr_hashed. All we do is data population from backend record --> sesn_ptr_hashed
	//THIS IS WRONG: RESPECT REFRENCE COUNTING
  if (SESSION_EID(sesn_ptr_transient) != SESSION_EID(sesn_ptr_hashed)) {
    syslog(LOG_NOTICE, "%s (pid:'%lu', o:'%p', cid:'%lu', eid:'%lu', o_hashed:'%p', cid_hashed:'%lu', eid_hashed:'%lu'): EVENT IDs don't match: reloading session from backend record...", __func__, pthread_self(), sesn_ptr_transient, SESSION_ID(sesn_ptr_transient), SESSION_EID(sesn_ptr_transient), sesn_ptr_hashed, SESSION_ID(sesn_ptr_hashed), SESSION_EID(sesn_ptr_hashed));
    if (!(CacheBackendInstantiateRawSessionRecord(sesn_ptr_transient, redis_ptr, CALL_FLAG_ATTACH_FENCE_LIST_TO_SESSION, instance_sesn_ptr_hashed)))
      goto _exit_refresh_failed;
  }

	freeReplyObject(redis_ptr);

	/*
	 * CALL_FLAG_SWAP_COOKIE: because we are not sure if sesn_ptr_hashed was connected before so just update cookie as well
	 *	we are not hashing session because id wouldn't have changed, not its underlying Session object
	 *	LOCKS sesn_ptr_hashed. sesn_ptr is  RETURNED TO THE POOL cannot be referenced anymore
	 */
	unsigned call_flags_sesn = CALL_FLAG_HASH_UID_LOCALLY|CALL_FLAG_HASH_USERNAME_LOCALLY|CALL_FLAG_SWAP_COOKIE|CALL_FLAG_WRITEBACK_SESSION_DATA_TO_BACKEND;
	UFSRVResult *res_ptr_error = InstateUnconnectedSession(instance_sesn_ptr_hashed, instance_sesn_ptr_transient, call_flags_sesn);

	if (SESSION_RESULT_TYPE_SUCCESS(sesn_ptr_hashed)) {
		UFSRVResult *res_ptr_error = HandleSessionReturnHandshake(instance_sesn_ptr_hashed, sock_msg_ptr, CALL_FLAG_SUSSPEND_SESSION);

		if (SESSION_RESULT_TYPE_SUCCESS(sesn_ptr_hashed)) {
			_RETURN_RESULT_SESN(sesn_ptr_hashed, instance_sesn_ptr_hashed, RESULT_TYPE_SUCCESS, RESCODE_USER_INITIALISED)
		}

		return res_ptr_error;
	} else {
		return res_ptr_error;
	}

	_exit_refresh_failed:
	syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p', cid:'%lu', o_hashed:'%p', cid_hashed:'%lu'): ERROR: COULD NOT REFRESH SESSION FROM BACKEND OBJECT: TERMINATING USER...", __func__, pthread_self(), sesn_ptr_transient, SESSION_ID(sesn_ptr_transient), sesn_ptr_hashed, SESSION_ID(sesn_ptr_hashed));

	freeReplyObject(redis_ptr);

	//TODO: terminate user, but we need a protocol message to inform them of the reason. Hashed instance remains where it was
	_RETURN_RESULT_SESN(sesn_ptr_transient, instance_sesn_ptr_transient, RESULT_TYPE_ERR, RESCODE_USERINFO_BAKENDERR)

}

// ═══════════════════════════════════════════════════════════════
// _InstateRemoteSessionAsConnected (from session.c:2733-2788)
// ═══════════════════════════════════════════════════════════════
static inline UFSRVResult *
_InstateRemoteSessionAsConnected(InstanceHolderForSession *instance_sesn_ptr_transient, InstanceHolderForSession *instance_sesn_ptr_hashed, redisReply *redis_ptr)
{
	int status_network  = atoi(redis_ptr->element[REDIS_KEY_USER_STATUS]->str);
	int server_id       = atoi(redis_ptr->element[REDIS_KEY_USER_SID]->str);
  unsigned long eid   = strtoul(redis_ptr->element[REDIS_KEY_USER_EVENT_COUNTER]->str, NULL, 10);

  Session *sesn_ptr_transient = SessionOffInstanceHolder(instance_sesn_ptr_transient),
          *sesn_ptr_hashed    = SessionOffInstanceHolder(instance_sesn_ptr_hashed) ;

	syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p', cid:'%lu'): FOUND COOKIE HASHED SESSION (cid_hashed:'%lu', o_hashed:'%p', sid:'%d' status:'%d', eid_hashed:'%lu', eid_backend:'%lu') WITH REMOTE FLAG: REBUILDING SESSION FROM BACKEND (ONLY IF EIDS ARE DIFFERENT)...",
		__func__, pthread_self(), sesn_ptr_transient, SESSION_ID(sesn_ptr_transient), SESSION_ID(sesn_ptr_hashed), sesn_ptr_hashed, server_id, status_network, SESSION_EID(sesn_ptr_hashed), eid);

	__unused Session *sesn_ptr_new = NULL;
	InstanceHolderForSession *instance_sesn_ptr_new;

	if (eid != SESSION_EID(sesn_ptr_hashed)) {
		//abandon original hashed session, backend copy remains intact. we now lost sesn_ptr_hash + unlocked
		//TODO: RESET LIFECYCLE MISSING inside ClearLocal.. although logical Session is not destroyed this just a reference to it

		ClearLocalSessionCache(instance_sesn_ptr_hashed, CALL_FLAG_UNLOCK_SESSION | CALL_FLAG_DONT_BROADCAST_FENCE_EVENT | CALL_FLAG_TRANSFER_SESSION_ACCESS_CONTEXT);

		//>>>>>>>>> session_ptr_hashed IS NOW UNLOCKED and INSIDE RECYCLER<<<<<<<<<<<<<<<<

		//rebuild object from backend cache and fully setup locally
		//LOCKS sesn_ptr_hashed
		unsigned long sesn_call_flags = CALL_FLAG_LOCK_SESSION | CALL_FLAG_HASH_SESSION_LOCALLY | CALL_FLAG_HASH_UID_LOCALLY | CALL_FLAG_HASH_USERNAME_LOCALLY | CALL_FLAG_ATTACH_FENCE_LIST_TO_SESSION;
		instance_sesn_ptr_new = CacheBackendInstantiateRawSessionRecord(sesn_ptr_transient, redis_ptr, sesn_call_flags, NULL); //no db refresh?
		sesn_ptr_new = SessionOffInstanceHolder(instance_sesn_ptr_new);
	} else {
	  instance_sesn_ptr_new = instance_sesn_ptr_hashed;
	  sesn_ptr_new = sesn_ptr_hashed;
	}
	//>>>>>>>>> sesion_ptr_new IS NOW LOCKED <<<<<<<<<<<<<<<<

	freeReplyObject(redis_ptr);

	UFSRVResult *res_ptr = InstateUnconnectedSession(instance_sesn_ptr_new, instance_sesn_ptr_transient, CALL_FLAG_DONT_LOCK_SESSION|CALL_FLAG_WRITEBACK_SESSION_DATA_TO_BACKEND);
	InstanceHolderForSession *instance_sesn_ptr_processed = (InstanceHolderForSession *)res_ptr->result_user_data;
  Session *sesn_ptr_processed = SessionOffInstanceHolder(instance_sesn_ptr_new);

	if (res_ptr->result_type == RESULT_TYPE_SUCCESS) {
		UFSRVResult *res_ptr_error = HandleSessionReturnHandshake(instance_sesn_ptr_processed, NULL, CALL_FLAG_SUSSPEND_SESSION);

		if (SESSION_RESULT_TYPE_SUCCESS(sesn_ptr_processed)) {
			_RETURN_RESULT_SESN(sesn_ptr_processed, instance_sesn_ptr_processed, RESULT_TYPE_SUCCESS, RESCODE_USER_INITIALISED)
		}
		else return res_ptr_error;//session suspended
	} else if (res_ptr->result_type == RESULT_TYPE_ERR) {
		InstanceHolderForSession *instance_sesn_ptr_processed = (InstanceHolderForSession *)res_ptr->result_user_data;
		if (!IS_EMPTY(instance_sesn_ptr_processed))	SuspendSession(instance_sesn_ptr_processed, SOFT_SUSPENSE);
	}

	return res_ptr;//whatever in it... can contain generic error

}

// ═══════════════════════════════════════════════════════════════
// _InstateSuspendedSessionAsConnected (from session.c:2800-2825)
// ═══════════════════════════════════════════════════════════════
static inline UFSRVResult *
_InstateSuspendedSessionAsConnected(InstanceHolderForSession *instance_sesn_ptr_transient, InstanceHolderForSession *instance_sesn_ptr_hashed, SocketMessage *sock_msg_ptr)
{
  Session     *sesn_ptr_hashed = SessionOffInstanceHolder(instance_sesn_ptr_hashed);
  Session     *sesn_ptr_transient = SessionOffInstanceHolder(instance_sesn_ptr_transient);

	unsigned long	sesn_call_flags = CALL_FLAG_WRITEBACK_SESSION_DATA_TO_BACKEND|CALL_FLAG_DONT_LOCK_SESSION;//since it is already locked by the caller
	UFSRVResult		*res_ptr = InstateUnconnectedSession(instance_sesn_ptr_hashed, instance_sesn_ptr_transient, sesn_call_flags);

	if (SESSION_RESULT_TYPE_SUCCESS(sesn_ptr_hashed)) {
		UFSRVResult *res_ptr_error = HandleSessionReturnHandshake (instance_sesn_ptr_hashed, sock_msg_ptr, CALL_FLAG_SUSSPEND_SESSION);

		if (SESSION_RESULT_TYPE_SUCCESS(sesn_ptr_hashed)) {
			_RETURN_RESULT_SESN(sesn_ptr_hashed, instance_sesn_ptr_hashed, RESULT_TYPE_SUCCESS, RESCODE_USER_INITIALISED)
		}

		return res_ptr_error;
	} else {
		if (!(IS_EMPTY(_RESULT_USERDATA(res_ptr)))) {
			SuspendSession(((InstanceHolderForSession *)_RESULT_USERDATA(res_ptr)), SOFT_SUSPENSE);
		}

		return res_ptr;
	}

}

#define _RECOVERY_BLOCK \
/*backend cookie hash is not affected as it has not been touched at this point*/\
new_sptr = sesn_ptr_transient->ufsrv_session.ssptr;\
sesn_ptr_transient->ufsrv_session.ssptr = sesn_ptr_migrated->ufsrv_session.ssptr; \
sesn_ptr_migrated->ufsrv_session.ssptr = new_sptr;	\
\
SESNSTATUS_UNSET(sesn_ptr_transient->ufsrv_session.stat, SESNSTATUS_SUSPENDED);\
AddSessionToMonitoredWorkEvents (instance_sesn_ptr_transient);\
\
if (!lock_already_owned)	SessionUnLockCtx (THREAD_CONTEXT_PTR, AS_UFSRV_SESSION(sesn_ptr_migrated), __func__);


// ═══════════════════════════════════════════════════════════════
// _InstateUnconnectedSessionAsConnected (from session.c:2931-3158)
// ═══════════════════════════════════════════════════════════════
static void
sTransferHeldCell(InstanceHolderForSession *from_holder, InstanceHolderForSession *to_holder, unsigned long call_flags)
{
	Session *from = SessionOffInstanceHolder(from_holder);
	Session *to = SessionOffInstanceHolder(to_holder);
	ThreadContext *ctx = THREAD_CONTEXT_PTR;
	bool locked = !(call_flags & CALL_FLAG_DONT_LOCK_SESSION);
	SessionIoHandoff handoff;
	if (!locked && IS_PRESENT(ctx) && IS_PRESENT(ctx->ht_ptr))
		locked = IsObjectInLockedObjectsStore(ctx->ht_ptr, to);
	if (!ctx || !ctx->io_claim || !ctx->io_claim_work || *ctx->io_claim != from_holder) return;
	if (!locked || WorkQueueLock(SessionsDelegatorPtr(), 0) != 0) return;
	SessionIoWork dest_identity = {.session = to_holder, .session_id = SESSION_ID(to),
	  .fd = SESSION_SOCKETFD(to), .generation = SessionIoGeneration(&to->ufsrv_session.io_work)};
	handoff = SessionIoHandoffHeld(&session_io_queue, &from->ufsrv_session.io_work, &to->ufsrv_session.io_work,
	                               ctx->io_claim_work, &dest_identity);
	WorkQueueUnLock(SessionsDelegatorPtr());
	if (handoff == SESSION_IO_HANDOFF_MOVED) {
		SessionIncrementReference(to_holder, 1);
		*ctx->io_claim_work = dest_identity;
		*ctx->io_claim = to_holder;
		SessionDecrementReference(from_holder, 1);
	} else if (handoff == SESSION_IO_HANDOFF_DEST_PIN) {
		*ctx->io_claim_work = dest_identity;
		*ctx->io_claim = to_holder;
		SessionDecrementReference(from_holder, 1);
	}
}

static inline UFSRVResult *						/*orig, local						                               new, transient state*/
_InstateUnconnectedSessionAsConnected(InstanceHolderForSession *instance_sesn_ptr_migrated, InstanceHolderForSession *instance_sesn_ptr_transient, unsigned long call_flags, UFSRVResult *res_ptr_in)
{
  Session     *sesn_ptr_migrated = SessionOffInstanceHolder(instance_sesn_ptr_migrated);
  Session     *sesn_ptr_transient = SessionOffInstanceHolder(instance_sesn_ptr_transient);

#ifdef __UF_TESTING
	 syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p', cid:'%lu'): INSTATING: (cid_migrated:'%lu' o_migrated:'%p', socket_fd:'%d')", __func__, pthread_self(),  sesn_ptr_transient, SESSION_ID(sesn_ptr_transient), SESSION_ID(sesn_ptr_migrated), sesn_ptr_migrated, SESSION_SOCKETFD(sesn_ptr_migrated));
#endif

	 //note try lock is on
	if (!(call_flags&CALL_FLAG_DONT_LOCK_SESSION)) {
		SessionLockRWCtx(THREAD_CONTEXT_PTR, AS_UFSRV_SESSION(sesn_ptr_migrated), _LOCK_TRY_FLAG_TRUE, __func__);
		if (_RESULT_TYPE_EQUAL(THREAD_CONTEXT_UFSRV_RESULT(THREAD_CONTEXT), RESULT_TYPE_ERR)) {
			#if UFSRVWEBSOCK_DEBUG_BUILD
			syslog(LOG_WARNING, "LOCKTRACE exit='instate_lock_failed' code='RESCODE_PROG_LOCKED' o_transient:'%p' cid_transient:'%lu' o_migrated:'%p' cid_migrated:'%lu' tid:'%lu'", sesn_ptr_transient, SESSION_ID(sesn_ptr_transient), sesn_ptr_migrated, SESSION_ID(sesn_ptr_migrated), pthread_self());
			#endif
			_RETURN_RESULT_RES(res_ptr_in, instance_sesn_ptr_transient, RESULT_TYPE_ERR, RESCODE_PROG_LOCKED) //will cause hashed object to be killed
		}
	}

	 bool lock_already_owned = (_RESULT_CODE_EQUAL(THREAD_CONTEXT_UFSRV_RESULT(THREAD_CONTEXT), RESCODE_PROG_LOCKED_BY_THIS_THREAD));
	 Socket *new_sptr;
	 redisReply *redis_ptr;

	 RemoveSessionToMonitoredWorkEvents(instance_sesn_ptr_transient); //remove association new socket<->new Session in epoll and stop monitoring socket

	 new_sptr = sesn_ptr_transient->ufsrv_session.ssptr;//live socket we need it
  sesn_ptr_transient->ufsrv_session.ssptr = sesn_ptr_migrated->ufsrv_session.ssptr; //this is a throw away socket

	 sesn_ptr_migrated->ufsrv_session.ssptr = new_sptr;//hijack new Socket, allowing us to create new association between new socket <->orig Session

	 SESSION_PROTOCOL_SESSION_DATA(sesn_ptr_migrated) = SESSION_PROTOCOL_SESSION_DATA(sesn_ptr_transient);
	 LOAD_NULL(SESSION_PROTOCOL_SESSION_DATA(sesn_ptr_transient));

	 //TODO: WE AUTOMATICALLY GET THE NEW SOCKET ADDRESS, BUT WE LOSE MSGQUEUE FOR EXISTING SOCKET MAY HAVE ENTRIES IN IT


	 if (call_flags&CALL_FLAG_TRANSFER_DB_USERDATA) {
		 TransferBasicSessionDbBackendDataFromSession(sesn_ptr_migrated, sesn_ptr_transient);//this is destructive to sesn_ptr_transient as those fields will be nulled out

#ifdef __UF_TESTING
		 syslog(LOG_DEBUG, "%s {pid:'%lu', o:'%p', cid:'%lu', uid_transferred:'%lu', uname_transferred:'%s', nickname_transferred:'%s'}: Transferred Basic Session Data from transient session...", __func__, pthread_self(), sesn_ptr_transient, SESSION_ID(sesn_ptr_transient), SESSION_USERID(sesn_ptr_transient), SESSION_USERNAME(sesn_ptr_transient), SESSION_USERNICKNAME(sesn_ptr_transient));
#endif
	 }

	 if (AddSessionToMonitoredWorkEvents(instance_sesn_ptr_migrated)) {
		 sesn_ptr_migrated->ufsrv_session.event_descriptor = sesn_ptr_transient->ufsrv_session.event_descriptor; //reassign the network event

		 //both sessions are in suspend and has session id hash. orig has cookie hash. new session has cookie hash if swap flag is set
		 //at this point we may receive network events, but the main loop will ignore t due to suspend flag

		 if (call_flags&CALL_FLAG_SWAP_COOKIE) {
#ifdef __UF_TESTING
			 syslog(LOG_DEBUG, "%s (pid:'%lu' cid:'%lu'): SWAPPING CONNECTED TRANSIENT cookie:'%s' INTO MIGRATED cookie:'%s'...", __func__, pthread_self(), SESSION_ID(sesn_ptr_transient), SESSION_COOKIE(sesn_ptr_transient), SESSION_COOKIE(sesn_ptr_migrated));
#endif

			 //0)this was set at request initialisation to block repeated incoming requests against the same cookie
			 RemoveFromHash(&(SessionsDelegatorPtr()->hashed_cookies.hashtable), (void *)instance_sesn_ptr_transient);
			 //1)remove old cookie associated with migrated session: both local and backend
			 RemoveFromHash(&(SessionsDelegatorPtr()->hashed_cookies.hashtable), (void *)instance_sesn_ptr_migrated);

			 //This hash may not be in the backend just yet
      redis_ptr = (*THREAD_CONTEXT_SESSION_CACHEBACKEND->send_command)(sesn_ptr_migrated, THREAD_CONTEXT_SESSION_CACHEBACKEND, REDIS_CMD_COOKIE_SESSION_DEL, SESSION_COOKIE(sesn_ptr_migrated));
      if (IS_PRESENT(redis_ptr)) freeReplyObject(redis_ptr);

			 //2) shift cookie across
			 memcpy(SESSION_COOKIE(sesn_ptr_migrated), SESSION_COOKIE(sesn_ptr_transient), strlen(sesn_ptr_transient->ufsrv_session.session_cookie) + 1);

			 //3) update local hash new cookie against migrated session, because they are different object instances
			 if (!(AddToHash(&(SessionsDelegatorPtr()->hashed_cookies.hashtable), (void *)instance_sesn_ptr_migrated))) {
				 //TODO: MORE RECOVERY FOR SWAP OPTION
				 RemoveSessionToMonitoredWorkEvents(instance_sesn_ptr_migrated);
				 //undo everything and re-engage new_sesn_ptr
				 _RECOVERY_BLOCK
				 #if UFSRVWEBSOCK_DEBUG_BUILD
				 syslog(LOG_WARNING, "LOCKTRACE exit='instate_hash_failed_swap' code='RESCODE_PROG_HASHED' o_transient:'%p' cid_transient:'%lu' o_migrated:'%p' cid_migrated:'%lu' tid:'%lu'", sesn_ptr_transient, SESSION_ID(sesn_ptr_transient), sesn_ptr_migrated, SESSION_ID(sesn_ptr_migrated), pthread_self());
				 #endif
				 _RETURN_RESULT_RES(res_ptr_in, instance_sesn_ptr_transient, RESULT_TYPE_ERR, RESCODE_PROG_HASHED)
			 }

			 //4)backend update for migrated happens below
		 } else {
       RemoveFromHash(&(SessionsDelegatorPtr()->hashed_cookies.hashtable), (void *)instance_sesn_ptr_transient); //was set at request initialisation
			 //cookie backend update for migrated happens below
			 if (!(AddToHash(&(SessionsDelegatorPtr()->hashed_cookies.hashtable), (void *)instance_sesn_ptr_migrated))) {
				 RemoveSessionToMonitoredWorkEvents(instance_sesn_ptr_migrated);
				 //undo everything and re-engage new_sesn_ptr
         AddToHash(&(SessionsDelegatorPtr()->hashed_cookies.hashtable), (void *)instance_sesn_ptr_transient);
				 _RECOVERY_BLOCK
				 #if UFSRVWEBSOCK_DEBUG_BUILD
				 syslog(LOG_WARNING, "LOCKTRACE exit='instate_hash_failed_noswap' code='RESCODE_PROG_HASHED' o_transient:'%p' cid_transient:'%lu' o_migrated:'%p' cid_migrated:'%lu' tid:'%lu'", sesn_ptr_transient, SESSION_ID(sesn_ptr_transient), sesn_ptr_migrated, SESSION_ID(sesn_ptr_migrated), pthread_self());
				 #endif
				 _RETURN_RESULT_RES(res_ptr_in, instance_sesn_ptr_transient, RESULT_TYPE_ERR, RESCODE_PROG_HASHED)
			 }
		 }

		 //status so far:
		 //cookie for migrated locally hashed. if swap cookie was on: cookie for connected us shifted and hash updated accordingly
		 //no backend update except for del of connected hash if swap was on

		 //SESNSTATUS_HANDSHAKEDis originally set in ProcessOutgoingWsHandshake for non migrated sessions
		 sesn_ptr_migrated->ufsrv_session.stat |= SESNSTATUS_HANDSHAKED;
		 sesn_ptr_migrated->ufsrv_session.stat |= SESNSTATUS_AUTHENTICATED;
		 sesn_ptr_migrated->ufsrv_session.stat |= SESNSTATUS_CONNECTED;

		sTransferHeldCell(instance_sesn_ptr_transient, instance_sesn_ptr_migrated, call_flags);

		 {//start reset new session as we no longer need that
			 //this will trigger a broadcast and update the corresponding UID:xxx status, which is not desirable,
			 //but we restore it with the update below
			 if (SuspendSession(instance_sesn_ptr_transient, SOFT_SUSPENSE)) {
				 SESNSTATUS_SET(sesn_ptr_transient->ufsrv_session.stat, SESNSTATUS_DEFERRED_RECYCLE);
				SessionUnLockCtx(THREAD_CONTEXT_PTR, AS_UFSRV_SESSION(sesn_ptr_transient), __func__);//lock should be owned by this thread
			 } else {
				 syslog(LOG_ERR, "%s (pid:'%lu', o_new:'%p', o_migrated:'%p', cid_new:'%lu'): ERROR: COULD NOT SUSPEND CONNECTED SESSION (cid:'%lu'): KILLING SESSION AND CONTINUING WITH MIGRATED cid_migrated:'%lu'",
					 __func__, pthread_self(), sesn_ptr_transient, sesn_ptr_migrated, SESSION_ID(sesn_ptr_transient), SESSION_ID(sesn_ptr_transient), SESSION_ID(sesn_ptr_migrated));

				 //this is a faulty object we ought to flush it out of the system and continue with migrated session.CALL_FLAG_TRANSFER_SESSION_ACCESS_CONTEXT passed just in case
				 ClearLocalSessionCache(instance_sesn_ptr_transient, CALL_FLAG_UNLOCK_SESSION|CALL_FLAGS_KILL_SESSION|CALL_FLAG_DONT_BROADCAST_FENCE_EVENT|CALL_FLAG_TRANSFER_SESSION_ACCESS_CONTEXT);
			 }

			 //controls flow naturally wth migrated session
		 }//end reset transient session

		 //>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
		 //DON't USE SESN_PTR_TRANSIENT beyond this point either recycled or killed
		 //>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

		 //CALL_FLAG_SWAP_COOKIE forces a cookie hash so no need to do it again
		 if ((call_flags&CALL_FLAG_HASH_SESSION_LOCALLY) && !(call_flags&CALL_FLAG_SWAP_COOKIE)) {
			 if (!(AddToHash(&(SessionsDelegatorPtr()->hashed_sessions.hashtable), (void *)instance_sesn_ptr_migrated))) {
				 RemoveFromHash(&(SessionsDelegatorPtr()->hashed_cookies.hashtable), (void *) instance_sesn_ptr_migrated);

				 RemoveSessionToMonitoredWorkEvents(instance_sesn_ptr_migrated);

				 //we cannot recover sesn_ptr_new so we must wrap up the whole lot and get the user to reconnect
				 //ask the calling environment to kill migrated it is of no use
				 #if UFSRVWEBSOCK_DEBUG_BUILD
				 syslog(LOG_WARNING, "LOCKTRACE exit='instate_hash_failed_session' code='RESCODE_PROG_HASHED' o_transient:'%p' cid_transient:'%lu' o_migrated:'%p' cid_migrated:'%lu' tid:'%lu'", sesn_ptr_transient, SESSION_ID(sesn_ptr_transient), sesn_ptr_migrated, SESSION_ID(sesn_ptr_migrated), pthread_self());
				 #endif
				 _RETURN_RESULT_RES(res_ptr_in, NULL, RESULT_TYPE_ERR, RESCODE_PROG_HASHED) //note NULL is RETURNED
			 }
		 }

		 if (call_flags&CALL_FLAG_HASH_USERNAME_LOCALLY) {
			 if (!(AddToHash(&(SessionsDelegatorPtr()->hashed_usernames.hashtable), (void *)instance_sesn_ptr_migrated))) {
				 RemoveFromHash(&(SessionsDelegatorPtr()->hashed_cookies.hashtable), (void *) instance_sesn_ptr_migrated);
				 RemoveFromHash(&(SessionsDelegatorPtr()->hashed_sessions.hashtable), (void *) instance_sesn_ptr_migrated);

				 RemoveSessionToMonitoredWorkEvents(instance_sesn_ptr_migrated);

				 #if UFSRVWEBSOCK_DEBUG_BUILD
				 syslog(LOG_WARNING, "LOCKTRACE exit='instate_hash_failed_username' code='RESCODE_PROG_HASHED' o_transient:'%p' cid_transient:'%lu' o_migrated:'%p' cid_migrated:'%lu' tid:'%lu'", sesn_ptr_transient, SESSION_ID(sesn_ptr_transient), sesn_ptr_migrated, SESSION_ID(sesn_ptr_migrated), pthread_self());
				 #endif
				 _RETURN_RESULT_RES(res_ptr_in, NULL, RESULT_TYPE_ERR, RESCODE_PROG_HASHED)
			 }
		 }

		 if (call_flags&CALL_FLAG_HASH_UID_LOCALLY) {
			 if (!(AddToHash(&(SessionsDelegatorPtr()->hashed_userids.hashtable), (void *)instance_sesn_ptr_migrated))) {
				 RemoveFromHash(&(SessionsDelegatorPtr()->hashed_cookies.hashtable), (void *) instance_sesn_ptr_migrated);
				 RemoveFromHash(&(SessionsDelegatorPtr()->hashed_sessions.hashtable), (void *) instance_sesn_ptr_migrated);
				 RemoveFromHash(&(SessionsDelegatorPtr()->hashed_usernames.hashtable), (void *) instance_sesn_ptr_migrated);

				 RemoveSessionToMonitoredWorkEvents(instance_sesn_ptr_migrated);

				 #if UFSRVWEBSOCK_DEBUG_BUILD
				 syslog(LOG_WARNING, "LOCKTRACE exit='instate_hash_failed_uid' code='RESCODE_PROG_HASHED' o_transient:'%p' cid_transient:'%lu' o_migrated:'%p' cid_migrated:'%lu' tid:'%lu'", sesn_ptr_transient, SESSION_ID(sesn_ptr_transient), sesn_ptr_migrated, SESSION_ID(sesn_ptr_migrated), pthread_self());
				 #endif
				 _RETURN_RESULT_RES(res_ptr_in, NULL, RESULT_TYPE_ERR, RESCODE_PROG_HASHED)
			 }
		 }

		 SESNSTATUS_SET(sesn_ptr_migrated->ufsrv_session.stat, SESNSTATUS_INSERVICE);

		 SESSION_WHEN_SERVICE_STARTED(sesn_ptr_migrated) = SESSION_WHEN_SERVICE_STARTED(sesn_ptr_transient);

		 SESNSTATUS_UNSET(sesn_ptr_migrated->ufsrv_session.stat, SESNSTATUS_SUSPENDED);
     SESNSTATUS_UNSET(sesn_ptr_migrated->ufsrv_session.stat, SESNSTATUS_TRANSIENT);
		 SESNSTATUS_UNSET(sesn_ptr_migrated->ufsrv_session.stat, SESNSTATUS_IDLED);
		 SESNSTATUS_SET(sesn_ptr_migrated->ufsrv_session.stat, SESNSTATUS_CONNECTED);
		 SESNSTATUS_UNSET(sesn_ptr_migrated->ufsrv_session.stat, SESNSTATUS_MIGRATED);
		 SESNSTATUS_UNSET(sesn_ptr_migrated->ufsrv_session.stat, SESNSTATUS_REMOTE);
		 SESNSTATUS_UNSET(sesn_ptr_migrated->ufsrv_session.stat, SESNSTATUS_REMOTE_CONNECTED);
		 SESNSTATUS_UNSET(sesn_ptr_migrated->ufsrv_session.stat, SESNSTATUS_IOERROR);

		 if (CALLGFLAG_IS_SET(call_flags, CALL_FLAG_WRITEBACK_SESSION_DATA_TO_BACKEND)) {
			 UFSRVResult *res_ptr = RefreshBackendCacheForSession (sesn_ptr_migrated, NULL, CALL_FLAG_DONT_BROADCAST_SESSION_EVENT); //because we testing for it below
			 if (_RESULT_TYPE_ERROR(res_ptr)) {
         RemoveFromHash(&(SessionsDelegatorPtr()->hashed_cookies.hashtable), (void *) instance_sesn_ptr_migrated);
         RemoveFromHash(&(SessionsDelegatorPtr()->hashed_sessions.hashtable), (void *) instance_sesn_ptr_migrated);
         RemoveFromHash(&(SessionsDelegatorPtr()->hashed_usernames.hashtable), (void *) instance_sesn_ptr_migrated);
         RemoveFromHash(&(SessionsDelegatorPtr()->hashed_userids.hashtable), (void *) instance_sesn_ptr_migrated);

         RemoveSessionToMonitoredWorkEvents(instance_sesn_ptr_migrated);

         #if UFSRVWEBSOCK_DEBUG_BUILD
         syslog(LOG_WARNING, "LOCKTRACE exit='instate_error_propagated' code='res_ptrresult_code' o_transient:'%p' cid_transient:'%lu' o_migrated:'%p' cid_migrated:'%lu' tid:'%lu'", sesn_ptr_transient, SESSION_ID(sesn_ptr_transient), sesn_ptr_migrated, SESSION_ID(sesn_ptr_migrated), pthread_self());
         #endif
         _RETURN_RESULT_RES(res_ptr_in, NULL, RESULT_TYPE_ERR, res_ptr->result_code)
			 }
		 }

		 //by default event is broadcasted unless CALL_FLAG_DONT_BROADCAST_SESSION_EVENT is passed
		 if (!(CALLGFLAG_IS_SET(call_flags, CALL_FLAG_DONT_BROADCAST_SESSION_EVENT))) {
			 if (IsProtocolPubSessionTransitions(PROTO_PROTOCOL_ID(((Protocol *)SESSION_PROTOCOLTYPE(sesn_ptr_migrated))))) {
				 InterBroadcastSessionStatus(sesn_ptr_migrated, NULL, SESSION_MESSAGE__STATUS__CONNECTED, 0);
			 }
		 }

		 //statsd_inc(THREAD_CONTEXT_INSTRUMENTATION_BACKEND, "sessions.resumed", 1.0);

		 //success

		 //return sesn_ptr_migrated;//sesn state: locked
		 #if UFSRVWEBSOCK_DEBUG_BUILD
		 syslog(LOG_WARNING, "LOCKTRACE exit='instate_success' code='RESCODE_USER_INSTATED' o_transient:'%p' cid_transient:'%lu' o_migrated:'%p' cid_migrated:'%lu' tid:'%lu'", sesn_ptr_transient, SESSION_ID(sesn_ptr_transient), sesn_ptr_migrated, SESSION_ID(sesn_ptr_migrated), pthread_self());
		 #endif
		 _RETURN_RESULT_RES(res_ptr_in, instance_sesn_ptr_migrated, RESULT_TYPE_SUCCESS, RESCODE_USER_INSTATED)
	 } else {
		 syslog(LOG_ERR, "%s (pid:'%lu' o:'%p', cid:'%lu'): SEVERE ERROR: COULD NOT POLL NEWLY UNSUSPENDED ORIG SESSION(cid:'%lu', o:'%p'): RESTORING NEW SESSION (cid='%lu')...",
			 __func__, pthread_self(), sesn_ptr_transient, SESSION_ID(sesn_ptr_transient), SESSION_ID(sesn_ptr_migrated), sesn_ptr_migrated, SESSION_ID(sesn_ptr_transient));

		 connected_session_recovery:

		 //restore transient session
		 //backend cookie hash is not affected as it has not been touched at this point
		 new_sptr = sesn_ptr_transient->ufsrv_session.ssptr;	//orig' original socket
     sesn_ptr_transient->ufsrv_session.ssptr = sesn_ptr_migrated->ufsrv_session.ssptr; 	//this new original socket
		 sesn_ptr_migrated->ufsrv_session.ssptr = new_sptr;	//orig gets its original socket

     SESSION_PROTOCOL_SESSION_DATA(sesn_ptr_transient) = SESSION_PROTOCOL_SESSION_DATA(sesn_ptr_migrated);
     LOAD_NULL(SESSION_PROTOCOL_SESSION_DATA(sesn_ptr_migrated));

		 //new_sesn_ptr->ufsrv_session.ssptr=orig_sesn_ptr->ufsrv_session.ssptr;
		 SESNSTATUS_UNSET(sesn_ptr_transient->ufsrv_session.stat, SESNSTATUS_SUSPENDED);
		 AddSessionToMonitoredWorkEvents(instance_sesn_ptr_transient);

		 //we did not succeed so unlock the original nd restore it to original condition
		 //caller will unlock new
		 //>>>>>>>>>>>>>>>>>>>>>>>
		 if (!(call_flags&CALL_FLAG_DONT_LOCK_SESSION)) SessionUnLockCtx(THREAD_CONTEXT_PTR, AS_UFSRV_SESSION(sesn_ptr_migrated), __func__);
		 //>>>>>>>>>>>>>>>>>>>>>>>

		 //instruct the user to reauthenticate
		 //returning environment must manage migrated session reclamation
		 #if UFSRVWEBSOCK_DEBUG_BUILD
		 syslog(LOG_WARNING, "LOCKTRACE exit='instate_poll_error' code='RESCODE_IO_POLLERROR' o_transient:'%p' cid_transient:'%lu' o_migrated:'%p' cid_migrated:'%lu' tid:'%lu'", sesn_ptr_transient, SESSION_ID(sesn_ptr_transient), sesn_ptr_migrated, SESSION_ID(sesn_ptr_migrated), pthread_self());
		 #endif
		 _RETURN_RESULT_RES(res_ptr_in, instance_sesn_ptr_transient, RESULT_TYPE_ERR, RESCODE_IO_POLLERROR)

		 //no change in state sesn_ptr_transient remains locked. up to calling environment to unlock
	 }

}

/**
 * 	@brief: Helper function to ease Sessions through final stage of being let through
 *
 * 	@note Required lock: sesn_ptr_backend: in main loop
 * 	@note Unlocks: None
 * 	@note Locks: None
 *
 * 	@note Flags: CALL_FLAG_SUSSPEND_SESSION: suspend session on error
 */
static UFSRVResult *
HandleSessionReturnHandshake(InstanceHolderForSession *instance_sesn_ptr, SocketMessage *sock_msg_ptr, unsigned long sesn_call_flags)
{
	Session *sesn_ptr = SessionOffInstanceHolder(instance_sesn_ptr);

	///>>>>> sesn_ptr_backend IS NOW LOCKED. sesn_ptr returned to pool <<<<<<<<<<<<<

	//user good to roll off: acknowledge the connection by replying in WS protocol
	UFSRVResult *res_ptr_aux;
	res_ptr_aux = ProcessOutgoingWsHandshake(instance_sesn_ptr, sock_msg_ptr);

	switch (_RESULT_TYPE(res_ptr_aux))
	{
	case RESULT_TYPE_ERR:
		if (_RESULT_CODE(res_ptr_aux) == RESCODE_IO_CONNECTIONCLOSED) {
			syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p' cid:'%lu'):  COULD NOT HANDSHAKE: connection closed", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr));
		} else if (_RESULT_CODE(res_ptr_aux) == RESCODE_PROTOCOL_WSHANDSHAKE) {
			syslog(LOG_DEBUG, "%s (pid:'%lu', o:'%p' cid:'%lu'):  COULD NOT HANDSHAKE: parsing error", __func__, pthread_self(), sesn_ptr, SESSION_ID(sesn_ptr));
		}

		statsd_gauge_inc(THREAD_CONTEXT_INSTRUMENTATION_BACKEND, "worker.work.handshake_failed", 1, UfsrvConfigInstrumentationBackendServerAsPtr());

		if (CALLGFLAG_IS_SET(sesn_call_flags, CALL_FLAG_SUSSPEND_SESSION))	SuspendSession(instance_sesn_ptr, SOFT_SUSPENSE);

		_RETURN_RESULT_SESN(sesn_ptr, instance_sesn_ptr, RESULT_TYPE_ERR, RESCODE_PROTOCOL_WSHANDSHAKE)
}

	exit_success:
		_RETURN_RESULT_SESN(sesn_ptr, instance_sesn_ptr, RESULT_TYPE_SUCCESS, RESCODE_PROTOCOL_WSHANDSHAKE)
}