#include <common/ipc.h>
#include <common/utils.h>
#include <daemon/thread_utils.h>
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

// TODO: Atomic int
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

void pretty_print_job() {}

int print_done_jobs() {}

int get_job_info(int64_t id) {}

int list_jobs() {}

/**
 * Creates a job and returns its id through the job_id argument.
 * Returns 0 for success or an error code otherwise.
 * Errors: -1 if path does not exist
 *         -2 if path is part of an existing job (the returned job_id)
 */
int create_job(const char *path, int8_t priority, int64_t *job_id) {
    struct Job *new_job = da_malloc(sizeof(*new_job));
    struct TraverseArgs *args = da_malloc(sizeof(*args));

    size_t path_len = strlen(path);
    args->path = da_malloc((path_len + 1) * sizeof(*args->path));
    strncpy(args->path, path, path_len);
    args->job = new_job;

    // TODO: Atomic int
    *job_id = job_count;
    ++job_count;

    jobs[*job_id] = new_job;

    // Create thread

    // Set thread priority
    pthread_attr_t tattr;
    if (pthread_attr_init(&tattr) < 0) {
        perror("pthread_attr_init");
        return -1;
    }

    struct sched_param param;
    if (pthread_attr_getschedparam(&tattr, &param) < 0) {
        perror("pthread_attr_getschedparam");
        return -1;
    }

    param.sched_priority = priority;

    if (pthread_attr_setschedparam(&tattr, &param) < 0) {
        perror("pthread_attr_setschedparam");
        return -1;
    }

    if (pthread_create(&new_job->thread, &tattr, traverse, (void *)args) < 0) {
        perror("pthread_create");
        return -1;
    }

    return 0;
}

int handle_ipc_cmd(int8_t cmd, struct ByteArray *payload) {
    // TODO: Handle reply, errors
    if (cmd == CMD_ADD) {
        if (payload->len <= 0) {
            // TODO: Generic bad input error
            return -255;
        }

        int8_t priority = payload->bytes[0];
        // Read path
        char *path = da_malloc((payload->len + 1) * sizeof(char));
        strncpy(path, payload->bytes, payload->len);
        // Null terminate
        path[payload->len] = 0;

        int64_t job_id;
        int8_t code = create_job(path, priority, &job_id);

        free(path);
    } else if (cmd == CMD_PRINT) {
        print_done_jobs();
    } else if (cmd == CMD_LIST) {
        list_jobs();
    } else {
        int64_t job_id;
        if (payload->len != sizeof(job_id)) {
            return -255;
        }

        // Read job_id
        memcpy(&job_id, payload->bytes, sizeof(job_id));

        struct Job *job;
        if ((job = find_job_by_id(job_id)) == NULL) {
            return -255;
        }

        if (cmd == CMD_SUSPEND) {
            pause_job(job);
        } else if (cmd == CMD_REMOVE) {
            remove_job(job);
        } else if (cmd == CMD_RESUME) {
            resume_job(job);
        } else if (cmd == CMD_INFO) {
            get_job_info(job);
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

        handle_ipc_cmd(cmd, &payload);

        bytearray_destroy(&payload);
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
    // int x = create_job("/home/adela/so_lab_mare", 2);
    // printf("%d\n", jobs[job_count - 1]->root->bytes);

    return EXIT_SUCCESS;
}
