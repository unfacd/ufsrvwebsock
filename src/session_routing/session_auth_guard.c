/**
 * @file session_auth_guard.c
 * @brief Maintain sign-on claims and select referenced local session holders.
 */

#ifdef HAVE_CONFIG_UFSRVWEBSOCK_H
#include <config_ufsrvwebsock.h>
#endif

#include <session_auth_guard.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <uflib/adt/adt_hashtable.h>
#include <uflib/exponential_backoff/algorithm.h>
#include <uflib/optional/ufoptional.h>

/* A contended index is transient: wait it out briefly, sized for the handshake path. */
#define SESSION_AUTH_LOCK_BACKOFF_BASE_MS   (1U)
#define SESSION_AUTH_LOCK_BACKOFF_MAX_MS    (16U)
#define SESSION_AUTH_LOCK_BACKOFF_MAX_TRIES (5U)

/**
 * @brief Sleep on the calling thread for a bounded backoff interval.
 * @param[in] delay_ms Milliseconds to wait.
 */
static void
sSleepMillis(uint16_t delay_ms)
{
  struct timespec remaining = {.tv_sec = delay_ms / 1000, .tv_nsec = (long)(delay_ms % 1000) * 1000000L};

  while ((nanosleep(&remaining, &remaining) == -1) && (errno == EINTR)) {}
}

bool
SessionAuthClaimBegin(SessionAuthClaims *claims, SessionAuthClaim *claim, const char *cookie)
{
  if (!cookie || !*cookie || pthread_mutex_lock(&claims->mutex) != 0) return false;
  for (SessionAuthClaim *current = claims->head; current; current = current->next) {
    if (strcmp(current->cookie, cookie) == 0) {
      pthread_mutex_unlock(&claims->mutex);
      return false;
    }
  }
  *claim = (SessionAuthClaim){.cookie = cookie, .next = claims->head};
  claims->head = claim;
  pthread_mutex_unlock(&claims->mutex);
  return true;
}

bool
SessionAuthClaimUser(SessionAuthClaims *claims, SessionAuthClaim *claim, unsigned long uid)
{
  if (!claim || !uid || pthread_mutex_lock(&claims->mutex) != 0) return false;
  bool found = false;
  for (SessionAuthClaim *current = claims->head; current; current = current->next) {
    if (current == claim) found = true;
    else if (current->uid == uid) {
      pthread_mutex_unlock(&claims->mutex);
      return false;
    }
  }
  bool accepted = found && (!claim->uid || claim->uid == uid);
  if (accepted) claim->uid = uid;
  pthread_mutex_unlock(&claims->mutex);
  return accepted;
}

void
SessionAuthClaimEnd(SessionAuthClaims *claims, SessionAuthClaim *claim)
{
  pthread_mutex_lock(&claims->mutex);
  SessionAuthClaim **link = &claims->head;
  while (*link && *link != claim) link = &(*link)->next;
  if (*link) *link = claim->next;
  pthread_mutex_unlock(&claims->mutex);
}

void *
SessionAuthSelectHolder(void *by_session, void *by_user, bool *conflict)
{
  *conflict = by_session && by_user && by_session != by_user;
  return *conflict ? NULL : (by_user ? by_user : by_session);
}


bool
SessionAuthConnectionIsLive(bool socket_alive, bool in_service, time_t now, time_t started, time_t finished, unsigned long idle_timeout, unsigned long service_timeout)
{
  if (!socket_alive) return false;
  if (now <= 0) return true;
  if (in_service && service_timeout && started > 0 && started >= finished && now >= started &&
      difftime(now, started) > service_timeout) return false;
  time_t last_progress = started > finished ? started : finished;
  if (idle_timeout && last_progress > 0 && now >= last_progress &&
      difftime(now, last_progress) > idle_timeout) return false;
  return true;
}

/**
 * @brief Look up a referenced holder, waiting out a contended index lock.
 * @param[in] hash Index to read.
 * @param[in] key Lookup key.
 * @param[in] retain Reference-increment callback, or NULL.
 * @param[out] holder Matching holder, or NULL when the key is absent.
 * @return true for a completed lookup, including absence; false for invalid arguments, allocation
 *         failure, contention beyond the retry budget, or another index error.
 * @note Only EBUSY/EAGAIN are retried. Exhaustion is an error and never an absence, so a busy index
 *       cannot be mistaken for an unhashed key.
 */
bool
SessionAuthLookupHolder(HashTable *hash, const void *key, void (*retain)(RecyclerClientData *, int), void **holder)
{
  if (!holder) {
    errno = EINVAL;
    return false;
  }
  *holder = NULL;

  BackoffAlgorithmContext_t backoff;

  BackoffAlgorithm_InitializeParamsExponential(&backoff, SESSION_AUTH_LOCK_BACKOFF_BASE_MS,
                                               SESSION_AUTH_LOCK_BACKOFF_MAX_MS,
                                               SESSION_AUTH_LOCK_BACKOFF_MAX_TRIES);

  for (;;) {
    UfOptional *result = HashLookupWithReferenceOptional(hash, key, retain, NULL);

    if (result) {
      *holder = UfOptionalTake(result);
      UfOptionalDestroy(result);
      return true;
    }

    int error = errno;

    /* only lock contention is transient; every other error fails the lookup */
    if ((error != EBUSY) && (error != EAGAIN)) {
      syslog(LOG_WARNING, "%s: hash lookup failed (error:%d)", __func__, error);
      errno = error;
      return false;
    }

    uint16_t delay = 0;

    if (BackoffAlgorithm_GetNextBackoff(&backoff, 0, &delay) == BackoffAlgorithmRetriesExhausted) {
      syslog(LOG_WARNING, "%s: index remained locked after '%u' attempts (error:%d)", __func__,
             backoff.attemptsDone, error);
      errno = error;
      return false;
    }

    syslog(LOG_DEBUG, "%s: index contended (error:%d): retrying in '%u' ms", __func__, error,
           (unsigned)delay);

    sSleepMillis(delay);
  }
}
