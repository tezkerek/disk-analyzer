#ifndef THREAD_UTILS_HEADER
#define THREAD_UTILS_HEADER

#include <errno.h>
#include <libgen.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>

#define JOB_STATUS_IN_PROGRESS 0
#define JOB_STATUS_REMOVED     1
#define JOB_STATUS_PAUSED      2
#define JOB_STATUS_DONE        3

struct Directory {
    char *path;
    struct Directory *parent;
    struct Directory **children;
    uint64_t bytes;
    uint32_t number_files;
    uint32_t number_subdir, children_capacity;
};

struct Job {
    pthread_t thread;
    int8_t priority;
    int8_t status;
    pthread_mutex_t status_mutex;     // used for accessing `status`
    pthread_cond_t mutex_resume_cond; // used for safely locking a thread
    struct Directory *root;           // children directories
    int64_t total_dir_count;
    int64_t total_file_count;
};

/**
 * Initialize a Job struct.
 */
int job_init(struct Job *job);

struct TraverseArgs {
    struct Job *job;
    char *path;
};

/**
 * Creates Job object and starts analysing directory
 */
void *traverse(void *path);

int pause_job(struct Job *job);

int resume_job(struct Job *job);

int remove_job(struct Job *job);

#endif
