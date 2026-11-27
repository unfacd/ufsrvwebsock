# ufsrvwebsock

Stateful WebSocket server for the unfacd network. It authenticates connections,
maintains live sessions, dispatches commands and delivers events to connected
clients. It also consumes commands staged by `ufsrvapi`.

## Modules

| Location | Main responsibilities |
|---|---|
| `src/main.c`, `src/data_protocols.h` | Register the WebSocket protocol and start the listener and Redis subscriber. |
| `src/protocol_websocket*.c`, `src/websock_parser` | Upgrade, frame parsing and encoding, protocol callbacks and connection-local state. |
| `src/session_routing` | Authentication, account and session resolution, and session resumption or takeover. |
| `src/command_parser`, `src/command_controllers` | Interpret incoming commands and apply messaging, group, user and event policy. |
| `src/command_delivery` | Stage and drain asynchronous delivery work. `MessageCommandDeliverySignal()` schedules message fan-out. |
| `src/msgqueue_broadcast`, `src/command_broadcast` | `WorkerThreadMsgQueueParserExecutor()` dispatches Redis jobs on the shared job pool, including staged INTRA commands and INTER cluster events. |

## Dependencies

- `ufsrvcorelib` supplies protocol dispatch, base sessions, listener and worker machinery, scheduling and instrumentation.
- `ufnetcorelib` supplies unfacd session state, protobuf messages, domain operations, Redis pub/sub and staged delivery storage.
- The server also links `uflib` and `ufsrvrxlib`. It uses MariaDB for durable data and Redis for caches, staged commands and the message bus.

See the [operations manual](docs/UFSRVWEBSOCK_OPERATIONS_MANUAL.md).
