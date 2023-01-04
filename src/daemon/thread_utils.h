#ifndef THREAD_UTILS_HEADER
#define THREAD_UTILS_HEADER

#include <errno.h>
#include <ftw.h>
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
    struct Directory *children[MAX_CHILDREN];
    uint64_t bytes;
    uint64_t number_files;
    uint64_t number_subdir;
};

struct Job {
    pthread_t thread;
    int8_t status;
    pthread_mutex_t status_mutex;     // used for accessing `status`
    pthread_cond_t mutex_resume_cond; // used for safely locking a thread
    struct Directory *root;           // children directories
};

/**
 * Finds the job associated with the given id.
 * Returns NULL on error.
 */
struct Job *find_job_by_id(int64_t id);

/**
 * Suspends the caller thread if variable is set.
 * Call on safe points where thread can be suspended.
 */
void check_suspend(struct Job *job_to_check);

/**
 * Creates Job object and starts analysing directory
 */
void *traverse(void *path);

#endif