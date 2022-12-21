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

# IPC protocol

## Message structure
- 2 bytes - `"da"` (hex: `64 61`) magic bytes
- 1 byte - command
- 8 bytes - payload length
- `<payload_length>` bytes - payload

### Payload Structure

| Command       | Payload Structure                                             |
| ------------- | ------------------------------------------------------------- |
| `CMD_ADD`     | `1` byte - priority <br/> `<payload_length> - 1` bytes - path |
| `CMD_SUSPEND` | `8` bytes - id                                                |
| `CMD_RESUME`  | `8` bytes - id                                                |
| `CMD_REMOVE`  | `8` bytes - id                                                |
| `CMD_INFO`    | `8` bytes - id                                                |
| `CMD_LIST`    | `0` bytes - id                                                |
| `CMD_PRINT`   | `8` bytes - id                                                |

## Response structure

The first byte of each response is an exit code.
If the code is 0, the command succeeded.
If the code is not 0, an error occurred.

### `CMD_ADD`

**Success**:
- 8 bytes - created job ID

**Error**:
- `code 1` - directory does not exist
- `code 2` - directory already part of another job
    - 8 bytes - the existing job ID

### `CMD_SUSPEND`

**Success**: *nothing*

**Error**:
- `code 1` - job not found

### `CMD_RESUME`

**Success**: *nothing*

**Error**:
- `code 1` - job not found

### `CMD_REMOVE`

**Success**: *nothing*

**Error**:
- `code 1` - job not found

### `CMD_INFO`

**Success**:

| ID      | Priority | Path length | Path                  | Progress | Status | File count | Dir count |
|---------|----------|-------------|-----------------------|----------|--------|------------|-----------|
| 8 bytes | 1 byte   | 8 bytes     | `<path-length>` bytes | 1 byte   | 1 byte | 8 bytes    | 8 bytes   |

**Error**:
- `code 1` - job not found

### `CMD_LIST`

**Success**:
- 8 bytes - entry count
- `<entry-count>` entries like `CMD_INFO`

**Error**: *nothing*

### `CMD_PRINT`

**Success**:
- 8 bytes - entry count
- First entry is the job path. The following entries are its subdirectories.
- Entry structure:

| Path length | Path                  | Size    |
|-------------|-----------------------|---------|
| 8 bytes     | `<path-length>` bytes | 8 bytes |

**Error**:
- `code 1` - job not found
- `code 2` - job not done
    - an entry like `CMD_INFO`
