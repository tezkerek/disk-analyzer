#include <common/ipc.h>
#include <common/utils.h>
#include <daemon/thread_utils.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

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

int pause_job(struct Job *job_to_pause) {
    pthread_mutex_t *status_mutex = &job_to_pause->status_mutex;

    pthread_mutex_lock(status_mutex);
    if (job_to_pause->status != JOB_STATUS_IN_PROGRESS) {
        pthread_mutex_unlock(status_mutex);
        return -1;
    }
    job_to_pause->status = JOB_STATUS_PAUSED;
    pthread_mutex_unlock(status_mutex);

    return 0;
}

int resume_job(struct Job *job_to_resume) {
    pthread_mutex_t *status_mutex = &job_to_resume->status_mutex;
    pthread_cond_t *resume_cond = &job_to_resume->mutex_resume_cond;

    pthread_mutex_lock(status_mutex);
    if (job_to_resume->status != JOB_STATUS_PAUSED) {
        pthread_mutex_unlock(status_mutex);
        return -1;
    }
    job_to_resume->status = JOB_STATUS_IN_PROGRESS;
    pthread_cond_broadcast(resume_cond);
    pthread_mutex_unlock(status_mutex);

    return 0;
}

int remove_job(struct Job *job_to_remove) {
    pthread_mutex_t *status_mutex = &job_to_remove->status_mutex;

    if (pause_job(job_to_remove) < 0) {
        return -1;
    }
    if (kill(job_to_remove->thread, SIGTERM) < 0) {
        return -1;
    }
    // TODO: free tree structure
    pthread_mutex_lock(status_mutex);
    job_to_remove->status = JOB_STATUS_REMOVED;
    pthread_mutex_unlock(status_mutex);

    return 0;
}

int print_done_jobs() {}

int get_job_info(struct Job *this_job) {}

int list_jobs() {}

/**
 * Creates a job and returns its id through the job_id argument.
 * Returns 0 for success or an error code otherwise.
 * Errors: -1 if path does not exist
 *         -2 if path is part of an existing job (the returned job_id)
 */
int create_job(char *path, int8_t priority, int64_t *job_id) {
    pthread_attr_t tattr;
    struct sched_param param;

    int ret = pthread_attr_init(&tattr);
    ret = pthread_attr_getschedparam(&tattr, &param);

    param.sched_priority = priority;

    ret = pthread_attr_setschedparam(&tattr, &param);

    // TODO: Mutex lock?
    *job_id = job_count;
    ++job_count;
    // TODO: Mutex unlock?

    struct Traverse_Args *args = malloc(sizeof(struct Traverse_Args));
    args->path = path;
    args->job_id = *job_id;

    jobs[*job_id] = malloc(sizeof(jobs[*job_id]));
    // TODO: add tattr to pthread_create?
    ret = pthread_create(&jobs[*job_id]->thread, NULL, traverse, (void *)args);

    if (ret) {
        perror("Error creating thread");
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

        struct Job *this_job;
        if ((this_job = find_job_by_id(job_id)) == NULL) {
            return -255;
        }

        if (cmd == CMD_SUSPEND) {
            pause_job(this_job);
        } else if (cmd == CMD_REMOVE) {
            remove_job(this_job);
        } else if (cmd == CMD_RESUME) {
            resume_job(this_job);
        } else if (cmd == CMD_INFO) {
            get_job_info(this_job);
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

    return EXIT_SUCCESS;
}
