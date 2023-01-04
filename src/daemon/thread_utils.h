#ifndef THREAD_UTILS_HEADER
#define THREAD_UTILS_HEADER

#include <errno.h>   // nsfw related
#include <ftw.h>     // nsfw related
#include <libgen.h>  // nsfw related
#include <pthread.h> // threads, mutexes
#include <sched.h>   // nsfw related
#include <stdint.h>  // nsfw related

#define MAX_THREADS            100
#define MAX_CHILDREN           100
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

/**
 * Fills directory structure.
 */
static int build_tree(const char *fpath,
                      const struct stat *sb,
                      int typeflag,
                      struct FTW *ftwbuf);

#endif