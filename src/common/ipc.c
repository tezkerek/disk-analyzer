#include <common/ipc.h>
#include <common/utils.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int init_socket(struct sockaddr_un *address) {
    int fd = socket(PF_UNIX, SOCK_STREAM, 0);

    if (fd == -1) {
        perror("Socket creation");
        exit(EXIT_FAILURE);
    }

    memset(address, 0, sizeof(*address));
    address->sun_family = AF_UNIX;
    strcpy(address->sun_path, SOCKET_PATH);

    return fd;
}

int validate_ipc_msg(int fd) {
    char prefix[sizeof(IPC_MAGIC)];

    if (read(fd, prefix, sizeof(prefix)) < 0) {
        perror("Prefix read");
        return 0;
    }

    return strcmp(prefix, IPC_MAGIC) == 0;
}

int read_ipc_payload(int fd, struct ByteArray *payload) {
    int64_t payload_len;
    if (read(fd, &payload_len, sizeof(payload_len)) < 0) {
        perror("Payload length read");
        return -1;
    }

    bytearray_init(payload, payload_len);
    if (read(fd, payload->bytes, payload->len) < 0) {
        bytearray_destroy(payload);
        return -1;
    }

    return 0;
}

void build_ipc_msg(int8_t cmd,
                   const struct ByteArray *payload,
                   struct ByteArray *msg) {
    int64_t msg_len =
        sizeof(IPC_MAGIC) + sizeof(cmd) + sizeof(payload->len) + payload->len;
    bytearray_init(msg, msg_len);

    char *msgptr = msg->bytes;

    memcpy(msgptr, IPC_MAGIC, sizeof(IPC_MAGIC));
    msgptr += sizeof(IPC_MAGIC);

    memcpy(msgptr, &cmd, sizeof(cmd));
    msgptr += sizeof(cmd);

    memcpy(msgptr, &payload->len, sizeof(payload->len));
    msgptr += sizeof(payload->len);

    strncpy(msgptr, payload->bytes, min(payload->len, msg->len));
}
