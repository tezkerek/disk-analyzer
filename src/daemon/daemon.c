#include <common/ipc.h>
#include <common/utils.h>
#include <daemon/thread_utils.h>
#include <pthread.h>    // threads, mutexes
#include <signal.h>     // kill, SIGTERM
#include <stdio.h>      // perror
#include <stdlib.h>     // atoi, malloc etc.
#include <string.h>     // strncpy
#include <sys/socket.h> // sockets
#include <sys/un.h>     // sockaddr_un
#include <unistd.h>     // sockets

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

int get_job_info(int64_t id) {}

int list_jobs() {}

int create_job(char *path, int8_t priority) {
    pthread_attr_t tattr;
    struct sched_param param;

    int ret = pthread_attr_init(&tattr);
    ret = pthread_attr_getschedparam(&tattr, &param);

    param.sched_priority = priority;

    ret = pthread_attr_setschedparam(&tattr, &param);

    jobs[job_count] = malloc(sizeof(jobs[job_count]));
    jobs[job_count]->status = JOB_STATUS_IN_PROGRESS;
    ret = pthread_create(&jobs[job_count]->thread, NULL, traverse, (void *)path);

    if (ret) {
        perror("Error creating thread");
        return -1;
    }

    pthread_join(jobs[job_count]->thread, NULL);

    jobs[job_count]->root = last[job_count];

    jobs[job_count]->status = JOB_STATUS_DONE;

    ++job_count;

    return 0;
}

int handle_ipc_cmd(int8_t cmd, char *payload, int64_t payload_len) {
    if (cmd == CMD_SUSPEND) { // pause analysis
        int64_t requested_id = atoi(payload);
        if (verify_id(requested_id) == 0) {
            return -1;
        }

        pause_job(jobs[requested_id]);
    } else if (cmd == CMD_REMOVE) { // remove job
        int64_t requested_id = atoi(payload);
        if (verify_id(requested_id) == 0) {
            return -1;
        }

        remove_job(jobs[requested_id]);
    } else if (cmd == CMD_RESUME) { // resume job
        int64_t requested_id = atoi(payload);
        if (verify_id(requested_id) == 0) {
            return -1;
        }

        resume_job(jobs[requested_id]);
    } else if (cmd == CMD_PRINT) { // print report for 'done' tasks
        print_done_jobs();

    } else if (cmd == CMD_INFO) { // info about analysis
        int64_t requested_id = atoi(payload);
        if (verify_id(requested_id) == 0) {
            return -1;
        }

        get_job_info(requested_id);
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
    // int x = create_job("/home/adela/so_lab_mare", 2);
    // printf("%d\n", jobs[job_count - 1]->root->bytes);

    return EXIT_SUCCESS;
}
