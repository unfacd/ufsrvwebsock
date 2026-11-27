# Session authentication regression checks

`session_auth_guard_tests` exercises the production claim registry and holder-selection helper. It covers missing indexes, matching indexes, conflicting holders, cookie contention, different cookies for one user, independent users, UID reassignment rejection, release/retry, and 4,800 threaded claims. Run the target through CTest with a test-enabled build.

The sign-on proof invokes the real CLI against endpoints in the selected account configuration:

```sh
python3 tests/session_auth/same_cookie_signon_proof.py \
  --home-base /tmp/ufrvrcli/test_agents --account 1 --workers 10 --rounds 10
```

Use a test account with no separately held connection. CLI exits 0 (accepted) and 1 (rejected) are expected, with at least one accepted sign-on required in every round. An unavailable server returning only CLI failures cannot pass. Timeouts, signals and other exits fail. This proof sends no fence or message commands. Run under GDB or sanitizers to detect server faults; bounded client responses alone do not prove lifetime safety or availability for other accounts.

On 2026-11-11 the original server crashed under the sign-on-only reproducer. GDB captured a null session-ID holder selected in the UID-only branch. The original core is retained in the Docker container at `/tmp/ufsrvwebsock-before-auth-remediation.core`; the local transcript is `/tmp/ufsrvwebsock-same-cookie-crash.gdb.txt`.

Remediation checks include the CTest target, ASan/UBSan and TSan runs of the production guard, and live sign-on replay against a temporary candidate under GDB. An additional live socket check held account 1 connected, rejected ten duplicate sign-ons, accepted account 2, retained the established socket, and accepted account 1 after disconnect. This is a finite regression check, not an exhaustive proof of every session lifecycle interleaving.

Takeover policy tests cover live/fresh rejection, dead sockets, expired connected/locationless idle limits, unfinished service expiry, completed-service progress, disabled limits, and unknown/future timestamps. The routing decision uses `sIsSocketAlive()` and evaluates `when_serviced_start`/`when_serviced_end` under the session lock. Target lock contention rejects the newcomer without destruction; a stale timestamp never authorizes clearing a lock owned by another worker.
