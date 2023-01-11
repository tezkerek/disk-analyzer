#ifndef THREAD_UTILS_HEADER
#define THREAD_UTILS_HEADER

#include "directory.h"
#include <errno.h>
#include <pthread.h>
#include <stdint.h>

#define JOB_STATUS_IN_PROGRESS 0
#define JOB_STATUS_REMOVED     1
#define JOB_STATUS_PAUSED      2
#define JOB_STATUS_DONE        3

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
 * Traverses a directory, measuring its size on disk.
 * Receives a TraverseArgs struct as argument.
 */
void *traverse(void *vargs);

int pause_job(struct Job *job);

int resume_job(struct Job *job);

int remove_job(struct Job *job);

#endif
