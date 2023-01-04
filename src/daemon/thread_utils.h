#ifndef THREAD_UTILS_HEADER
#define THREAD_UTILS_HEADER

#include <errno.h>
#include <libgen.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>

#define MAX_THREADS            100
#define MAX_CHILDREN           100
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
    int8_t status;
    pthread_mutex_t status_mutex;     // used for accessing `status`
    pthread_cond_t mutex_resume_cond; // used for safely locking a thread
    struct Directory *root;           // children directories
};

struct traverse_args {
  int job_id;
  char *path;
};

static uint64_t job_count = 0;

/**
 * Finds the job associated with the given id.
 * Returns NULL on error.
 */
struct Job *find_job_by_id(int64_t id);

/**
 * Keeps track of all jobs.
 */
struct Job *jobs[MAX_THREADS];

/**
 * Suspends the caller thread if variable is set.
 * Call on safe points where thread can be suspended.
 */
void check_suspend(struct Job *job_to_check);

/**
 * Memorises parent directory for traversal.
 */
struct Directory *last[MAX_THREADS];

/**
 * Creates Job object and starts analysing directory
 */
void *traverse(void *path);

#endif
