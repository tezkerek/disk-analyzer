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

    int64_t total_size = 0;

    static const char *UNITS[] = {"B", "KB", "MB", "GB", "TB"};
    const int UNIT_COUNT = sizeof(UNITS) / sizeof(UNITS[0]);

    char *path = NULL;
    size_t path_bufsize = 0;

    char *last_top_dir = NULL;
    size_t last_top_dir_bufsize = 0;

    for (int64_t i = 0; i < entry_count; ++i) {
        char *branch_symbol;
        if (i == 0) {
            branch_symbol = "";
        } else if (i == entry_count - 1) {
            // Symbol for the last entry
            branch_symbol = "└─ /";
        } else {
            // Symbol for middle entries
            branch_symbol = "├─ /";
        }

        int64_t path_len;
        saferead(serverfd, &path_len, sizeof(path_len));

        if (path_len + 1 > path_bufsize) {
            path_bufsize = path_len + 1;
            path = da_malloc(path_bufsize * sizeof(*path));
        }
        saferead(serverfd, path, path_len);
        // Null terminate
        path[path_len] = 0;

        int64_t dir_bytes;
        saferead(serverfd, &dir_bytes, sizeof(dir_bytes));

        if (i == 0) {
            total_size = dir_bytes;
        }

        // Find the top dir (first in the path)
        const char *first_path_sep = strchr(path, '/');
        size_t top_dir_len;
        if (first_path_sep == NULL) {
            // The entire path is the dir
            top_dir_len = path_len;
        } else {
            top_dir_len = first_path_sep - path;
        }

        if (last_top_dir == NULL || strncmp(path, last_top_dir, top_dir_len) != 0) {
            // Top dir changed
            if (i > 0) {
                // Output a newline when the top dir changes
                puts("│");
            }

            // Realloc last_top_dir if needed
            if (top_dir_len + 1 > last_top_dir_bufsize) {
                free(last_top_dir);
                last_top_dir_bufsize = top_dir_len + 1;
                last_top_dir =
                    da_malloc(last_top_dir_bufsize * sizeof(*last_top_dir));
            }

            // Update last_dir
            strncpy(last_top_dir, path, top_dir_len);
        }

        // Compute human size and the unit
        double human_size = (double)dir_bytes;
        int32_t unit_index = 0;
        while (human_size >= 1024 && unit_index < UNIT_COUNT - 1) {
            human_size /= 1024;
            unit_index++;
        }

        // Compute percentage and the bar width
        double ratio = ((double)dir_bytes) / total_size;
        double percentage = ratio * 100;
        int bar_width = ratio * 40;

        // Print line
        printf("%s%s %.2lf%% %.2lf %s ",
               branch_symbol,
               path,
               percentage,
               human_size,
               UNITS[unit_index]);
        fputs_repeated("#", stdout, bar_width);
        fputs("\n", stdout);
    }

    free(path);
    free(last_top_dir);
}

void handle_info_reply(int serverfd, int print_heading) {
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
    const char *status_string;
    if (status == JOB_STATUS_IN_PROGRESS) {
        status_string = "In progress";
    } else if (status == JOB_STATUS_DONE) {
        status_string = "Done";
    } else if (status == JOB_STATUS_PAUSED) {
        status_string = "Paused";
    } else if (status == JOB_STATUS_REMOVED) {
        status_string = "Removed";
    }

    if (print_heading) {
        printf("%-3s %-3s  %-*s %-9s %-*s  %s\n",
               "ID",
               "Pri",
               (int)path_len,
               "Path",
               "Progress",
               (int)strlen(status_string),
               "Status",
               "Details");
    }
    printf("%-3lu %-3d %s  %d%%       %-6s  %lu files, %lu dirs\n",
           job_id,
           priority,
           path,
           progress,
           status_string,
           file_count,
           dir_count);
}

void handle_list_reply(int serverfd) {
    int64_t entry_count;
    saferead(serverfd, &entry_count, sizeof(entry_count));

    int print_heading = 1;
    for (int64_t i = 0; i < entry_count; ++i) {
        handle_info_reply(serverfd, print_heading);
        print_heading = 0;
    }
}

int handle_reply(int8_t cmd, int serverfd) {
    int8_t code;
    saferead(serverfd, &code, sizeof(code));

    if (cmd == CMD_ADD) {
        if (code == 0) {
            int64_t job_id;
            saferead(serverfd, &job_id, sizeof(job_id));
            printf("New analysis task with ID %ld\n", job_id);
        } else if (code == 1) {
            fputs("Path does not exist\n", stderr);
            return -1;
        } else if (code == 2) {
            int64_t job_id;
            saferead(serverfd, &job_id, sizeof(job_id));
            fprintf(stderr,
                    "Path already being analyzed by job with ID %lu\n",
                    job_id);
            return -1;
        }
    } else if (cmd == CMD_PRINT) {
        if (code == 0) {
            handle_print_reply(serverfd);
        } else if (code == 1) {
            fputs("Job not found\n", stderr);
        } else if (code == 2) {
            fputs("Job still in progress\n", stderr);
        }
    } else if (cmd == CMD_LIST) {
        if (code == 0) {
            handle_list_reply(serverfd);
        } else {
            fputs("Unknown error\n", stderr);
        }
    } else {
        if (code == 1) {
            fputs("Job not found\n", stderr);
            return -1;
        } else if (code > 1) {
            fputs("Unknown error\n", stderr);
            return -1;
        }

        if (cmd == CMD_INFO) {
            handle_info_reply(serverfd, 1);
        } else if (cmd == CMD_SUSPEND) {
            fputs("Job paused\n", stdout);
        } else if (cmd == CMD_RESUME) {
            fputs("Job resumed\n", stdout);
        } else if (cmd == CMD_REMOVE) {
            fputs("Job removed\n", stdout);
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
