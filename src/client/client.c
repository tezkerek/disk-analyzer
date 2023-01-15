#include <client/arg_parse.h>
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

void handle_print_reply(int serverfd) {
    int64_t entry_count;
    saferead(serverfd, &entry_count, sizeof(entry_count));

    // TODO: Pretty print
    char* last_dir;
    last_dir = malloc(1);
    int64_t total_size = 0;

const char* units[] = {"B", "KB", "MB", "GB", "TB"};
const int UNIT_COUNT = sizeof(units) / sizeof(units[0]);

for (int64_t i = 0; i < entry_count; ++i) {
        int64_t path_len;
        saferead(serverfd, &path_len, sizeof(path_len));

        char *path = da_malloc((path_len + 1) * sizeof(*path));
        saferead(serverfd, path, path_len);
        // Null terminate
        path[path_len] = 0;

        int64_t dir_size;
        saferead(serverfd, &dir_size, sizeof(dir_size));

        int poz = -1;
        int len = strlen(path);
        for(int j = 0; j < len ; j++){
            if(path[j] == '/'){
                poz = j;
                break;
            }
        }
        if(poz == -1){
            poz = strlen(path);
        }
        char* dir;
        dir = malloc(poz+1);
        strncpy(dir, path, poz);
        if(strcmp(dir, last_dir) != 0){
            if(i != 0){    
                printf("|\n");
            }
            free(last_dir);
            last_dir = malloc(strlen(dir)+1);
            strncpy(last_dir, dir, strlen(dir));
        }
        double size = (double) dir_size;
        int unit_index = 0;
        while (size >= 1024 && unit_index < UNIT_COUNT - 1) {
            size /= 1024;
            unit_index++;
        }
        if(i != 0){
            printf("|-/%s %.2lf%% %.2lf %s\n", path, ((double)dir_size)/total_size*100, size, units[unit_index]);
        }
        else{
            printf("%s %.2lf%%% .2lf %s\n", path, 100.00, size, units[unit_index]);
            //total_size = dir_size;
            total_size = 100000;
        }
        free(dir);
    }
    free(last_dir);
}

void handle_info_reply(int serverfd) {
    int64_t job_id;
    saferead(serverfd, &job_id, sizeof(job_id));

    int8_t priority;
    saferead(serverfd, &priority, sizeof(priority));

    int64_t path_len;
    saferead(serverfd, &path_len, sizeof(path_len));

    char *path = da_malloc((path_len + 1) * sizeof(*path));
    saferead(serverfd, path, path_len);
    // Null terminate
    path[path_len] = 0;

    int8_t progress;
    saferead(serverfd, &progress, sizeof(progress));

    int8_t status;
    saferead(serverfd, &status, sizeof(status));

    int64_t file_count;
    saferead(serverfd, &file_count, sizeof(file_count));

    int64_t dir_count;
    saferead(serverfd, &dir_count, sizeof(dir_count));

    // TODO: print header, align columns
    char* status_string;
    status_string = malloc(12);
    if (status == JOB_STATUS_IN_PROGRESS){
        strncpy(status_string, "In progress", 12);
    }
    else if (status == JOB_STATUS_DONE){
        strncpy(status_string, "Done", 5);
    }
    else if (status == JOB_STATUS_PAUSED){
        strncpy(status_string, "Paused", 7);
    }
    else if (status == JOB_STATUS_REMOVED){
        strncpy(status_string, "Removed", 8);
    }

    printf("Id: %lu, Priority: %d %s \nstatus: %s, %lu files, %lu dirs\n",
           job_id,
           priority,
           path,
           status_string,
           file_count,
           dir_count);

    free(status_string);
}

int handle_reply(int8_t cmd, int serverfd) {
    int8_t code;
    saferead(serverfd, &code, sizeof(code));
    printf("Code %d\n", code);

    if (cmd == CMD_ADD) {
        if (code == 0) {
            int64_t job_id;
            saferead(serverfd, &job_id, sizeof(job_id));
            printf("New analysis task with ID %ld\n", job_id);
        }
    } else if (cmd == CMD_INFO) {
        if (code == 0) {
            handle_info_reply(serverfd);
        }
    } else if (cmd == CMD_PRINT) {
        if (code == 0) {
            handle_print_reply(serverfd);
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    struct da_args args;

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

    handle_reply(args.cmd, serverfd);

    bytearray_destroy(&payload);

    close(serverfd);

    return EXIT_SUCCESS;
}
