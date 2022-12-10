#include <common/ipc.h>
#include <common/utils.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include "client/arg_parse.h"

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

struct return_struct *args_struct;

int main(int argc, char *argv[]) {
    int serverfd = connect_to_socket();

    char *text = argv[1];
    printf("petrik\n");
    // Send command to daemon
    // if (send_ipc_msg(serverfd, CMD_ADD, text, strlen(text)) < 0) {
    //     perror("Failed send");
    //     exit(EXIT_FAILURE);
    // }

    // Read reply
    // if (!validate_ipc_msg(serverfd)) {
    //     perror("Connection validation");
    //     exit(EXIT_FAILURE);
    // }

    // Read command
    args_struct = get_args(argc, argv);
    int8_t cmd = args_struct->cmd;
    // if (read(serverfd, &cmd, 1) < 0) {
    //     perror("Command");
    //     exit(EXIT_FAILURE);
    // }

    // Read payload
    int64_t payload_len = args_struct->payload_len;
    // if ((payload_len = read_payload_length(serverfd)) < 0) {
    //     perror("Payload length");
    //     exit(EXIT_FAILURE);
    // }

    // char *payload = da_malloc((payload_len + 1) * sizeof(*payload));
    // if (read(serverfd, payload, payload_len) < 0) {
    //     perror("Payload read");
    //     free(payload);
    //     exit(EXIT_FAILURE);
    // }
    char *payload;
    payload = malloc(payload_len+1);
    strcpy(payload, args_struct->payload);
    payload[payload_len] = 0;
    printf("%d, %ld, %s", cmd, payload_len, payload);

    printf("Received %s\n", payload);

    free(payload);

    close(serverfd);

    return EXIT_SUCCESS;
}
