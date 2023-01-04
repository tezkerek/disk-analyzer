#include <common/ipc.h>
#include <common/utils.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int connect_to_socket() {
    struct sockaddr_un address;
    int fd = init_socket(&address);

    if (connect(fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Socket connect");
        exit(EXIT_FAILURE);
    }

    return fd;
}

int send_ipc_msg(int serverfd, int8_t cmd, char *payload, int64_t payload_len) {
    int64_t msg_len = min_ipc_msg_len(payload_len);
    char *msg = da_malloc(msg_len * sizeof(*msg));

    build_ipc_msg(cmd, payload, payload_len, msg, msg_len);

    int written_bytes;
    if ((written_bytes = write(serverfd, msg, msg_len)) < 0) {
        free(msg);
        return -1;
    }

    free(msg);
    return written_bytes;
}

int main(int argc, char *argv[]) {
    int serverfd = connect_to_socket();
    char *text = argv[1];

    // Send command to daemon
    if (send_ipc_msg(serverfd, CMD_ADD, text, strlen(text)) < 0) {
        perror("Failed send");
        exit(EXIT_FAILURE);
    }

    // Read reply
    if (!validate_ipc_msg(serverfd)) {
        perror("Connection validation");
        exit(EXIT_FAILURE);
    }

    // Read command
    int8_t cmd;
    if (read(serverfd, &cmd, 1) < 0) {
        perror("Command");
        exit(EXIT_FAILURE);
    }

    // Read payload
    int64_t payload_len;
    if ((payload_len = read_payload_length(serverfd)) < 0) {
        perror("Payload length");
        exit(EXIT_FAILURE);
    }

    char *payload = da_malloc((payload_len + 1) * sizeof(*payload));
    if (read(serverfd, payload, payload_len) < 0) {
        perror("Payload read");
        free(payload);
        exit(EXIT_FAILURE);
    }
    payload[payload_len] = 0;

    printf("Received %s\n", payload);

    free(payload);

    close(serverfd);

    return EXIT_SUCCESS;
}
