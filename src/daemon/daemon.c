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

#define MAX_THREADS            100 // danger
#define MAX_CHILDREN           100 // danger
#define JOB_STATUS_IN_PROGRESS 0
#define JOB_STATUS_REMOVED     1
#define JOB_STATUS_PAUSED      2
#define JOB_STATUS_DONE        3

struct Directory {
    char *path;                               // path to this directory
    struct Directory *children[MAX_CHILDREN]; // children directories
    // Change unit of measurement for folders?
    uint64_t bytes; // size of folder
    // number of files at this level, not counting grandchildren !!
    uint64_t file_count;
};
struct Job // este Misu <3
{
    pthread_t thread;
    int8_t status; // status of job -> in progress(0), done(1), removed(3), paused(2)
    struct Directory *root; // children directories
};

static uint64_t job_count = 0;
struct Job job_history[MAX_THREADS];

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

void pretty_print_job(){};

int remove_job(int64_t id) {
    struct Job *this_job = &job_history[id];
    if (this_job->status != JOB_STATUS_DONE) {
        if (kill(this_job->thread, SIGTERM) < 0) {
            return -1;
        }
    }
    this_job->status = JOB_STATUS_REMOVED;
    return 0;
}

int resume_job(int64_t id) {
    struct Job *this_job = &job_history[id];
    if (kill(this_job->thread, SIGCONT) < 0) {
        return -1;
    }
    this_job->status = JOB_STATUS_IN_PROGRESS;
    return 0;
}

int pause_job(int64_t id) {
    struct Job *this_job = &job_history[id];
    if (this_job->status != JOB_STATUS_DONE) {

        if (kill(this_job->thread, SIGSTOP) < 0) {
            return -1;
        }
        this_job->status = JOB_STATUS_PAUSED;
    }
    return 0;
} // mmove to new file?

int print_done_jobs() {
    for (size_t i = 0; i < job_count; i++) {
        if (job_history[i].status == JOB_STATUS_DONE) {
            pretty_print_job(i);
        }
    }
}

int get_job_info(int64_t id) {}

int list_jobs() {}

/**
 * Creates a job and returns its id through the job_id argument.
 * Returns 0 for success or an error code otherwise.
 * Errors: -1 if path does not exist
 *         -2 if path is part of an existing job (the returned job_id)
 */
int create_job(const char *path, int8_t priority, int64_t *job_id) {}

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

        if (cmd == CMD_SUSPEND) {
            pause_job(job_id);
        } else if (cmd == CMD_REMOVE) {
            remove_job(job_id);
        } else if (cmd == CMD_RESUME) {
            resume_job(job_id);
        } else if (cmd == CMD_INFO) {
            get_job_info(job_id);
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
