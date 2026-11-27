#ifndef UFSRVWEBSOCK_SESSION_AUTH_GUARD_TYPE_PRIV_H
#define UFSRVWEBSOCK_SESSION_AUTH_GUARD_TYPE_PRIV_H

/**
 * @file session_auth_guard_type_priv.h
 * @brief Private storage for in-flight cookie and account sign-on claims.
 */

#include <pthread.h>

/*! Caller-owned claim retained until the complete handshake path returns. */
typedef struct SessionAuthClaim {
  const char *cookie; ///< Borrowed, immutable cookie string; must outlive the active claim.
  unsigned long uid; ///< Resolved account ID, or zero until identity is established.
  struct SessionAuthClaim *next; ///< Registry link protected by SessionAuthClaims::mutex.
} SessionAuthClaim;

/*! Process-local registry; initialize its mutex and set head to NULL before use. */
typedef struct SessionAuthClaims {
  pthread_mutex_t mutex; ///< Protects claim insertion, account binding and removal only.
  SessionAuthClaim *head; ///< Borrowed active claims; never free or modify them outside registry operations.
} SessionAuthClaims;

#endif
