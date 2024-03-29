#include "job.h"
#include <common/ipc.h>
#include <common/utils.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define MAX_THREADS  100
#define MAX_CHILDREN 100

static uint64_t job_count = 0;

struct Job *jobs[MAX_THREADS];

/**
 * Finds the job associated with the given id.
 * Returns NULL on error.
 */
struct Job *find_job_by_id(int64_t id) {
    if (id >= 0 && id < job_count)
        return jobs[id];
    return NULL;
}

int bind_socket() {
    struct sockaddr_un address;
    int fd = init_socket(&address);

    unlink(SOCKET_PATH);
    if (bind(fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Socket bind");
        exit(EXIT_FAILURE);
    }

    if (listen(fd, 1) < 0) {
        perror("Listen");
        exit(EXIT_FAILURE);
    }

    return fd;
}

int8_t get_job_results(struct Job *job, struct ByteArray *result) {
    if (job->status != JOB_STATUS_DONE) {
        return -2;
    }

    int8_t exit_code = 0;
    int64_t entry_count = job->total_subdir_count + 1;

    // Calculate necessary space for the serialization
    size_t serialized_len =
        sizeof(exit_code) + sizeof(entry_count) + job->total_path_len +
        entry_count * (sizeof(int64_t) + sizeof(job->root->bytes));

    bytearray_init(result, serialized_len);

    char *ptr = result->bytes;
    memcpy(ptr, &exit_code, sizeof(exit_code));
    ptr += sizeof(exit_code);

    memcpy(ptr, &entry_count, sizeof(entry_count));
    ptr += sizeof(entry_count);

    directory_serialize(job->root, ptr);

    return 0;
}

int get_job_info(struct Job *job, struct ByteArray *result) {
    int64_t payload_len = sizeof(/* exit code */ int8_t) +
                          sizeof(/* job id */ int64_t) + job_serialized_size(job);

    bytearray_init(result, payload_len);

    // Leave space for the error code and job id
    char *buf = result->bytes + sizeof(int8_t) + sizeof(int64_t);
    job_info_serialize(job, buf);

    return 0;
}

int list_jobs(struct ByteArray *result) {
    // Leave space for the exit code and the entry count
    int64_t entry_count = 0;
    int64_t total_bufsize = sizeof(/* exit code */ int8_t) + sizeof(entry_count);

    // Measure required buffer size
    for (int64_t job_id = 0; job_id < job_count; ++job_id) {
        struct Job *job = find_job_by_id(job_id);
        if (job == NULL || job->status == JOB_STATUS_REMOVED)
            continue;

        total_bufsize += sizeof(/* job id */ int64_t) + job_serialized_size(job);
        ++entry_count;
    }

    bytearray_init(result, total_bufsize);

    char *ptr = result->bytes + sizeof(/* exit code */ int8_t);
    memcpy(ptr, &entry_count, sizeof(entry_count));
    ptr += sizeof(entry_count);

    // Add entries to buffer
    for (int64_t job_id = 0; job_id < job_count; ++job_id) {
        struct Job *job = find_job_by_id(job_id);
        if (job == NULL || job->status == JOB_STATUS_REMOVED)
            continue;

        memcpy(ptr, &job_id, sizeof(job_id));
        ptr += sizeof(job_id);
        ptr = job_info_serialize(job, ptr);
    }

    return 0;
}
/**
 * Returns the id of the job containing the path, and -1 if no such job is found.
 */
int64_t find_job_containing_path(const char *path) {
    for (int64_t job_id = 0; job_id < job_count; job_id++) {
        struct Job *job = find_job_by_id(job_id);
        if (job == NULL || job->status == JOB_STATUS_REMOVED) {
            continue;
        }

        if (strstr(path, job->root->path) != NULL) {
            return job_id;
        }
    }

    return -1;
}

/**
 * Creates a job and returns its id through the job_id argument.
 * Returns 0 for success or an error code otherwise.
 * Errors: -1 if path does not exist
 *         -2 if path is part of an existing job (the returned job_id)
 */
int8_t create_job(const char *path, int8_t priority, int64_t *job_id) {
    int path_result = exists_dir(path);
    if (path_result < 1) {
        if (path_result < 0) {
            perror("Path check");
        }
        return -1;
    }

    *job_id = find_job_containing_path(path);
    if (*job_id >= 0) {
        return -2;
    }

    struct Job *new_job = da_malloc(sizeof(*new_job));
    job_init(new_job);
    new_job->priority = priority;

    struct TraverseArgs *args = da_malloc(sizeof(*args));

    size_t path_len = strlen(path);
    args->path = da_malloc((path_len + 1) * sizeof(*args->path));
    strncpy(args->path, path, path_len);
    args->path[path_len] = 0;

    args->job = new_job;

    *job_id = job_count;
    ++job_count;

    jobs[*job_id] = new_job;

    // Create thread

    // Set thread priority
    pthread_attr_t tattr;
    if (pthread_attr_init(&tattr) < 0) {
        perror("pthread_attr_init");
        return -128;
    }

    struct sched_param param;
    if (pthread_attr_getschedparam(&tattr, &param) < 0) {
        perror("pthread_attr_getschedparam");
        return -128;
    }

    param.sched_priority = priority;

    if (pthread_attr_setschedparam(&tattr, &param) < 0) {
        perror("pthread_attr_setschedparam");
        return -128;
    }

    if (pthread_create(&new_job->thread, &tattr, traverse, (void *)args) < 0) {
        perror("pthread_create");
        return -128;
    }

    return 0;
}

int handle_ipc_cmd(int8_t cmd, struct ByteArray *payload, struct ByteArray *reply) {
    // TODO: Handle reply, errors
    int8_t exit_code = 0;

    if (cmd == CMD_ADD) {
        if (payload->len <= 0) {
            // TODO: Generic bad input error
            return -128;
        }

        int8_t priority = payload->bytes[0];
        // Read path
        int64_t path_len = payload->len - 1;
        char *path = da_malloc((path_len + 1) * sizeof(*path));
        strncpy(path, payload->bytes + 1, path_len);
        // Null terminate
        path[path_len] = 0;

        int64_t job_id;
        exit_code = create_job(path, priority, &job_id);

        free(path);
        exit_code = -exit_code;

        // Set reply payload
        int64_t reply_len = sizeof(exit_code);
        if (exit_code == 0 || exit_code == 2) {
            reply_len += sizeof(job_id);
        }
        bytearray_init(reply, reply_len);

        memcpy(reply->bytes, &exit_code, sizeof(exit_code));
        if (exit_code == 0 || exit_code == 2) {
            memcpy(reply->bytes + sizeof(exit_code), &job_id, sizeof(job_id));
        }
    } else if (cmd == CMD_LIST) {
        list_jobs(reply);
        exit_code = 0;
        memcpy(reply->bytes, &exit_code, sizeof(exit_code));
    } else {
        int64_t job_id;
        if (payload->len != sizeof(job_id)) {
            return -128;
        }

        // Read job_id
        memcpy(&job_id, payload->bytes, sizeof(job_id));

        struct Job *job;
        if ((job = find_job_by_id(job_id)) == NULL) {
            // Job not found
            exit_code = 1;
            bytearray_init(reply, sizeof(exit_code));
            memcpy(reply->bytes, &exit_code, sizeof(exit_code));
            return 0;
        }

        if (cmd == CMD_SUSPEND) {
            pause_job(job);
            exit_code = 0;
            bytearray_init(reply, sizeof(exit_code));
            memcpy(reply->bytes, &exit_code, sizeof(exit_code));

        } else if (cmd == CMD_REMOVE) {
            remove_job(job);
            exit_code = 0;
            bytearray_init(reply, sizeof(exit_code));
            memcpy(reply->bytes, &exit_code, sizeof(exit_code));

        } else if (cmd == CMD_RESUME) {
            resume_job(job);
            exit_code = 0;
            bytearray_init(reply, sizeof(exit_code));
            memcpy(reply->bytes, &exit_code, sizeof(exit_code));

        } else if (cmd == CMD_INFO) {
            get_job_info(job, reply);
            memcpy(reply->bytes, &exit_code, sizeof(exit_code));
            memcpy(reply->bytes + sizeof(exit_code), &job_id, sizeof(job_id));

        } else if (cmd == CMD_PRINT) {
            exit_code = -get_job_results(job, reply);
            if (exit_code != 0) {
                bytearray_init(reply, sizeof(exit_code));
                memcpy(reply->bytes, &exit_code, sizeof(exit_code));
            }
        } else {
            return -1;
        }
    }

    return 0;
}

void *monitor_ipc(void *vserverfd) {
    int serverfd = *((int *)vserverfd);

    while (1) {
        // Continuously accept connections
        int clientfd;
        if ((clientfd = accept(serverfd, NULL, NULL)) < 0) {
            perror("Accepting connection");
            continue;
        }

        if (!validate_ipc_msg(clientfd)) {
            continue;
        }

        // Read command
        int8_t cmd;
        if (read(clientfd, &cmd, 1) < 0) {
            continue;
        }

        // Read payload
        struct ByteArray payload;
        if (read_ipc_payload(clientfd, &payload) < 0) {
            continue;
        }

        struct ByteArray reply;

        handle_ipc_cmd(cmd, &payload, &reply);

        if (write(clientfd, reply.bytes, reply.len) < 0) {
            perror("IPC reply");
            continue;
        }

        bytearray_destroy(&payload);
        bytearray_destroy(&reply);
    }
}

int main() {
    /* int childpid = fork(); */

    /* if (childpid == -1) { */
    /*     // Failed to fork */
    /*     return 1; */
    /* } */
    /* if (childpid > 0) { */
    /*     // Forked successfully */
    /*     return 0; */
    /* } */

    // Prevent SIGPIPE from stopping the daemon
    signal(SIGPIPE, SIG_IGN);

    // In daemon now
    int serverfd = bind_socket();

    // Start IPC monitoring thread
    pthread_t ipc_monitor_thread;
    if (pthread_create(&ipc_monitor_thread, NULL, monitor_ipc, &serverfd) != 0) {
        perror("Failed to create IPC monitoring thread");
        close(serverfd);
        exit(EXIT_FAILURE);
    }
    pthread_join(ipc_monitor_thread, NULL);

    close(serverfd);

    return EXIT_SUCCESS;
}
