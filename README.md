### TODO

- [x] client argument parsing (POSIX getopt)
- [x] ipc: sockets
- [ ] daemon
  - array of thread ids and state
    - reallocation??
  - (1): `pthread_create`
    - (b): recursive basename() and check for existence
  - (2): `pthread_cancel`? or running state + polling?
  - (3): `pthread_suspend`? or mutexes?
  - (4): query state
  - task progress - query number of files in first level? all levels?
  - traversal
    - nftw
    - don't follow symlinks
- [ ] printing results in client
  - pretty printing can be complex

### Build

`$ make build`

### IPC protocol from client to daemon

- 2 bytes - `"da"` (hex: `64 61`) magic bytes
- 1 byte - command
- 8 bytes - payload length
- `<payload_length>` bytes - payload

### Payload Structure

| COMMAND     | PAYLOAD STRUCTURE                                             |
| ----------- | ------------------------------------------------------------- |
| CMD_ADD     | `1` byte - priority <br/> `<payload_length> - 1` bytes - path |
| CMD_SUSPEND | `8` bytes - id                                                |
| CMD_RESUME  | `8` bytes - id                                                |
| CMD_REMOVE  | `8` bytes - id                                                |
| CMD_INFO    | `8` bytes - id                                                |
| CMD_LIST    | `0` bytes - id                                                |
| CMD_PRINT   | `8` bytes - id                                                |

### IPC protocol from daemon to client

- `json?`
