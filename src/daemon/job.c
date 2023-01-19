#include "job.h"
#include "directory.h"
#include <common/ipc.h>
#include <common/utils.h>
#include <errno.h>
#include <fts.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int job_init(struct Job *job) {
    job->priority = 2;
    job->status = JOB_STATUS_IN_PROGRESS;
    pthread_mutex_init(&job->status_mutex, NULL);
    job->total_subdir_count = 0;
    job->total_file_count = 0;
    job->total_path_len = 0;

    return 0;
}

void job_destroy(struct Job *job) {
    pthread_mutex_destroy(&job->status_mutex);
    directory_destroy(job->root);
    free(job->root);
    job->root = NULL;
}

/**
 * Suspends the caller thread if variable is set.
 * Call on safe points where thread can be suspended.
 * Returns 1 of the job needs to be removed
 * Returns 0 if the job should be continued
 */
int check_suspend(struct Job *job_to_check) {
    pthread_mutex_t *status_mutex = &job_to_check->status_mutex;
    pthread_cond_t *resume_cond = &job_to_check->mutex_resume_cond;

    pthread_mutex_lock(status_mutex);
    while (job_to_check->status == JOB_STATUS_PAUSED)
        pthread_cond_wait(resume_cond, status_mutex);

    if (job_to_check->status == JOB_STATUS_REMOVED) {
        pthread_mutex_unlock(status_mutex);
        return 1;
    }
    pthread_mutex_unlock(status_mutex);
    return 0;
}

void *traverse(void *vargs) {
    struct TraverseArgs *args = (struct TraverseArgs *)vargs;
    char *path = args->path;
    struct Job *job = args->job;

    size_t path_len = strlen(path);

    job->status = JOB_STATUS_IN_PROGRESS;
    job->total_subdir_count = 0;
    job->total_file_count = 0;
    job->total_path_len = path_len;

    // Create root dir
    struct Directory *current_dir = da_malloc(sizeof(*current_dir));
    directory_init(current_dir, path);
    job->root = current_dir;

    FTS *ftsp;
    FTSENT *p;

    char *fts_args[] = {path, NULL};

    if ((ftsp = fts_open(fts_args, FTS_NOCHDIR | FTS_PHYSICAL, NULL)) == NULL) {
        perror("fts_open");
        return NULL;
    }

    size_t path_prefix_len = path_len + 1;

    int is_root = 1;
    while ((p = fts_read(ftsp)) != NULL) {

        if (check_suspend(job) == 1) {
            fts_close(ftsp);
            job_destroy(job);

            return 0;
        }
        // TODO: Check if job was removed as well

        switch (p->fts_info) {
        case FTS_F:
            // It's a file, add its size to the total
            current_dir->bytes += p->fts_statp->st_size;
            job->total_file_count += 1;
            break;
        case FTS_D:
            // It's a directory, traverse its contents

            // if it's the root directory, we shouldn't create it again
            if (is_root) {
                current_dir->bytes += p->fts_statp->st_size;
                is_root = 0;
            } else {
                // Create a new directory
                struct Directory *new_dir = da_malloc(sizeof(*new_dir));

                // Use the path relative to the job path
                const char *relative_path = p->fts_path + path_prefix_len;
                directory_init(new_dir, relative_path);

                // Add it as a subdir of the current directory
                current_dir->subdirs =
                    dirlist_push_front(current_dir->subdirs, new_dir);
                new_dir->parent = current_dir;

                new_dir->bytes += p->fts_statp->st_size;

                current_dir = new_dir;
                job->total_subdir_count += 1;
                job->total_path_len += strlen(relative_path);
            }

            break;
        case FTS_NS:
            fprintf(stderr, "fts_read: no stat for %s\n", p->fts_path);
            break;
        case FTS_DP:
            // if it's the root directory, we shouldn't go to its parent
            if (current_dir->parent != NULL) {
                current_dir->parent->bytes += current_dir->bytes;
                current_dir = current_dir->parent;
            }
            break;
        }
    }

    if (errno != 0) {
        perror("fts_read");
    }

    fts_close(ftsp);
    job->status = JOB_STATUS_DONE;

    return 0;
}

int pause_job(struct Job *job) {
    pthread_mutex_t *status_mutex = &job->status_mutex;

    pthread_mutex_lock(status_mutex);
    if (job->status != JOB_STATUS_IN_PROGRESS) {
        pthread_mutex_unlock(status_mutex);
        return -1;
    }
    job->status = JOB_STATUS_PAUSED;
    pthread_mutex_unlock(status_mutex);

    return 0;
}

int resume_job(struct Job *job) {
    pthread_mutex_t *status_mutex = &job->status_mutex;
    pthread_cond_t *resume_cond = &job->mutex_resume_cond;

    pthread_mutex_lock(status_mutex);
    if (job->status != JOB_STATUS_PAUSED) {
        pthread_mutex_unlock(status_mutex);
        return -1;
    }
    job->status = JOB_STATUS_IN_PROGRESS;
    pthread_cond_broadcast(resume_cond);
    pthread_mutex_unlock(status_mutex);

    return 0;
}

int remove_job(struct Job *job) {
    pthread_mutex_t *status_mutex = &job->status_mutex;

    pthread_mutex_lock(status_mutex);
    job->status = JOB_STATUS_REMOVED;
    pthread_mutex_unlock(status_mutex);

    return 0;
}

int64_t job_serialized_size(struct Job *job) {
    int64_t path_len = strlen(job->root->path);
    return sizeof(job->priority) + sizeof(path_len) + path_len +
           sizeof(/* progress */ int8_t) + sizeof(job->status) +
           sizeof(job->total_file_count) + sizeof(job->total_subdir_count);
}

char *job_info_serialize(struct Job *job, char *buf) {
    char *ptr = buf;

    memcpy(ptr, &job->priority, sizeof(job->priority));
    ptr += sizeof(job->priority);

    int64_t path_len = strlen(job->root->path);
    memcpy(ptr, &path_len, sizeof(path_len));
    ptr += sizeof(path_len);

    memcpy(ptr, job->root->path, path_len);
    ptr += path_len;

    int8_t progress = job->status == JOB_STATUS_DONE ? 100 : 50;
    memcpy(ptr, &progress, sizeof(progress));
    ptr += sizeof(progress);

    memcpy(ptr, &job->status, sizeof(job->status));
    ptr += sizeof(job->status);

    memcpy(ptr, &job->total_file_count, sizeof(job->total_file_count));
    ptr += sizeof(job->total_file_count);

    memcpy(ptr, &job->total_subdir_count, sizeof(job->total_subdir_count));
    ptr += sizeof(job->total_subdir_count);

    return ptr;
}
