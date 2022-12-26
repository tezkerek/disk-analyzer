#define _XOPEN_SOURCE 500
#include <common/utils.h>
#include <common/ipc.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sched.h>
#include <ftw.h>
#include <stdint.h>
#include <sys/stat.h>
#include <libgen.h>
#include <errno.h>

#define MAX_THREADS            100 // danger
#define MAX_CHILDREN           100 // danger
#define JOB_STATUS_IN_PROGRESS 0
#define JOB_STATUS_REMOVED     1
#define JOB_STATUS_PAUSED      2
#define JOB_STATUS_DONE        3

struct Directory {
    char *path;             
    struct Directory *parent;                  
    struct Directory *children[MAX_CHILDREN]; 
    uint64_t bytes;        
    uint64_t number_files;
    uint64_t number_subdir; 
};

struct Job { 
    pthread_t thread;
    int8_t status;           // status of job -> in progress(0), done(1), removed(3), paused(2)
    struct Directory *root; // children directories
};

static uint64_t job_count = 0;
struct Job job_history[MAX_THREADS];

struct Job *jobs[MAX_THREADS];

struct Directory *last[MAX_THREADS];

static int build_arb (const char* fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {

	if (!strcmp(fpath, last[job_count]->path)) return 0;

	char *aux = malloc(strlen(fpath) * sizeof(char) + 1);
	strcpy(aux, fpath);
	strcpy(aux, dirname(aux));
	while (strcmp(aux, last[job_count]->path)) {
		last[job_count]->parent->bytes += last[job_count]->bytes;
		last[job_count] = last[job_count]->parent;
	}

	if (typeflag == FTW_D) {
		struct Directory *current = malloc(sizeof(*current));
		current->parent = malloc(sizeof(*current->parent));
		current->parent = last[job_count];
		last[job_count]->number_subdir++;
		last[job_count]->bytes += sb->st_size;
		current->number_subdir = 0;
		current->path = malloc(strlen(fpath) * sizeof(char) + 1);
		strcpy(current->path, fpath); 
		current->bytes = 0;
		current->number_files = 0;	
		last[job_count]->children[last[job_count]->number_subdir] = current;
		last[job_count] = current;
	}
	else {
		last[job_count]->number_files++;
		last[job_count]->bytes += sb->st_size;
	}

	return 0;
}

void *traverse (void *path) {

	int nopenfd = 20;  // ne gandim cat vrem sa punem
	char *p = path;
	
	struct Directory *root = malloc(sizeof(*root));
	root->path = malloc(strlen(p) * sizeof(char) + 1);
	strcpy(root->path, p);
	root->parent = malloc(sizeof(*root->parent));
	root->parent = NULL;
	root->bytes = root->number_files = root->number_subdir = 0;        
	last[job_count] = root;
	
	if (nftw(p, build_arb, nopenfd, FTW_PHYS) == -1) {
		perror("nsfw");   // modif
		return errno;
	}

	while (last[job_count]->parent != NULL) {
		last[job_count]->parent->bytes += last[job_count]->bytes;
		last[job_count] = last[job_count]->parent;
	}

	return 0;
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

int create_job(char *path, int8_t priority) {
	pthread_attr_t tattr;
	struct sched_param param;

	int ret = pthread_attr_init (&tattr);
	ret = pthread_attr_getschedparam (&tattr, &param);

	param.sched_priority = priority;
	
	ret = pthread_attr_setschedparam (&tattr, &param);

	jobs[job_count] = malloc(sizeof(jobs[job_count]));
	jobs[job_count]->status = JOB_STATUS_IN_PROGRESS;
	ret = pthread_create (&jobs[job_count]->thread, NULL, traverse, (void*)path);
	
	if (ret) {
		perror("Error creating thread");
		exit(EXIT_FAILURE);
	}

	pthread_join(jobs[job_count]->thread, NULL);
	
	jobs[job_count]->root = malloc(sizeof(*jobs[job_count]->root));
	jobs[job_count]->root = last[job_count];

	jobs[job_count]->status = JOB_STATUS_DONE;

	++job_count;
	
	return 0;
}


int handle_ipc_cmd(int8_t cmd, char *payload, int64_t payload_len) {
    if (cmd == CMD_SUSPEND) { // pause analysis
        pause_job(atoi(payload));
    } else if (cmd == CMD_REMOVE) { // remove job
        remove_job(atoi(payload));
    } else if (cmd == CMD_RESUME) { // resume job
        resume_job(atoi(payload));
    } else if (cmd == CMD_PRINT) { // print report for 'done' tasks
        print_done_jobs();
    } else if (cmd == CMD_INFO) { // info about analysis
        get_job_info(atoi(payload));
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
