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

int64_t read_payload_length(int fd) {
    int64_t payload_len;

    if (read(fd, &payload_len, sizeof(payload_len)) < 0) {
        perror("Payload length read");
        return -1;
    }

    return payload_len;
}

int64_t min_ipc_msg_len(int64_t payload_len) {
    return sizeof(IPC_MAGIC) + sizeof(int8_t) + sizeof(int64_t) + payload_len;
}

void build_ipc_msg(int8_t cmd,
                   char *payload,
                   int64_t payload_len,
                   char *msgbuf,
                   int64_t msgbuf_len) {
    char *msgptr = msgbuf;
    memcpy(msgptr, IPC_MAGIC, sizeof(IPC_MAGIC));
    msgptr += sizeof(IPC_MAGIC);
    memcpy(msgptr, &cmd, sizeof(cmd));
    msgptr += sizeof(cmd);
    memcpy(msgptr, &payload_len, sizeof(payload_len));
    msgptr += sizeof(payload_len);
    strncpy(msgptr, payload, min(payload_len, msgbuf_len));
}
