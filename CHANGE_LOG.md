# Change Log — ufsrvwebsock

## Release-261127 — 5.0.1
`05e06ab` → `3223cd21` · 2026-07-16 → 2026-11-27

### New
- **Sessions** — authentication, account and session resolution, resumption and takeover, a composed session view 
  (UfsrvSession + UfnetSession)
- **Delivery** — asynchronous delivery staged per event and per user, g


### Fixes
- **Delivery** — recipient policy is applied before a message is staged, not after
- **Session** — WebSocket state is released when the protocol resets
- **Build** — valgrind gating is fixed, a build directory pinned to a scratch companion prefix is rejected, and the protobuf schema follows `ufnetcorelib`

### Deprecated
- (none)

### Removed
- The dead assets left behind by the monorepo split
