#ifndef IPC_HEADER
#define IPC_HEADER

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
 * Returns the length of the payload or -1 for failure.
 */
int64_t read_payload_length(int fd);

/**
 * Returns the required IPC message buffer size for a payload of the given
 * length.
 */
int64_t min_ipc_msg_len(int64_t payload_len);

/**
 * Builds a message with the given command and payload in msgbuf.
 * The size of msgbuf must be at least sizeof_ipc_msg(payload_len).
 */
void build_ipc_msg(int8_t cmd,
                   char *payload,
                   int64_t payload_len,
                   char *msgbuf,
                   int64_t msgbuf_len);

#endif
