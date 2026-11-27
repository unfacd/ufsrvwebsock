#ifndef UFSRVWEBSOCK_SESSION_AUTH_GUARD_H
#define UFSRVWEBSOCK_SESSION_AUTH_GUARD_H

/**
 * @file session_auth_guard.h
 * @brief Cookie and account exclusion for session authentication transitions.
 */

#include <stdbool.h>
#include <time.h>
#include <uflib/adt/adt_hashtable_type.h>
#include <uflib/recycler/recycler_type.h>
#include <session_auth_guard_type_priv.h>

/**
 * @brief Reserve a cookie for one in-flight authentication transition.
 * @param[in,out] claims Initialized process-local registry.
 * @param[out] claim Unregistered caller-owned storage; populated only on success.
 * @param[in] cookie Nonempty terminated string, immutable until SessionAuthClaimEnd().
 * @return true when registered; false for an invalid cookie, duplicate or lock failure.
 * @note The registry mutex is released before return; no session work runs under it.
 * @code{.c}
 * SessionAuthClaims registry = {.mutex = PTHREAD_MUTEX_INITIALIZER};
 * SessionAuthClaim claim;
 * if (SessionAuthClaimBegin(&registry, &claim, "session-cookie")) {
 *   SessionAuthClaimEnd(&registry, &claim);
 * }
 * pthread_mutex_destroy(&registry.mutex);
 * @endcode
 */
bool SessionAuthClaimBegin(SessionAuthClaims *claims, SessionAuthClaim *claim, const char *cookie);
/**
 * @brief Bind an active cookie claim to its resolved account without waiting for another sign-on.
 * @param[in,out] claims Registry containing claim.
 * @param[in,out] claim Active claim acquired by SessionAuthClaimBegin().
 * @param[in] uid Nonzero verified account ID; an existing binding cannot change.
 * @return true for a new or repeated binding; false for contention, invalid state or lock failure.
 * @note A rejected binding leaves the cookie claim active; its owner must still release it.
 * @code{.c}
 * SessionAuthClaims registry = {.mutex = PTHREAD_MUTEX_INITIALIZER};
 * SessionAuthClaim claim;
 * if (SessionAuthClaimBegin(&registry, &claim, "session-cookie")) {
 *   bool accepted = SessionAuthClaimUser(&registry, &claim, 663);
 *   if (!accepted) {
 *     SessionAuthClaimEnd(&registry, &claim);
 *     pthread_mutex_destroy(&registry.mutex);
 *     return false;
 *   }
 *   SessionAuthClaimEnd(&registry, &claim);
 * }
 * pthread_mutex_destroy(&registry.mutex);
 * @endcode
 */
bool SessionAuthClaimUser(SessionAuthClaims *claims, SessionAuthClaim *claim, unsigned long uid);
/**
 * @brief Remove a claim so another authentication transition can use its cookie and account.
 * @param[in,out] claims Registry initialized before use and still alive.
 * @param[in,out] claim Caller-owned claim previously registered in claims.
 * @note Call on every path after successful acquisition, before releasing claim or cookie storage.
 * @code{.c}
 * SessionAuthClaims registry = {.mutex = PTHREAD_MUTEX_INITIALIZER};
 * SessionAuthClaim claim;
 * if (SessionAuthClaimBegin(&registry, &claim, "session-cookie")) {
 *   SessionAuthClaimEnd(&registry, &claim);
 * }
 * pthread_mutex_destroy(&registry.mutex);
 * @endcode
 */
void SessionAuthClaimEnd(SessionAuthClaims *claims, SessionAuthClaim *claim);
/**
 * @brief Select an existing holder without dereferencing either independent index result.
 * @param[in] by_session Referenced session-ID lookup result, or NULL.
 * @param[in] by_user Referenced account-ID lookup result, or NULL.
 * @param[out] conflict Required output; true when both results exist and disagree.
 * @return Matching holder or sole present holder; NULL for conflicting or absent results.
 * @note Transfers no references. Retain lookup pins, then lock and validate the chosen session.
 * @code{.c}
 * int holder;
 * bool conflict;
 * void *selected = SessionAuthSelectHolder(NULL, &holder, &conflict);
 * if (conflict || selected != &holder) { return false; }
 * @endcode
 */
void *SessionAuthSelectHolder(void *by_session, void *by_user, bool *conflict);

/**
 * @brief Decide whether socket and service progress justify preserving the current connection.
 * @param[in] socket_alive Result of the routing module's socket-liveness probe.
 * @param[in] in_service Whether the session is marked as servicing work.
 * @param[in] now Current wall-clock time in seconds.
 * @param[in] started Start of the most recent service cycle, or zero if unknown.
 * @param[in] finished End of the most recent completed service cycle, or zero if unknown.
 * @param[in] idle_timeout Effective connected/locationless idle limit; zero disables it.
 * @param[in] service_timeout Maximum unfinished service age; zero disables it.
 * @return true when the newcomer should be declined; false when takeover is permitted.
 * @note Unknown/future timestamps alone do not prove staleness. The caller must own the session lock.
 * @code{.c}
 * bool keep_current = SessionAuthConnectionIsLive(true, false, 1000, 995, 996, 60, 20);
 * if (keep_current) { return false; }
 * @endcode
 */
bool SessionAuthConnectionIsLive(bool socket_alive, bool in_service, time_t now, time_t started, time_t finished, unsigned long idle_timeout, unsigned long service_timeout);

/**
 * @brief Perform a pinned lookup without confusing an operational error with an absent holder.
 * @param[in,out] hash Initialized local session index.
 * @param[in] key Lookup key in the configured index format.
 * @param[in] retain Reference increment callback executed under the hash read lock.
 * @param[out] holder Required output; receives the referenced holder, or NULL on absence/error.
 * @return true for a completed lookup, including absence; false on operational error with errno preserved.
 * @note Successful references transfer to the caller. The temporary optional wrapper is always destroyed.
 * @code{.c}
 * void *holder = NULL;
 * if (!SessionAuthLookupHolder(hash, key, RetainHolder, &holder)) { return false; }
 * if (holder) { ReleaseHolder((RecyclerClientData *)holder, 1); }
 * @endcode
 */
bool SessionAuthLookupHolder(HashTable *hash, const void *key, void (*retain)(RecyclerClientData *, int), void **holder);

#endif
