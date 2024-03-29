#ifndef THREAD_UTILS_HEADER
#define THREAD_UTILS_HEADER

#include "directory.h"
#include <errno.h>
#include <pthread.h>
#include <stdint.h>

struct Job {
    pthread_t thread;
    int8_t priority;
    int8_t status;
    pthread_mutex_t status_mutex;     /// Lock for `status`
    pthread_cond_t mutex_resume_cond; /// Condition for resuming job
    struct Directory *root;           /// Root directory of job
    int64_t total_subdir_count;       /// Number of all subdirs
    int64_t total_file_count;         /// Number of all files
    int64_t total_path_len;           /// Sum of lengths of all paths
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

/**
 * The required size to serialize the job.
 */
int64_t job_serialized_size(struct Job *job);

/**
 * Serializes the job info into buf.
 * Assumes that enough space has been allocated.
 * Returns pointer to the next position in the buffer to write.
 */
char* job_info_serialize(struct Job *job, char *buf);

#endif
