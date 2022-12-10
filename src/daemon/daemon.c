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
#define _XOPEN_SOURCE 501
#include <ftw.h>
#include <stdint.h>
#include <sys/stat.h>
#include <libgen.h>



#define MAX_THREADS 100
#define MAX_CHILDREN 59

struct Directory 
{
    char *path;             
    struct Directory *parent;                  
    struct Directory *children[MAX_CHILDREN]; 
    uint32_t bytes;        
    uint32_t number_files;
    uint32_t number_subdir; 
};

struct Pet_tree_node // este Misu <3
{
    short status;           // status of job -> in progress(0), done(1), removed(3), paused(2)
    struct Directory *root; // children directories
};
static uint64_t idSequence = 0;

pthread_t threads_arr[MAX_THREADS]; 

struct Directory *last[100];

static int build_arb (const char* fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
	if (S_ISDIR(sb->st_mode) && last[0]->parent != NULL) {
		while (strcmp(dirname(fpath), last[0]->path)) {
			last[0] = last[0]->parent;
		}
		struct Directory *current;
		current->parent = last[0];
		last[0]->number_subdir++;
		current->number_subdir = 0;
		current->path = fpath;
		current->bytes = 0;
		current->number_files = 0;	
		last[0]->children[last[0]->number_subdir] = current;
		last[0] = current;
	}
	else if (!S_ISDIR(sb->st_mode)) {
		last[0]->number_files++;
		last[0]->bytes += sb->st_size;
	}

	return 0;
}

void *traverse (void *path) {
	printf("%s", "am intrat in traverse\n");
	int nopenfd = 20;  // ne gandim cat vrem sa punem
	char *p = path;
	
	struct Directory *root;
	strcpy(root->parent, p);
	root->parent = NULL;
	root->bytes = root->number_files = root->number_subdir = 0;        
	last[0] = root;
	
	if (nftw(p, build_arb, nopenfd, NULL) == -1) {
		perror("nsfw");   // modif
		exit(EXIT_FAILURE); 
	}
	
	
	printf("ajung aici? %d\n", root->bytes);
	return 0;
	//exit(EXIT_SUCCESS);
	//return 5;
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

int pause_thread(int64_t id) {}
int remove_analisys(int64_t id) {}
int print_done_jobs() {}
int resume_analisys(int64_t id) {}
int info_analisys(int64_t id) {}
int list_jobs() {}


int create_job(char *path, int8_t priority) {
	pthread_attr_t tattr;
	struct sched_param param;

	int ret = pthread_attr_init (&tattr);
	ret = pthread_attr_getschedparam (&tattr, &param);

	param.sched_priority = priority;
	
	ret = pthread_attr_setschedparam (&tattr, &param);

	ret = pthread_create (&threads_arr[0], NULL, traverse, (void*)path);

	if (ret) {
		perror("Error creating thread");
		exit(EXIT_FAILURE);
	}

	pthread_join(threads_arr[0], NULL);
	
	printf("%s", "gata create job\n");
	
	
	
	++idSequence;
	
	return 0;
}


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
        create_job(payload + 1, atoi(payload[0]));
    }
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
    //int serverfd = bind_socket();

    // Start IPC monitoring thread
    //pthread_t ipc_monitor_thread;
    //if (pthread_create(&ipc_monitor_thread, NULL, monitor_ipc, &serverfd) != 0) {
    //    perror("Failed to create IPC monitoring thread");
    //    close(serverfd);
    //    exit(EXIT_FAILURE);
    //}
    //pthread_join(ipc_monitor_thread, NULL);

    //close(serverfd);
    printf("%s", "meow");
	int x = create_job("/home/adela/disk-analyzer", 2);
    return EXIT_SUCCESS;
}
