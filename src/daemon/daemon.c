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

#define MAX_THREADS  100
#define MAX_CHILDREN 100

static uint64_t job_count = 0;

struct Job *jobs[MAX_THREADS];

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

int create_job(char *path, int8_t priority) {
    pthread_attr_t tattr;
    struct sched_param param;

    int ret = pthread_attr_init(&tattr);
    ret = pthread_attr_getschedparam(&tattr, &param);

    param.sched_priority = priority;

    ret = pthread_attr_setschedparam(&tattr, &param);

    // TODO: Mutex lock?
    int job_id = job_count;
    ++job_count;
    // TODO: Mutex unlock?

    struct traverse_args *args = malloc(sizeof(struct traverse_args));
    struct Job *new_job = find_job_by_id(job_id);

    args->path = path;
    args->this_job = new_job;

    new_job = malloc(sizeof(*new_job));
    // TODO: add tattr to pthread_create?
    ret = pthread_create(&new_job->thread, NULL, traverse, (void *)args);

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

        // Read payload length
        int64_t payload_len;
        if ((payload_len = read_payload_length(clientfd)) < 0) {
            continue;
        }

        // Read payload
        char *payload = da_malloc((payload_len + 1) * sizeof(*payload));
        if (read(clientfd, payload, payload_len) < 0) {
            free(payload);
            continue;
        }
        // Null terminate
        payload[payload_len] = 0;

        handle_ipc_cmd(cmd, payload, payload_len);

        // Build reply payload
        int64_t reply_len = payload_len + 6;
        char *reply = da_malloc(reply_len * sizeof(*reply));
        strncpy(reply, "Hello ", sizeof("Hello "));
        strncpy(reply + sizeof("Hello ") - 1, payload, payload_len);

        free(payload);

        // Build IPC message
        int64_t msg_len = min_ipc_msg_len(reply_len);
        char *msg = da_malloc(msg_len * sizeof(*msg));
        build_ipc_msg(cmd, reply, reply_len, msg, msg_len);

        int written_bytes;
        if ((written_bytes = write(clientfd, msg, msg_len)) < 0) {
            perror("Failed to write reply");
        }

        free(reply);
        free(msg);
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
