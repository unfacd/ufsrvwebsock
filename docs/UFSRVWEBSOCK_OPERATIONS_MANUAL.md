
# Operations manual

This document describes running the server: the service unit, the filesystem
layout it expects, the switches it accepts, where it logs, how its configuration
file is structured, and how to reach its command console. It deliberately
contains nothing about building, packaging or releasing the source.

The manual is maintained in two repos and is split the same way in both. Every
section up to and including **Command console** is the shared ufsrv server base:
it describes patterns common to any server derived from the ufsrv family and the
text is identical in both copies. The final section, **Flavour**, carries what
is specific to this server — mostly the configuration additions — and is the
only part that differs between the copies.

Throughout, `<server>` stands for this binary's flavour name (`ufsrvapi`,
`ufsrvwebsock`, …), and `<instance>` for the numeric instance id.

## Contents

- [1. Service unit](#1-service-unit)
- [2. Runtime basedir layout](#2-runtime-basedir-layout)
  - [2.1 The shared `/opt/ufsrv` tree](#21-the-shared-optufsrv-tree)
- [3. Instances, start-up and operator procedures](#3-instances-start-up-and-operator-procedures)
  - [3.1 Start-up sequence (happy path)](#31-start-up-sequence-happy-path)
  - [3.2 Configuration precedence](#32-configuration-precedence)
  - [3.3 Instances and operator procedures](#33-instances-and-operator-procedures)
- [4. Command-line switches](#4-command-line-switches)
- [5. Logging](#5-logging)
- [6. Configuration file](#6-configuration-file)
  - [6.1 `ufsrv` — identity and listeners](#61-ufsrv--identity-and-listeners)
  - [6.2 `ufsrv.session_workers_thread_pool`, `ufsrv_workers_thread_pool`, `listener_workers_thread_pool`](#62-ufsrvsession_workers_thread_pool-ufsrv_workers_thread_pool-listener_workers_thread_pool)
  - [6.3 `ufsrv.ssl_command_console` — admin console TLS](#63-ufsrvssl_command_console--admin-console-tls)
  - [6.4 `ufsrv.ssl_command_console_client` — outbound console client](#64-ufsrvssl_command_console_client--outbound-console-client)
  - [6.5 `ufsrv.stats_backend` / `ufsrv.instrumentation_backend`](#65-ufsrvstats_backend--ufsrvinstrumentation_backend)
  - [6.6 `ufsrv.user_timeouts` — session lifetimes, seconds](#66-ufsrvuser_timeouts--session-lifetimes-seconds)
  - [6.7 `ufsrv.buffer_sizes` — I/O buffers, bytes](#67-ufsrvbuffer_sizes--io-buffers-bytes)
  - [6.8 `ufsrv.memory_specs_for_session` — session pool](#68-ufsrvmemory_specs_for_session--session-pool)
  - [6.9 `ufsrv.fileloader` — concurrent file registry](#69-ufsrvfileloader--concurrent-file-registry)
  - [6.10 `ufsrv.sys_runtime` — privilege drop](#610-ufsrvsys_runtime--privilege-drop)
  - [6.11 `ufsrv.ratelimit`](#611-ufsrvratelimit)
  - [6.12 `ufsrv.email` — outbound mail (Mailgun-compatible API)](#612-ufsrvemail--outbound-mail-mailgun-compatible-api)
  - [6.13 `ufnet` — messaging-network backends](#613-ufnet--messaging-network-backends)
  - [6.14 Lua schema and validation assets](#614-lua-schema-and-validation-assets)
- [7. Redis persistence backend](#7-redis-persistence-backend)
  - [7.1 Session store](#71-session-store)
  - [7.2 Fence store](#72-fence-store)
    - [7.2.1 Delivery queue record](#721-delivery-queue-record)
    - [7.2.2 Delivery attempt log](#722-delivery-attempt-log)
  - [7.3 User-message store](#73-user-message-store)
    - [7.3.1 User delivery queue record](#731-user-delivery-queue-record)
    - [7.3.2 User delivery attempt log](#732-user-delivery-attempt-log)
    - [7.3.3 Guardian sync queue record](#733-guardian-sync-queue-record)
    - [7.3.4 State queue record](#734-state-queue-record)
  - [7.4 Message queue](#74-message-queue)
- [8. Command console](#8-command-console)
  - [8.1 Node catalogue (shared base)](#81-node-catalogue-shared-base)
  - [8.2 Console pitfalls](#82-console-pitfalls)
- [9. Troubleshooting](#9-troubleshooting)
- [10. Flavour: `ufsrvwebsock`](#10-flavour-ufsrvwebsock)
  - [10.1 `ufsrvwebsock.websocket` — protocol limits](#101-ufsrvwebsockwebsocket--protocol-limits)
  - [10.2 `ufsrvwebsock.cloud_service_accounts`](#102-ufsrvwebsockcloud_service_accounts)
  - [10.3 Additional console node](#103-additional-console-node)
  - [10.4 What differs from the base](#104-what-differs-from-the-base)

---

## 1. Service unit

The server ships a systemd *template* unit, `etc/deploy/systemd/<server>@.service`.
It is instantiated with a numeric id, and `%i` expands to that id everywhere the
unit needs it — so one template serves every instance on a host.

| Unit setting | Value | Why it matters |
|---|---|---|
| `User` / `Group` | `ufsrv` / `ufsrv` | The process never runs as root; the runtime tree is owned by this account. |
| `Type` | `notify` | The server signals readiness on the notify socket; `systemctl start` returns only once it is actually serving. |
| `NotifyAccess` | `all` | Readiness may be signalled from a worker thread, not just the main PID. |
| `WorkingDirectory` | `/opt/ufsrv/<server>/etc` | Relative paths in the unit resolve against the configuration directory. |
| `ExecStart` | `/opt/ufsrv/<server>/bin/<server> -C /opt/ufsrv/<server> -c <server>.config.lua -i %i` | `-C` is the runtime base, `-c` the config file inside it, `-i` the instance id. |
| `LoadCredential` | `ufsrv_secret_key:/opt/ufsrv/<server>/etc/secrets/ufsrv_secret_key` | The instance secret key is handed to the process by systemd, not read from the environment. |
| `RuntimeDirectory` | `<server>-%i` | Provides `/run/<server>-%i`, owned by the service account and removed on stop; per-instance, so instances cannot collide. |
| `SyslogIdentifier` | `<server>-%i` | Tags journald/rsyslog entries with the instance. |
| `LimitCORE` | `infinity` | Cores are wanted on failure; the deployment decides where they land. |
| `SuccessExitStatus` | `143 0` | SIGTERM (128+15) is a clean stop, not a failure. |
| `WantedBy` | `<server>.target` | Instances are pulled in by the fleet target. |
| `PartOf` | `<server>.target` (commented in the template) | Opt in when `systemctl stop <server>.target` must stop the instances too. |

`After=network.target` is the only ordering constraint in the template: the
server does not require a database or cache to be up, it reports unavailability
on the console and in its log if a backend is missing.

---

## 2. Runtime basedir layout

The unit hard-codes `/opt/ufsrv/<server>` as both the binary root and the
configuration root. Two things sit outside it by design: the host-wide
`/opt/ufsrv` tree shared by every flavour on the machine (2.1), and the system
libraries and systemd credential.

```
/opt/ufsrv/<server>/
├── bin/
│   └── <server>                     the executable
├── etc/                             -C target; the process's working directory
│   ├── <server>.config.lua          -c target: the configuration (sections 6–9)
│   ├── ufsrv_console_certificate.pem  console TLS certificate
│   ├── ufsrv_console.key              console TLS private key
│   ├── secrets/
│   │   └── ufsrv_secret_key         instance secret, loaded via LoadCredential
│   └── lua/                         schema + validator (required at start-up, 6.14)
└── var/
    └── log/
        └── <server>.log             the log file (section 5)
```

| Path | Owner | Content | Replaced on upgrade |
|---|---|---|---|
| `bin/<server>` | `ufsrv:root` | executable | yes |
| `etc/<server>.config.lua` | `ufsrv:devops` | configuration | no — operator-owned |
| `etc/ufsrv_console*.{pem,key}` | `ufsrv:devops` | console TLS material | no |
| `etc/secrets/ufsrv_secret_key` | `ufsrv:root` | instance secret | no |
| `var/log/<server>.log` | `ufsrv` | log output | rotated, never replaced |

There is no pid file in the default unit: `Type=notify` means systemd learns the
PID directly. `/run/<server>-%i` exists only while the instance runs.

### 2.1 The shared `/opt/ufsrv` tree

`/opt/ufsrv/<server>` is this flavour's own runtime and the only tree the process
is configured against (`-C`). One level up, `/opt/ufsrv` is **host-wide and
shared by every ufsrv flavour installed on the machine**: it holds material that
is provisioned once and consumed by several servers, so it must not be confused
with, or written to as if it were, a per-flavour directory.

```
/opt/ufsrv/
├── etc/                            shared credentials (host-provisioned)
│   ├── oauth_jwt_signing_key.pem   OAuth token signing key
│   ├── oauth_jwt_kid               key id published beside the tokens
│   ├── gcp_service_account_ufsrv_firebase_messaging_id_rsa
│   └── gcp_service_account_unfacd_integrity_id_rsa
├── oauth/                          shared OAuth documents served to clients
│   ├── jwks.json
│   ├── oauth_auto_discovery.json
│   ├── oauth_resource_protected_api.json
│   ├── oauth_resource_protected_mcp.json
│   └── oauth_login.html
├── <server>/                       this flavour's runtime (section 2)
└── <other-server>/                 another flavour's runtime, untouched by this one
```

| Path | Owner / mode | Content | Consumed by |
|---|---|---|---|
| `etc/oauth_jwt_signing_key.pem` | `ufsrv:devops` `0600` | token signing key | the flavour that issues tokens, referenced by absolute path from its config |
| `etc/oauth_jwt_kid` | `ufsrv:devops` `0644` | key id matching that key | same |
| `etc/gcp_service_account_*_id_rsa` | `ufsrv:devops` `0600` | Google service-account keys | flavours that talk to Google services |
| `oauth/*.json`, `oauth/oauth_login.html` | `ufsrv:devops` `0644` | discovery, protected-resource, JWKS and login documents | the HTTP flavour serving them, referenced through the file loader |

Rules for operators:

- The shared tree is provisioned by the deployment, once per host. Upgrading or
  restarting one flavour must not rewrite it, and a flavour must never place its
  own state there.
- Its files are referenced by **absolute path** from each flavour's configuration
  (for example `oauth.jwt_signing_key` and the file loader's `files_registry`),
  not by a path relative to `-C` — so the two trees are independent: the
  configuration directory can be changed or replaced without moving them, and
  vice versa.
- Because it is shared, a mistake in it affects every flavour on that host. Key
  rotation means replacing the file and restarting every consumer; the
  cross-check is that each consumer's resolved configuration names the same path
  (`/internals/config/runtime`, section 8).
- A flavour's runtime is self-contained otherwise: nothing outside
  `/opt/ufsrv/<server>` and `/opt/ufsrv` is read, apart from the shared libraries
  and the systemd credential.

---

## 3. Instances, start-up and operator procedures

### 3.1 Start-up sequence (happy path)

```
 systemd
   │  ExecStart: <server> -C /opt/ufsrv/<server> -c <server>.config.lua -i %i
   │  LoadCredential: etc/secrets/ufsrv_secret_key
   ▼
┌──────────────────────────────────────────────────────────────────────────┐
│ 1  config store: built-in defaults                                       │
│    compiled catalogue — every key has a value before anything is read    │
└──────────────────────────────────────────────────────────────────────────┘
   │
   ▼
┌──────────────────────────────────────────────────────────────────────────┐
│ 2  argv parsed                                                           │
│    -h / -v ─────────────────────────────────────► print, exit 0          │
│    -C <dir>  → config dir + path of the instance secret                  │
│    -c <file> → configuration file name                                   │
│    (these two must land first: they tell the process WHAT to read)       │
└──────────────────────────────────────────────────────────────────────────┘
   │
   ▼
┌──────────────────────────────────────────────────────────────────────────┐
│ 3  runtime identity                                                      │
│    time, server class, random server id, descriptive name                │
│    syslog banner  +  linked-library versions   ← first audit trail       │
└──────────────────────────────────────────────────────────────────────────┘
   │
   ▼
┌──────────────────────────────────────────────────────────────────────────┐
│ 4  configuration file validated                                          │
│    <config-dir>/etc/lua/{core_schema_ufsrv, core_schema_validator,       │
│                         adjunct_schema_<flavour>}.lua                    │
│    ✗ invalid ────────────────────────────────────► syslog + exit 1       │
└──────────────────────────────────────────────────────────────────────────┘
   │
   ▼
┌──────────────────────────────────────────────────────────────────────────┐
│ 5  file values transferred into the store  (defaults are now overlaid)   │
│    scopes applied in order: ufsrv → ufnet → <flavour>                    │
└──────────────────────────────────────────────────────────────────────────┘
   │
   ▼
┌──────────────────────────────────────────────────────────────────────────┐
│ 6  final CLI overrides                                                   │
│    -i <id> → instance identity      -m <mode> → daemon|terminal          │
└──────────────────────────────────────────────────────────────────────────┘
   │
   ▼
┌──────────────────────────────────────────────────────────────────────────┐
│ 7  logger opened          ✗ fails ───────────────► exit 1                │
│    file loader, application config finalisation, sensitive material      │
│    decrypted with the instance secret (envelopes are opened here)        │
│    --dump-config-json ───────────────────────────► write JSON, exit 0    │
└──────────────────────────────────────────────────────────────────────────┘
   │
   ▼
┌──────────────────────────────────────────────────────────────────────────┐
│ 8  process wired                                                         │
│    signal handlers → daemonise (only with -m daemon) → protocols         │
│    console handlers registered, pid recorded, sd_notify STATUS=Starting  │
└──────────────────────────────────────────────────────────────────────────┘
   │
   ├──► command console listening (default 127.0.0.1:19700)  ◄── reachable
   │
   ▼
┌──────────────────────────────────────────────────────────────────────────┐
│ 9  main listener                                                         │
│    server_run_mode = normal → bind main_listener_port, then drop         │
│    privileges (sys_runtime.run_as_user) and serve                        │
│    server_run_mode = shadow → listener NOT opened, logged as such        │
└──────────────────────────────────────────────────────────────────────────┘
   │
   ▼
┌──────────────────────────────────────────────────────────────────────────┐
│ 10 READY=1 to systemd, main thread joins the worker pools                │
│    unit becomes active; a failure before this point leaves it failed     │
└──────────────────────────────────────────────────────────────────────────┘
```

### 3.2 Configuration precedence

Three sources, lowest to highest:

| Order | Source | Covers | Where it is applied |
|---|---|---|---|
| 1 | compiled defaults | every key | before argv is touched |
| 2 | configuration file | scopes `ufsrv` → `ufnet` → `<flavour>` | after validation, over the defaults |
| 3 | command line | `-i` instance id, `-m` process mode | last, over the file |

Two CLI values sit outside that ladder by necessity: `-C` and `-c` select *which*
file is read, so they are applied before it and cannot be overridden by it. They
are also the only CLI settings whose effect is visible in the config store during
validation — which is why a wrong `-c` shows up as "config file not found" rather
than as a value conflict.

Consequences worth remembering:

- A value absent from the file is not "unset" — the compiled default applies, and
  that default is what `GET /internals/config/runtime` reports.
- Editing the file never changes `-i`: instance identity is CLI-only (6.1).
- `-m` selects the process mode (`daemon` forks, `terminal` stays in the
  foreground, and without it the server does not fork at all — the default mode
  is neither). It has no bearing on whether the main listener opens: that is
  `server_run_mode`.

### 3.3 Instances and operator procedures

Every instance is an independent unit: its own process, cgroup, runtime
directory, journal identifier and copy of the configuration. Instances differ
only by `-i` (and, if the operator chooses, by a per-instance config file — the
template shows the commented form).

```bash
# one instance
systemctl start   <server>@0
systemctl stop    <server>@0
systemctl restart <server>@0
systemctl status  <server>@0

# the fleet
systemctl start   <server>.target
systemctl stop    <server>.target
systemctl enable  <server>@0 <server>@1    # start at boot
systemctl enable  <server>.target          # …or put the fleet in the boot set

# logs
journalctl -u <server>@0 -f
journalctl -u <server>@0 --since '10 min ago'
```

Shutdown is a SIGTERM: the server closes its listener, finishes in-flight work,
releases backends and exits, which systemd records as success (`143`). A stop
that takes noticeably longer than a second or two means a backend is not
answering.

Configuration changes require a restart of the affected instance; the
configuration is read once at start-up. The console's `--config verify` mode
(section 4) is the way to check a file before restarting.

---

## 4. Command-line switches

Parsed by the shared core, so the set is identical across the family. The
defaults shown are the core defaults; the unit overrides `-C`, `-c` and `-i`.

| Switch | Argument | Default | Effect |
|---|---|---|---|
| `-h`, `--help` | — | — | Print usage and exit. |
| `-v`, `--version` | — | — | Print the binary name and its version, exit. |
| `-c`, `--config-file` | `<file>` | `ufsrv.conf.lua` | Configuration file, resolved relative to `--config-dir`. |
| `-C`, `--config-dir` | `<dir>` | `/opt/ufsrv/etc` | Configuration base directory; also the working directory in the unit. |
| `-m`, `--server-mode` | `daemon` \| `terminal` | `daemon` | Background or foreground; systemd uses the default (it supervises regardless). |
| `-i`, `--instance-id` | `<id>` | none | Instance identity, passed through to the unit's `%i`. |
| `--sandbox-protocol` | — | off | Sandbox protocol mode; development use. |
| `--config` | `verify` \| `verify-strict` \| `generate` | — | Configuration operation, then exit: validate once, validate and fail on unknown keys, or emit a generated file. |
| `--dump-config-json[=<path>]` | optional path | `<prog>_config_dump.json` | Dump the fully resolved configuration as JSON, then exit. |
| `--cpu-affinity-verify` | — | — | Report the CPU affinity layout and the recommended policy, then exit. |

Operational notes:

- The `--config` operations are declared but **not implemented in current
  builds** — the binary answers `config op 'verify': not yet implemented` and
  exits. Pre-flight validation therefore happens in the start-up path, not
  through this switch.
- `--dump-config-json` shows what the server actually resolved — defaults
  included — and is the quickest way to prove which value an instance is using.
- Unknown switches are collected and reported rather than silently dropped.

---

## 5. Logging

The default logger destination is a file inside the instance's runtime tree:

| Property | Value |
|---|---|
| Path | `/opt/ufsrv/<server>/var/log/<server>.log` |
| Level | `INFO` by default |
| Format | `%d(%F %T) %-8V [%c] %f:%L %m%n` — timestamp, level, component, source file and line, message |
| Rotation | 64 MiB per file, 8 archives retained |
| Driver | `file` (configuration `ufsrv.logger.driver`) |
| Prefix | `<server>` (configuration `ufsrv.logger.prefix`) |

In current deployments the sink and prefix are built-in defaults: the deployed
configuration file carries no `logger` table, so these are changed by rebuilding,
not by editing the file.

The logger also writes to syslog/journald at start-up: the banner line naming the
server, its version, instance id, server id and compilation stamp is emitted
immediately, followed by one line carrying the versions of the linked ufsrv
libraries (section 8). Both are useful for proving *which* build an instance is
actually running.

Rotation is internal to the logger process: no logrotate rule is required, and
the active file is never renamed under the writer. To read history, take the
numbered archives beside the active file.

```bash
tail -f /opt/ufsrv/<server>/var/log/<server>.log
grep -E 'ERROR|FATAL' /opt/ufsrv/<server>/var/log/<server>.log
journalctl -u <server>@0 --since today | grep '::::::::::'
```

Increasing the level is a configuration change (`logger` table, section 6),
followed by a restart.

---

## 6. Configuration file

One Lua file, loaded from `--config-dir` + `--config-file`. It returns a table
with up to three namespaces, in inheritance order:

| Namespace | Tier | Meaning |
|---|---|---|
| `ufsrv` | 1 | Every server derived from the ufsrv base. |
| `ufnet` | 2 | Servers participating in the unfacd messaging network (backends, queues, GeoIP). |
| `<server>` | 3 | This flavour only — see the Flavour section. |

The file is validated against the merged schema at load: the core schemas shipped
with the base library plus the flavour's own adjunct schema. Values are typed and
range-checked (ports, sizes, minimum thread counts), so a misconfiguration is
caught at start-up rather than at first use. Findings go through the server's
configuration validator and are logged with the rest of the start-up output.

Two conventions apply to every secret in the file:

- Values prefixed `enc:v1:` are encrypted envelopes, decrypted at the point of
  use with the instance secret from `etc/secrets/ufsrv_secret_key`. An instance
  whose credential is missing cannot start.
- Plaintext secrets are accepted but are visible to anyone who can read the
  configuration file; the `enc:v1` form is the intended operational state.

### 6.1 `ufsrv` — identity and listeners

| Key | Type | Default | Meaning |
|---|---|---|---|
| `server_run_mode` | string | `normal` | `normal` serves clients; `shadow` keeps the process and console alive but closes the main listener. |
| `server_cpu_affinity` | string | `system_override` | `system_override` uses the deployed policy, `managed` lets the server pin its own threads, `user_override` honours an explicit pin set. |
| `ufsrv_geogroup` | integer | required | Geographic group used for routing and naming. |
| `main_listener_port` | integer | required | The client-facing port. |
| `main_listener_address` | string | `127.0.0.1` | Bind address for the main listener. |
| `main_listener_protocol_id` | integer | `1` | Protocol identifier the main listener speaks. |
| `command_console_port` | integer | `19700` | Admin console port (section 8). |
| `command_console_address` | string | `127.0.0.1` | Console bind address. Bind the loopback or a private interface; the console is not a public endpoint. |

Instance identity is **not** configured in this file: it comes from the `-i`
switch (the unit's `%i`), and the banner reports that value.

### 6.2 `ufsrv.session_workers_thread_pool`, `ufsrv_workers_thread_pool`, `listener_workers_thread_pool`

| Key | Type | Default | Meaning |
|---|---|---|---|
| `session_workers_thread_pool` | integer | `2` | Worker threads servicing established sessions. |
| `ufsrv_workers_thread_pool` | integer | `2` | Worker threads for server-wide tasks (timers, delegators). |
| `listener_workers_thread_pool` | integer | `1` | Threads accepting and provisioning new connections. |

These are the primary capacity control. They are sized at start-up and are not
resizable at runtime.

### 6.3 `ufsrv.ssl_command_console` — admin console TLS

| Key | Type | Default | Meaning |
|---|---|---|---|
| `certificate_file` | string | `ufsrv_console_certificate.pem` | Server certificate, relative to `--config-dir`. |
| `key_file` | string | `ufsrv_console.key` | Private key for that certificate. |
| `is_enabled` | boolean | `false` | Whether the console listener is TLS. Disabling it puts the admin console on plain TCP — not recommended. |
| `is_client_certificate_required` | boolean | `false` | Reject console clients that present no certificate. |
| `allow_list_god` | array | `{}` | Certificate SHA1 fingerprints allowed the god role (full console). |
| `allow_list_browser` | array | `{}` | Fingerprints allowed the reduced browser role. |
| `deny_list` | array | `{}` | Fingerprints refused regardless of the allow lists. |

### 6.4 `ufsrv.ssl_command_console_client` — outbound console client

| Key | Type | Default | Meaning |
|---|---|---|---|
| `certificate_file` | string | `ufsrv_console_client_certificate_key.pem` | Client certificate the server presents when it connects *to* a console. |
| `key_file` | string | `ufsrv_console_client_key.key` | Key for that certificate. |
| `is_enabled` | boolean | `false` | Whether outbound console connections are made over TLS. |

### 6.5 `ufsrv.stats_backend` / `ufsrv.instrumentation_backend`

| Key | Type | Default | Meaning |
|---|---|---|---|
| `address` | string | `127.0.0.1` | StatsD endpoint address. |
| `port` | integer | `8125` | StatsD endpoint port. |

Both names appear in the shipped files; `stats_backend` is the schema name and
the one the server reads. Metrics are fire-and-forget UDP: an unreachable
collector costs nothing operationally.

### 6.6 `ufsrv.user_timeouts` — session lifetimes, seconds

| Key | Type | Default | Meaning |
|---|---|---|---|
| `unauthenticated_timeout` | integer | `60` | How long a connection may stay unauthenticated before it is dropped. |
| `connected_timeout` | integer | `120` | Idle limit for an authenticated session. |
| `suspended_timeout` | integer | `60` | Limit for a suspended session (`0` disables). |
| `locationless_timeout` | integer | `300` | Limit for a session that has not reported a location (`0` disables). |

### 6.7 `ufsrv.buffer_sizes` — I/O buffers, bytes

| Key | Type | Default | Meaning |
|---|---|---|---|
| `incoming_buffer_size` | integer | `1024` | Initial read buffer per connection. |
| `incoming_buffer_maxsize` | integer | `1024000` | Cap before a message is refused; raise for large frames. |
| `outgoing_buffer_size` | integer | `1024` | Initial write buffer. |
| `holding_buffer_size` | integer | `1024` | Buffer used while a message is held pending. |

### 6.8 `ufsrv.memory_specs_for_session` — session pool

| Key | Type | Default | Meaning |
|---|---|---|---|
| `sessions_per_allocation_group` | integer | `1024` | Sessions per preallocated group. |
| `allocation_groups` | integer | `10` | Groups preallocated at start-up. |
| `allocation_threshold_trigger` | string | `10%` | Free-capacity watermark that triggers another group. |

### 6.9 `ufsrv.fileloader` — concurrent file registry

| Key | Type | Default | Meaning |
|---|---|---|---|
| `is_enabled` | boolean | `false` | Build and start the file loader. |
| `registry_size` | integer | `4096` | Registered files tracked. |
| `registry_hot_loaded_size` | integer | `64` | Entries kept hot in memory. |
| `black_listed_files_registry` | array | `{}` | Paths the loader must never serve. |
| `files_registry` | array | `{}` | Files the registry is seeded with at start-up; present in the deployed WebSocket-server configuration. |

The deployed files set `registry_size = 5`; the effective value is the one in the
file, and the values above apply only where a key is absent.

### 6.10 `ufsrv.sys_runtime` — privilege drop

| Key | Type | Default | Meaning |
|---|---|---|---|
| `run_as_user` | string | empty | If set, the server drops to this account after binding privileged ports. Empty means “stay as the unit's user”. |
| `chroot` | string | `/` | Filesystem root after start-up. Keep `/` unless the whole runtime tree is mirrored inside the jail. |

### 6.11 `ufsrv.ratelimit`

| Key | Type | Default | Meaning |
|---|---|---|---|
| `is_enabled` | boolean | `false` | Enables request rate limiting. |
| `is_bypass_private_ip` | boolean | `false` | Exempts private/loopback peers from the limits; intended for trusted internal callers only. |

### 6.12 `ufsrv.email` — outbound mail (Mailgun-compatible API)

| Key | Type | Default | Meaning |
|---|---|---|---|
| `api_key` | string | empty | Provider API key; normally an `enc:v1` envelope. |
| `api_url` | string | empty | Full messages endpoint of the relay (host-run: `127.0.0.1`; containerised: the bridge gateway). |

Used for identity-verification and notification mail. With both values empty the
server still runs; mail-dependent flows fail at the point of use.

### 6.13 `ufnet` — messaging-network backends

| Key | Type | Default | Meaning |
|---|---|---|---|
| `geoip_service.address` / `.port` | string / integer | `127.0.0.1` / `19801` | GeoIP lookup service. |
| `msgqueue.is_enabled` | boolean | `false` | Inter/intra-instance message queue. |
| `msgqueue.address` / `.port` | string / integer | `ufsrvmsgqueue.unfacd.com` / `6380` | Queue endpoint. |
| `db_backend.address` / `.port` | string / integer | required | MariaDB endpoint. |
| `db_backend.username` / `.password` | string | required | Credentials; password may be an `enc:v1` envelope. |
| `persistence_backend.address` / `.port` | string / integer | `127.0.0.1` / `19705` | Redis persistence endpoint. |
| `persistence_backend.mode` / `.timeout` | string / integer | `tcp` / `500000` | Connection mode and timeout (microseconds). |
| `cache_backend_usrmsg.address` / `.port` | string / integer | `127.0.0.1` / `22001` | User-message cache endpoint. |
| `cache_backend_usrmsg.mode` / `.timeout` | string / integer | `tcp` / `500000` | As above. |
| `cache_backend_fence.address` / `.port` | string / integer | `127.0.0.1` / `32001` | Fence cache endpoint. |
| `cache_backend_fence.mode` / `.timeout` | string / integer | `tcp` / `500000` | As above. |

Backends are contacted lazily: an instance starts with any of them down and
reports failures per operation. `db_backend` has no defaults — the three values
are mandatory.

### 6.14 Lua schema and validation assets

The configuration is validated by Lua scripts, not by C code, so three files must
be present in the configuration tree before the server can start:

| File | Role | Required |
|---|---|---|
| `core_schema_ufsrv.lua` | Core schema: the `ufsrv` and `ufnet` scopes, their types and defaults. | yes |
| `core_schema_validator.lua` | The validator harness the server runs; loads the config plus both schemas. | yes |
| `adjunct_schema_<flavour>.lua` | This flavour's own scope (named in the Flavour section). | yes for the flavour |

They live in the configuration directory, one level below it:

```
<config-dir>/etc/lua/
├── core_schema_ufsrv.lua
├── core_schema_validator.lua
└── adjunct_schema_<flavour>.lua
```

With the unit's `-C /opt/ufsrv/<server>`, that is
`/opt/ufsrv/<server>/etc/lua/`. The core pair ships with the base library and the
adjunct file with the application; the deployment copies them there, so they are
replaced on upgrade and must not be edited in place.

Start-up order is: read the configuration file, run the validator against the
core and adjunct schemas, and only then transfer values into the config store.
A validation failure is fatal:

```
Config validation failed with error(s) — exiting
```

The process exits non-zero and systemd reports the unit as failed. The two
syslog lines just before the failure name the paths the server actually used —
`config file`, `core_schema path`, `adjunct_schema path`, `schema_validator path`
— which is the quickest way to spot a missing or misnamed file after an upgrade
or a hand-built runtime tree.

Because the schemas are code, not configuration, they are versioned with the
server: a schema from a different release may accept keys this binary ignores or
reject keys it needs. Keep the three files in step with the binary.

---

## 7. Redis persistence backend

Four Redis instances sit behind an instance. MariaDB remains the source of
truth; these stores hold session and fence caches, the transient message
staging, and the delivery queues. Keys carry no TTL unless the table says so.

| Store | Config key | Default endpoint | Holds |
|---|---|---|---|
| Session | `ufnet.persistence_backend` | `127.0.0.1:19705` | Session records, identity lookups, membership indexes, share lists, cluster counters |
| Fence | `ufnet.cache_backend_fence` | `127.0.0.1:32001` | Fence records and indexes, permissions, staging queue and delivery log |
| User-message | `ufnet.cache_backend_usrmsg` | `127.0.0.1:22001` | Staged outbound messages, intra-command staging, per-user delivery queue and attempt log, staging locks |
| Message queue | `ufnet.msgqueue` | `ufsrvmsgqueue.unfacd.com:6380` | Pub/sub only; no retained keys |

### 7.1 Session store

`redis-cli -p 19705`

| Key | Type | Written by | Read |
|---|---|---|---|
| `UID:<uid>` | hash | session record write on sign-on, status and location changes | `HGETALL UID:664` |
| `COOKIE:<cookie>` | string `<cid>:<uid>` | sign-on | `GET COOKIE:<cookie>` |
| `USERNAME_TO_USERID` | hash `username → uid` | account creation, rename | `HGET USERNAME_TO_USERID <username>` |
| `_NICKNAMES_DIRECTORY` | hash `nickname → uid` | nickname set | `HGET _NICKNAMES_DIRECTORY <nickname>` |
| `PROFILES_DIRECTORY`, `USERNAMES_TOKENS_DIRECTORY` | hash | account directory sync | `HKEYS PROFILES_DIRECTORY` |
| `UF:<uid>` | zset, member `<fid>:<fence_type>` | fence join/leave | `ZRANGE UF:664 0 -1` |
| `MEMBER_USERS_FOR_FENCE:<fid>` | zset, member `<uid>` | fence membership sync | `ZRANGE MEMBER_USERS_FOR_FENCE:<fid> 0 -1` |
| `SHL_<type>:<uid>` | zset, member `<related_uid>` | share/blocked list changes | `ZRANGE SHL_5:664 0 -1` |
| `SHDL_<uid>` | zset, member `<related_uid>:<type>` | reverse share index | `ZRANGE SHDL_664 0 -1` |
| `GUARDIAN_FOR_<uid>` | set of originator uids, keyed by guardian | guardian link | `SMEMBERS GUARDIAN_FOR_<uid>` |
| `USER_PREFS_ONOFF:<uid>` | string bitmap | user preference toggle | `GETRANGE USER_PREFS_ONOFF:664 0 -1` |
| `USERS_GEO` | geoset, member `<uid>:<device>:` | location update | `GEOPOS USERS_GEO <uid>:0:` |
| `PENDINGACCT_COOKIE:<cookie>` | string, with EX | pending registration | `GET PENDINGACCT_COOKIE:<cookie>` |
| `_CONFIG_UFSRV_<class>_<geogroup>` | hash `reqid` | INTRA request allocation | `HGET _CONFIG_UFSRV_ufsrvwebsock_3 reqid` |
| `_CONFIG_UFSRV_MEMBERS_<class>_<geogroup>` | set of server ids | cluster membership | `SMEMBERS _CONFIG_UFSRV_MEMBERS_ufsrvwebsock_3` |

`UID:` fields as written by the current build:

```
cid sid uid profile_key when haddress baseloc status when_serviced when_suspended
when_resumed uname cookie geogroup events_counter nickname geofid_current
geofid_past location_byu location_bys home_baseloc avatar guardian_uid
```

`events_counter` is the per-user event sequence. It is incremented in place
(`HINCRBY UID:<uid> events_counter 1`) by `GenerateUserEventId()`, which is
called for network activity that answers a client: on receipt read/viewed and
delivery receipts **both** the sender and the originator advance their counter.
Only the recipient's new value is used as an identity (it keys the
`USER_OUT_QUEUE:<uid>:<eid>` record and rides the envelope); the sender's value
is reserved for the echo reply, which is not sent yet.

### 7.2 Fence store

`redis-cli -p 32001`

| Key | Type | Written by | Read |
|---|---|---|---|
| `BID:<fid>` | hash, one field per fence attribute | fence create/update, event counter | `HGETALL BID:<fid>` |
| `FENCES_MAIN` | zset, member `<name>:<fid>` | fence create/remove | `ZRANGE FENCES_MAIN 0 -1` |
| `FENCES_NAMEINDEX` | zset, member `<folded_name>:<fid>:<cname>:<lng>:<lat>` | fence create/rename | `ZRANGEBYLEX FENCES_NAMEINDEX [<prefix> [<prefix>\xff` |
| `FENCES_GEO` | geoset, member `<fid>:<type>:` | fence location set | `GEORADIUS FENCES_GEO <lng> <lat> <km> km` |
| `<fid>:<permission>` | set of uids | permission grant/revoke | `SMEMBERS <fid>:<permission>` |
| `FENCE_OUT_QUEUE:<fid>:<eid>` | list of pending delivery records | message staged for fan-out | `LRANGE FENCE_OUT_QUEUE:<fid>:<eid> 0 -1` |
| `FENCE_OUT_QUEUE_LOG:<fid>:<eid>` | list of attempt outcomes, TTL 7 days | every drain attempt | `LRANGE FENCE_OUT_QUEUE_LOG:<fid>:<eid> 0 -1` |

`BID:` fields:

```
id type when uid baseloc cname dname bname lng lat maxusers ttl event_counter
avatar expiry list_semantics fence_key
```

#### 7.2.1 Delivery queue record

One record per accepted fence event, appended with `RPUSH` and consumed with
`LPOP`, so the list only holds events that are still pending:

```
<eid>:<gid>:<userid>:<uids_sz>:<uid>:...:<raw_sz>:<attempts>:<lzf payload>
```

`userid` is the originator (never a delivery target); the `uids` are the
staged recipients, already excluding the originator. `attempts` is 0 on the
first drain. A record is re-staged with `attempts + 1` while failed recipients
remain and fewer than three attempts have run; thereafter it is dropped.
Everything after the last `:` is compressed binary, so read it with `LLEN` and
`LRANGE` for the prefix only, or `--raw` piped to `xxd`.

#### 7.2.2 Delivery attempt log

One element per drain attempt, appended with `RPUSH`; the list expires after
seven days, refreshed on each append. This is the operational record of delivery
health and the first place to look when a client reports a missing message:

```
<attempt>:outcome:*:when:<ms>:worker:<tid>
<attempt>:outcome:<m>-<n>:rescode:<code>:when:<ms>:worker:<tid>:uid:<uid>...
```

- `*` — every staged recipient accepted the write;
- `<m>-<n>` — `m` recipients were attempted, `n` failed, and each failed uid is
  appended as `:uid:<uid>`;
- `rescode` — the attributed error. `100605` is `RESCODE_PROG_WONTLOCK` (the
  recipient's session lock was contended and the record was re-staged),
  `100003` is `RESCODE_IO_CONNECTIONCLOSED` (socket write failed).

Example:

```
$ redis-cli -p 32001 LRANGE FENCE_OUT_QUEUE_LOG:1650577838211863385:12 0 -1
0:outcome:9-2:rescode:100605:when:1794805896088:worker:137668206778048:uid:665:uid:670
1:outcome:*:when:1794805896089:worker:137668206778048
$ redis-cli -p 32001 TTL FENCE_OUT_QUEUE_LOG:1650577838211863385:12
604800
```

An event with only `0:outcome:*` was delivered first time; two elements mean one
retry; three mean the third attempt was the last.

### 7.3 User-message store

`redis-cli -p 22001`

| Key | Type | Written by | Read |
|---|---|---|---|
| `STAGED_OUTMSG:<uid>` | hash, field `<fid>:<eid>`, value `<len>:<packed message>` | message staged for a user whose socket write failed | `HGETALL STAGED_OUTMSG:<uid>` (binary: pipe through `xxd`) |
| `STAGED_OUTMSG_EVENTS:<uid>` | zset, member `<fid>:<eid>`, score is the write time in ms | same write | `ZRANGE STAGED_OUTMSG_EVENTS:<uid> 0 -1 WITHSCORES` |
| `STAGED_INTRAMSGS_<server_class>_<geogroup>` | zset, member `<reqid>:<type>:<packed command>`, score is `reqid` | INTRA command staged by the API | `ZRANGE STAGED_INTRAMSGS_<class>_<geogroup> 0 -1` |
| `STAGED_INMSGS:PL:<uid>` | string, `SET NX EX` | staging serialisation lock | `GET STAGED_INMSGS:PL:<uid>`, `TTL ...` |
| `USER_OUT_QUEUE:<uid>:<eid>` | list of pending delivery records | client-bound command (receipt, error reply) staged for a user | `LRANGE USER_OUT_QUEUE:<uid>:<eid> 0 -1` |
| `USER_OUT_QUEUE_LOG:<uid>:<eid>` | list of attempt outcomes, TTL 7 days | every user-queue drain attempt | `LRANGE USER_OUT_QUEUE_LOG:<uid>:<eid> 0 -1` |
| `SYNC_OUT_QUEUE:<uid>:<eid>` | list of pending sync records | guardian copy of a message accepted from a guarded user | `LRANGE SYNC_OUT_QUEUE:<uid>:<eid> 0 -1` |
| `SYNC_OUT_QUEUE_LOG:<uid>:<eid>` | list of attempt outcomes, TTL 7 days | every sync-queue drain attempt | `LRANGE SYNC_OUT_QUEUE_LOG:<uid>:<eid> 0 -1` |
| `STATE_OUT_QUEUE:<uid>:<eid>` | list of pending state records | state command (typing) staged for the originator's fence peers | `LRANGE STATE_OUT_QUEUE:<uid>:<eid> 0 -1` |
| `STATE_OUT_QUEUE_LOG:<uid>:<eid>` | list of attempt outcomes, TTL 7 days | every state-queue drain attempt | `LRANGE STATE_OUT_QUEUE_LOG:<uid>:<eid> 0 -1` |

`server_class` and the geogroup are the instance identity from the
configuration (this flavour reports `ufsrvwebsock` and
`ufsrv.ufsrv_geogroup`), so the staging key differs per deployed instance.

#### 7.3.1 User delivery queue record

Commands that answer one user (receipt read/viewed/delivered, and error replies
to a rejected command) are staged per user and delivered by a worker thread,
exactly like the fence queue. Appended with `RPUSH`, consumed with `LPOP`, so
the list holds only what is still pending:

```
<eid>:<fid>:<userid>:<uids_sz>:<uid>:...:<raw_sz>:<attempts>:<lzf payload>
```

- the key is `<uid>:<eid>` where `uid` is the **target user** and `eid` is that
  user's freshly allocated event id — the same value carried as the envelope's
  `eid` on the wire;
- `fid` is fence metadata; it is `0` when the command has no fence (a delivery
  receipt that omitted it, or an error reply raised before a fence was found);
- `userid` is the sender, never a target; `uids` is usually a single recipient
  and is kept as a list for multi-target fan-out;
- `attempts` is 0 on first drain; failed recipients are re-staged with
  `attempts + 1` while fewer than three attempts have run.

Everything after the last `:` is compressed binary: use `LLEN`/`LRANGE` for the
prefix, or `--raw` piped to `xxd` for the payload.

#### 7.3.2 User delivery attempt log

One element per drain attempt, same grammar and seven-day TTL as the fence log:

```
<attempt>:outcome:*:when:<ms>:worker:<tid>
<attempt>:outcome:<m>-<n>:rescode:<code>:when:<ms>:worker:<tid>:uid:<uid>...
```

- `*` — the record's target accepted the write;
- `<m>-<n>` — `m` targets attempted, `n` failed, each failed uid appended as
  `:uid:<uid>`;
- `rescode` — `100605` `RESCODE_PROG_WONTLOCK` (target session lock contended,
  record re-staged) or `100003` `RESCODE_IO_CONNECTIONCLOSED` (socket write
  failed).

Example:

```
$ redis-cli -p 22001 LRANGE USER_OUT_QUEUE_LOG:664:118 0 -1
0:outcome:1-1:rescode:100605:when:1794805896088:worker:137668206778048:uid:664
1:outcome:*:when:1794805896089:worker:137668206778048
$ redis-cli -p 22001 TTL USER_OUT_QUEUE_LOG:664:118
604800
```

A record here usually has one target, so `m` is normally `1`. The number of
elements is bounded by the retry budget only for records that share a key:
each receipt activity allocates its own eid and therefore its own key, while
error replies reuse the sender's last allocated eid, so several replies can
append to the same `USER_OUT_QUEUE_LOG` and interleave there.

#### 7.3.3 Guardian sync queue record

A message accepted from a user who has a guardian attached also produces a sync
copy for that guardian. The copy is staged once, when the message is accepted —
not once per fan-out attempt — and a worker delivers it:

```
<eid>:<userid>:<raw_sz>:<attempts>:<lzf payload>
```

- the key is `<uid>:<eid>` where `uid` is the user whose event the copy belongs
  to (the message sender) and `eid` is the event id allocated for that user at
  acceptance — the same value the copy carries in its `eid` header;
- `userid` repeats the sender; the recipient is **not** in the record: the
  guardian is read from that user's session when the record is delivered;
- no fence id is stored, because the copy has no fence identity;
- retries match the other queues: `attempts` starts at 0 and the copy is
  re-staged while fewer than three attempts have run.

Because the guardian is resolved at delivery time, removing the guardian
relation between acceptance and drain consumes the record without delivery.

Attempts are logged to `SYNC_OUT_QUEUE_LOG:<uid>:<eid>` with the same grammar
and seven-day TTL as §7.3.2, always with `m` equal to `1`:

```
$ redis-cli -p 22001 LLEN SYNC_OUT_QUEUE:664:118
$ redis-cli -p 22001 LRANGE SYNC_OUT_QUEUE_LOG:664:118 0 -1
```

#### 7.3.4 State queue record

A state command (typing) is generated for the fence peers the originator is
allowed to reach. It is staged once, as one record for the whole peer snapshot,
and a worker relays it per target:

```
<eid>:<userid>:<uids_sz>:<uid>:...:<raw_sz>:<attempts>:<lzf payload>
```

- the key is `<uid>:<eid>` where `uid` is the originator the event belongs to
  and `eid` is the event id allocated for that user when the state was accepted
  — the same value the command carries in its `eid` header;
- `userid` repeats the originator; `uids` is the peer snapshot taken at
  acceptance, with the originator itself and peers that block the originator
  already filtered out;
- no fence id is stored: the fence is part of the command inside the payload;
- retries match the other queues: `attempts` starts at 0, failed targets are
  re-staged with `attempts + 1` while fewer than three attempts have run.

Attempts are logged to `STATE_OUT_QUEUE_LOG:<uid>:<eid>` with the same grammar
and seven-day TTL as §7.3.2:

```
$ redis-cli -p 22001 LRANGE STATE_OUT_QUEUE_LOG:664:118 0 -1
```

### 7.4 Message queue

`redis-cli -p 6380`

The queue carries commands between instances; it is pub/sub, so messages are
gone once published. Channels in use:

| Channel | Carries |
|---|---|
| `UFSRV:INTRA:MSG` | message commands staged by `ufsrvapi` for `ufsrvwebsock` |
| `UFSRV:INTRA:FENCE` | fence mutations |
| `UFSRV:INTRA:USER` | user commands |
| `UFSRV:INTRA:SESSION` | session commands |
| `UFSRV:INTRA:LOC` | location updates |
| `UFSRV:INTRA:CALL` | call setup |
| `UFSRV:INTRA:RECEIPT` | receipts |
| `UFSRV:INTRA:STATE` | state changes |
| `UFSRV:INTRA:SYNC` | sync commands |
| `UFSRV:INTRA:SYS` | system commands |

Read-only inspection:

```
$ redis-cli -p 6380 PUBSUB CHANNELS 'UFSRV:*'
$ redis-cli -p 6380 SUBSCRIBE UFSRV:INTRA:MSG
$ redis-cli -p 6380 PSUBSCRIBE 'UFSRV:INTRA:*'
```

There is no backlog to query: an operator investigating a missed command uses
the server logs, the `STAGED_INTRAMSGS_*` set on the API side, and the delivery
logs in the fence store (`FENCE_OUT_QUEUE_LOG`) and the user-message store
(`USER_OUT_QUEUE_LOG`).

---

## 8. Command console

An administrative text protocol over TLS, served on
`ufsrv.command_console_address`:`ufsrv.command_console_port` (default
`127.0.0.1:19700`). Each request is one line, `METHOD path[:pipeline]`; the reply
is one JSON line, so it pipes straight into `jq`. Methods are `GET`, `PUT`,
`DEL`, and `EVAL` for a Lua expression.

The server certificate and key come from `ufsrv.ssl_command_console` (section
6.3). Access is controlled by the certificate allow/deny lists; with
`is_client_certificate_required = false` and empty allow lists the console still
requires a TLS connection but does not authenticate the caller, which is why the
listener must stay on a private interface.

```bash
# basic query
echo "GET /internals/logger" | socat - OPENSSL:127.0.0.1:19700,verify=0,nosni

# with jq, and a path expression
echo "GET /internals/config/runtime" | socat - OPENSSL:127.0.0.1:19700,verify=0,nosni | jq

# Lua, for ad-hoc inspection
echo "EVAL return 1 + 1" | socat - OPENSSL:127.0.0.1:19700,verify=0,nosni
```

`verify=0` is correct only when the certificate is not in the client's trust
store (typically the self-signed console certificate shipped with the runtime
tree); with a proper CA, drop it.

### 8.1 Node catalogue (shared base)

| Node | Returns |
|---|---|
| `/internals/version/self` | `{"name","version","compiled","by"}` — the running binary's own version. |
| `/internals/version/linked_libs` | `{"uflib","ufnetcorelib","ufsrvcorelib","ufsrvrxlib",…}` — full versions of the linked ufsrv libraries, from their own version functions. |
| `/internals/logger` | Active logger configuration and its capabilities. |
| `/internals/config/runtime` | Runtime configuration values as resolved. |
| `/internals/config/config_file` | The configuration file as loaded. |
| `/internals/threads` | Thread inventory for the process. |
| `/internals/memory/jemalloc` | jemalloc statistics for the process. |
| `/internals/hashtables/sessions` | Session hashtable occupancy. |
| `/internals/recyclers/sessions` | Session recycler state. |

Group nodes (`/internals`, `/internals/version`, `/internals/config`, …) answer
with their children when queried directly, so a single `GET /internals/version`
enumerates what exists.

The two version nodes are the operational answer to “what is actually running”:
`self` is the binary, `linked_libs` is every library folded into it at link time.
Both are also written to syslog once, immediately after the start-up banner
(section 5), so the same information can be recovered from the logs of a past
run without opening the console.

### 8.2 Console pitfalls

- The listener is bound to the configured address, not to `0.0.0.0`; a
  connection refused from `127.0.0.1` usually means the configured address is a
  specific interface address.
- An empty reply (connection accepted, no body) means the path is unknown — the
  dispatch layer writes nothing for an unresolved node. `GET /internals` shows
  the tree.
- The console is not a shell: `EVAL` is sandboxed and has no filesystem access.

---

## 9. Troubleshooting

| Symptom | Likely cause | Check |
|---|---|---|
| Unit fails immediately | configuration invalid | `journalctl -u <server>@<id> -b`, then `--config verify` |
| Process runs, listener closed | `server_run_mode = shadow` | banner line, then `/internals/config/runtime` |
| `Connection refused` on the console | wrong address/port, or instance not running | `ss -ltn` in the instance's namespace, config values |
| Console answers but a node is empty | unknown path, or an older binary | `GET /internals`; compare `self` version with the expected build |
| Log file absent | logger driver not `file`, or wrong runtime tree | `/internals/logger`, `sys_runtime` values |
| Mail or OAuth flows fail | `email`/`oauth_*` unset or envelope undecryptable | configuration, presence of `etc/secrets/ufsrv_secret_key` |
| Backend errors at runtime | `ufnet` endpoints unreachable | resolved endpoints via `/internals/config/runtime` |
| Secrets cannot be decrypted | credential not loaded by systemd | `systemctl show <server>@<id> -p LoadCredential` |

---

## 10. Flavour: `ufsrvwebsock`

This server is the WebSocket edge of the family. It terminates client WebSocket
connections, authenticates sessions and routes protocol frames to the messaging
backends. Its client-facing listener speaks WebSocket over TLS on
`ufsrv.main_listener_port` (shipped value `19701`); the command console keeps the
family default `19700`.

### 10.1 `ufsrvwebsock.websocket` — protocol limits

| Key | Type | Default | Meaning |
|---|---|---|---|
| `max_frame_size` | integer | `65536` | Largest single WebSocket frame accepted, bytes. |
| `max_message_size` | integer | `1048576` | Largest reassembled message accepted, bytes. |
| `heartbeat_interval_secs` | integer | `30` | Ping/pong interval used to detect dead peers. |
| `close_timeout_secs` | integer | `10` | Grace period for the closing handshake before the socket is dropped. |
| `max_connections_per_session` | integer | `1` | Concurrent sockets allowed per authenticated session. Raising this changes session semantics; keep at `1` unless the client needs multiplexed sockets. |

These are per-connection accounting limits, not capacity limits: capacity is
governed by the thread pools and by the backends.

### 10.2 `ufsrvwebsock.cloud_service_accounts`

| Key | Type | Default | Meaning |
|---|---|---|---|
| `gpc_firebase_messaging` | string | empty | Firebase messaging service-account document, as an `enc:v1` envelope. |
| `gpc_integrity` | string | empty | Firebase App Integrity service-account document, as an `enc:v1` envelope. |

The envelopes are decrypted at the point of use with the instance secret key; the
document inside supplies both the signing key and the key id Google files it
under. With the values empty, push and integrity verification are unavailable but
the server runs.

### 10.3 Additional console node

| Node | Returns |
|---|---|
| `/internals/recycler` | Recycler state for the WebSocket session objects. |

### 10.4 What differs from the base

- **Listener** is WebSocket-over-TLS, so `ssl_command_console` is only half the
  TLS story: client connections use the listener's own certificate configuration.
- **Config additions**: the `ufsrvwebsock` namespace (9.1, 9.2) and nothing else.
  The `ufsrv` and `ufnet` tables are the shared base exactly as described in
  section 6.
- **Schema asset**: the flavour scope is validated by
  `etc/lua/adjunct_schema_ufsrvwebsock.lua`, which must be present in the runtime
  tree (6.14).
- **Failure mode to expect first** is at the WebSocket layer, not the REST layer:
  frames over `max_frame_size`, or sessions exceeding
  `max_connections_per_session`, disconnect clients rather than returning errors.
