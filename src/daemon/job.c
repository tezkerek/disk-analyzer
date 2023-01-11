#include "job.h"
#include "directory.h"
#include <common/utils.h>
#include <errno.h>
#include <fts.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int job_init(struct Job *job) {
    pthread_mutex_init(&job->status_mutex, NULL);
    job->total_dir_count = 0;
    job->total_file_count = 0;

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
 */
void check_suspend(struct Job *job_to_check) {
    pthread_mutex_t *status_mutex = &job_to_check->status_mutex;
    pthread_cond_t *resume_cond = &job_to_check->mutex_resume_cond;

    pthread_mutex_lock(status_mutex);
    while (job_to_check->status == JOB_STATUS_PAUSED)
        pthread_cond_wait(resume_cond, status_mutex);
    pthread_mutex_unlock(status_mutex);
}

void *traverse(void *vargs) {
    struct TraverseArgs *args = (struct TraverseArgs *)vargs;
    char *path = args->path;
    struct Job *job = args->job;

    job->status = JOB_STATUS_IN_PROGRESS;
    job->total_dir_count = 0;
    job->total_file_count = 0;

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

    int is_root = 1;
    while ((p = fts_read(ftsp)) != NULL) {
        check_suspend(job);
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
                is_root = 0;
            } else {
                // Create a new directory as subdir of the current directory
                struct Directory *new_dir = da_malloc(sizeof(*new_dir));
                directory_init(new_dir, p->fts_path);
                current_dir->subdirs =
                    dirlist_push_front(current_dir->subdirs, new_dir);

                new_dir->bytes += p->fts_statp->st_size;

                current_dir = new_dir;
                job->total_dir_count += 1;
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
    if (job->status == JOB_STATUS_REMOVED) {
        job_destroy(job);
    } else
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
