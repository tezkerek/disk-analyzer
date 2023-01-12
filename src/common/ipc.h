#ifndef IPC_HEADER
#define IPC_HEADER

#include <common/utils.h>
#include <stdint.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/disk-analyzer.socket"
#define IPC_MAGIC   "da"

#define CMD_ADD     1
#define CMD_SUSPEND 2
#define CMD_RESUME  3
#define CMD_REMOVE  4
#define CMD_INFO    5
#define CMD_LIST    6
#define CMD_PRINT   7

/**
 * Initializes the socket and the address struct.
 * Returns a file descriptor to the socket.
 */
int init_socket(struct sockaddr_un *address);

/**
 * Returns 1 if the connection is valid.
 */
int validate_ipc_msg(int fd);

/**
 * Reads a payload from the given fd.
 * Returns 0 for success or -1 for failure.
 * Allocates `bytes` if successful, remember to free it.
 */
int read_ipc_payload(int fd, struct ByteArray *payload);

/**
 * Builds a message with the given command and payload in msg.
 * Remember to call `bytearray_destroy` on msg.
 */
void build_ipc_msg(int8_t cmd,
                   const struct ByteArray *payload,
                   struct ByteArray *msg);

/**
 * A read() that exits on error.
 */
void saferead(int fd, void *buf, int count);

#endif
