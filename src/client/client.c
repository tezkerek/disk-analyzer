#include <client/arg_parse.h>
#include <common/ipc.h>
#include <common/utils.h>
#include <stdio.h>
#include <stdlib.h>
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

int send_ipc_msg(int serverfd, int8_t cmd, const struct ByteArray *payload) {
    struct ByteArray ipc_msg;

    build_ipc_msg(cmd, payload, &ipc_msg);

    int written_bytes;
    if ((written_bytes = write(serverfd, ipc_msg.bytes, ipc_msg.len)) < 0) {
        bytearray_destroy(&ipc_msg);
        return -1;
    }

    bytearray_destroy(&ipc_msg);
    return written_bytes;
}

int main(int argc, char *argv[]) {
<<<<<<< HEAD
    struct da_args args;
=======
    int serverfd = connect_to_socket();
    char *text = argv[1];
>>>>>>> create header for thread utils

    if (parse_args(argc, argv, &args) < 0) {
        fputs("Failed to parse args\n", stderr);
        fputs(DA_USAGE_HELP, stderr);
        exit(EXIT_FAILURE);
    }

    struct ByteArray payload;

    if (args.cmd == CMD_ADD) {
        // Payload is "<priority><path>"
        bytearray_init(&payload, strlen(args.path) + 1);

        payload.bytes[0] = args.priority;
        strncpy(payload.bytes + 1, args.path, payload.len - 1);
        free(args.path);
    } else if (args.cmd == CMD_LIST) {
        // No payload
        payload.len = 0;
        payload.bytes = NULL;
    } else {
        // Payload is the job id
        bytearray_init(&payload, sizeof(args.job_id));
        memcpy(payload.bytes, &args.job_id, payload.len);
    }

    int serverfd = connect_to_socket();

    // Send command to daemon
    if (send_ipc_msg(serverfd, args.cmd, &payload) < 0) {
        perror("Failed send");
        exit(EXIT_FAILURE);
    }

    bytearray_destroy(&payload);

    close(serverfd);

    return EXIT_SUCCESS;
}
