#ifndef SRC_INCLUDE_SESSION_AUTH_ROUTING_H_
#define SRC_INCLUDE_SESSION_AUTH_ROUTING_H_

/**
 * @file session_auth_routing.h
 * @brief Session authentication, replacement and post-handshake routing contracts.
 */

#include <ufsrv_core/ufsrvresult_type.h>
#include <ufnetcorelib/ufnet_core/session/session_type.h>
#include <uflib/net/socket_message_type.h>

/**
 * @brief Authenticate against a referenced holder already indexed by cookie.
 * @param[in] instance_sesn_ptr_transient Incoming session whose write lock the I/O worker owns.
 * @param[in] instance_sesn_ptr_hashed Pinned cookie lookup result; retained through this call.
 * @param[in,out] sock_msg_ptr Parsed handshake message.
 * @return Session-owned authentication result; its holder can replace the incoming holder.
 * @pre Execute inside HandlePostSuccessfulIncomingHandshake() with a thread-local sign-on claim.
 * @note A busy target is preserved. A connected target is preserved only when its socket is live and service timestamps are fresh; stale or dead targets can be taken over.
 * @code{.c}
 * UFSRVResult *result = AuthenticateForCookieHashedSession(transient, pinned, message);
 * if (_RESULT_TYPE_ERROR(result)) { return result; }
 * return result;
 * @endcode
 */
UFSRVResult *AuthenticateForCookieHashedSession(InstanceHolderForSession *instance_sesn_ptr_transient, InstanceHolderForSession *instance_sesn_ptr_hashed, SocketMessage *sock_msg_ptr);
/**
 * @brief Transfer an incoming connection to an existing session during guarded authentication.
 * @param[in] instance_sesn_ptr_unconnected Existing holder protected by the caller's lifetime reference.
 * @param[in] instance_sesn_ptr_transient Incoming holder whose write lock the caller owns.
 * @param[in] sesn_call_flags Controls session locking, hashing and backend writeback.
 * @return Session-owned transition result, or generic error when no carrier remains.
 * @warning Success can suspend and defer recycling of the incoming session; use the returned holder.
 * @note Failed target try-lock/hash acquisition rejects the incoming connection without destroying the target.
 * @code{.c}
 * UFSRVResult *result = InstateUnconnectedSession(existing, incoming, flags);
 * if (_RESULT_TYPE_ERROR(result)) { return result; }
 * return result;
 * @endcode
 */
UFSRVResult *InstateUnconnectedSession(InstanceHolderForSession *instance_sesn_ptr_unconnected, InstanceHolderForSession *instance_sesn_ptr_transient, unsigned long sesn_call_flags);
/**
 * @brief Authenticate an incoming connection under cookie and resolved-account exclusion.
 * @param[in] instance_sesn_ptr_transient Incoming holder, write-locked by the session I/O worker.
 * @param[in,out] sock_msg_ptr Parsed handshake message forwarded to lifecycle callbacks.
 * @return Authentication/lifecycle result; result_user_data identifies the holder returned to I/O.
 * @pre The current thread has initialized server worker context and access to the session registries.
 * @note Claims and temporary references are released on return. Duplicate rejection suspends only the newcomer.
 * @note A returned replacement holder retains the session lock required by the I/O caller.
 * @code{.c}
 * UFSRVResult *result = HandlePostSuccessfulIncomingHandshake(incoming, message);
 * if (_RESULT_TYPE_ERROR(result)) { return result; }
 * return result;
 * @endcode
 */
UFSRVResult * HandlePostSuccessfulIncomingHandshake(InstanceHolderForSession *instance_sesn_ptr_transient, SocketMessage *sock_msg_ptr);


#endif /* SRC_INCLUDE_SESSION_AUTH_ROUTING_H_ */
