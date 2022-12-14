#include <common/ipc.h>
#include <common/utils.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#define MAX_THREADS  100 // danger
#define MAX_CHILDREN 100 // danger
#define IN_PROGRESS  0
#define REMOVED      1
#define PAUSED       2
#define DONE         3

struct Directory {
    char *path;                               // path to this directory
    struct Directory *children[MAX_CHILDREN]; // children directories
    // Change unit of measurement for folders?
    uint32_t bytes; // size of folder
    uint32_t
        number_files; // number of files at this level, not counting grandchildren !!
};
struct Pet_tree_node // este Misu <3
{
    pthread_t thread;
    short status; // status of job -> in progress(0), done(1), removed(3), paused(2)
    struct Directory *root; // children directories
};

static uint64_t id_sequence = 0;
struct Pet_tree_node job_history[MAX_THREADS];

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

int remove_analisys(int64_t id) {
    struct Pet_tree_node *this_job = &job_history[id];
    if (this_job->status != DONE) {
        if (kill(this_job->thread, SIGTERM) < 0) {
            return -1;
        }
    }
    this_job->status = REMOVED;
    return 0;
}

int resume_analisys(int64_t id) {
    struct Pet_tree_node *this_job = &job_history[id];
    if (kill(this_job->thread, SIGCONT) < 0) {
        return -1;
    }
    this_job->status = IN_PROGRESS;
    return 0;
}

int pause_thread(int64_t id) {
    struct Pet_tree_node *this_job = &job_history[id];
    if (this_job->status != DONE) {

        if (kill(this_job->thread, SIGSTOP) < 0) {
            return -1;
        }
        this_job->status = PAUSED;
    }
    return 0;
} // mmove to new file?

int print_done_jobs() {
    for (size_t i = 0; i < id_sequence; i++) {
        if (job_history[i].status == DONE) {
            pretty_print_job(i);
        }
    }
}

int info_analisys(int64_t id) {}

int list_jobs() {}

int create_job(char *path, int8_t priority) {}

int handle_ipc_cmd(int8_t cmd, char *payload, int64_t payload_len) {

    if (cmd == CMD_SUSPEND) { // pause analysis
        pause_thread(atoi(payload));
    } else if (cmd == CMD_REMOVE) { // remove job
        remove_analisys(atoi(payload));
    } else if (cmd == CMD_RESUME) { // resume job
        resume_analisys(atoi(payload));
    } else if (cmd == CMD_PRINT) { // print report for 'done' tasks
        print_done_jobs();
    } else if (cmd == CMD_INFO) { // info about analysis
        info_analisys(atoi(payload));
    } else if (cmd == CMD_LIST) { // list all tasks
        list_jobs();
    } else if (cmd == CMD_ADD) { // create job
        create_job(payload + 1, payload[0] - '0');
    }
    return 1;
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

    return EXIT_SUCCESS;
}
