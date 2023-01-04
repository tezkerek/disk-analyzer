#define _XOPEN_SOURCE 500
#include <common/utils.h>
#include <common/ipc.h>
#include <pthread.h>
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



#define MAX_THREADS 100
#define MAX_CHILDREN 59

struct Directory 
{
    char *path;             
    struct Directory *parent;                  
    struct Directory *children[MAX_CHILDREN]; 
    uint64_t bytes;        
    uint64_t number_files;
    uint64_t number_subdir; 
};

struct Job // este Misu <3
{
    pthread_t thread;
    int8_t status;           // status of job -> in progress(0), done(1), removed(3), paused(2)
    struct Directory *root; // children directories
};

static uint64_t job_count = 0;
struct Job job_history[MAX_THREADS];

struct Job *jobs[MAX_THREADS];

static uint64_t idSequence = 0;

pthread_t threads_arr[MAX_THREADS]; 

struct Directory *last[MAX_THREADS];

static int build_arb (const char* fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {

	if (!strcmp(fpath, last[idSequence]->path)) return 0;
	
	char *aux = malloc(strlen(fpath) * sizeof(char) + 1);
	strcpy(aux, fpath);
	strcpy(aux, dirname(aux));
	while (strcmp(aux, last[idSequence]->path)) {
		last[idSequence]->parent->bytes += last[idSequence]->bytes;
		last[idSequence] = last[idSequence]->parent;
	}
	
	if (typeflag == FTW_D) {
		struct Directory *current = malloc(sizeof(*current));
		current->parent = malloc(sizeof(*current->parent));
		current->parent = last[idSequence];
		last[idSequence]->number_subdir++;
		last[idSequence]->bytes += sb->st_size;
		current->number_subdir = 0;
		current->path = malloc(strlen(fpath) * sizeof(char) + 1);
		strcpy(current->path, fpath); 
		current->bytes = 0;
		current->number_files = 0;	
		last[idSequence]->children[last[idSequence]->number_subdir] = current;
		last[idSequence] = current;
	}
	else {
		last[idSequence]->number_files++;
		last[idSequence]->bytes += sb->st_size;
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
	last[idSequence] = root;
	
	if (nftw(p, build_arb, nopenfd, FTW_PHYS) == -1) {
		perror("nsfw");   // modif
		return errno;
	}

	while (last[idSequence]->parent != NULL) {
		last[idSequence]->parent->bytes += last[idSequence]->bytes;
		last[idSequence] = last[idSequence]->parent;
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

	jobs[idSequence] = malloc(sizeof(jobs[idSequence]));
	jobs[idSequence]->status = JOB_STATUS_IN_PROGRESS;
	ret = pthread_create (&threads_arr[idSequence], NULL, traverse, (void*)path);
	
	if (ret) {
		perror("Error creating thread");
		exit(EXIT_FAILURE);
	}

	pthread_join(threads_arr[idSequence], NULL);
	
	jobs[idSequence]->root = malloc(sizeof(*jobs[idSequence]->root));
	jobs[idSequence]->root = last[idSequence];

	jobs[idSequence]->status = JOB_STATUS_DONE;

	++idSequence;
	
	return 0;
}


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
	// int x = create_job("/home/adela/so_lab_mare", 2);
	// printf("%d\n", jobs[idSequence - 1]->root->bytes);

    return EXIT_SUCCESS;
}
