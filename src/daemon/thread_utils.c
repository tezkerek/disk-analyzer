#include <common/utils.h>
#include <daemon/thread_utils.h>
#include <errno.h>
#include <fts.h>
#include <libgen.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void clean_dir(struct Directory *dir) {
    free(dir->path);
    for (uint32_t i = 0; i < dir->children_capacity; i++) {
        if (dir->children[i] != NULL) {
            clean_dir(dir->children[i]);
            free(dir->children[i]);
        }
    }
    free(dir->children);
}

void clean_job(struct Job *job) {
    clean_dir(job->root);
    free(job->root);
    pthread_mutex_destroy(&job->status_mutex);
    job->root = NULL;
}

int job_init(struct Job *job) {
    pthread_mutex_init(&job->status_mutex, NULL);
    job->total_dir_count = 0;
    job->total_file_count = 0;

    return 0;
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

/**
 * Creates a new directory
 * path - the path of the new directory
 * parent - the parent of the new directory,
 * should be NULL if the new directory is root
 */
struct Directory *create_directory(const char *path, struct Directory *parent) {
    struct Directory *new_dir = da_malloc(sizeof(*new_dir));

    new_dir->path = da_malloc((strlen(path) + 1) * sizeof(*new_dir->path));
    strcpy(new_dir->path, path);
    new_dir->parent = parent;
    new_dir->bytes = new_dir->number_files = 0;
    new_dir->children_capacity = new_dir->number_subdir = 0;
    new_dir->children = NULL;

    if (parent != NULL) {
        // Add the new directory to the list of his parents children
        if (parent->children_capacity == parent->number_subdir) {
            // TODO: Switch to linked list
            if (parent->children_capacity == 0)
                parent->children_capacity = 1;
            else
                parent->children_capacity *= 2;

            parent->children =
                realloc(parent->children,
                        parent->children_capacity * sizeof(struct Directory *));
        }

        parent->children[parent->number_subdir++] = new_dir;
    }

    return new_dir;
}

void *traverse(void *vargs) {
    struct TraverseArgs *args = (struct TraverseArgs *)vargs;
    char *path = args->path;
    struct Job *job = args->job;

    job->status = JOB_STATUS_IN_PROGRESS;
    job->total_dir_count = 0;
    job->total_file_count = 0;

    struct Directory *current_dir = create_directory(path, NULL);
    job->root = current_dir;

    FTS *ftsp;
    FTSENT *p, *chp;

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
            current_dir->number_files += 1;
            job->total_file_count += 1;
            break;
        case FTS_D:
            // It's a directory, traverse its contents

            // if it's the root directory, we shouldn't create it again
            if (is_root) {
                is_root = 0;
            } else {
                job->total_dir_count += 1;
                current_dir = create_directory(p->fts_path, current_dir);
                current_dir->bytes += p->fts_statp->st_size;
            }

            chp = fts_children(ftsp, 0);
            if (chp == NULL) {
                if (errno != 0) {
                    perror("fts_children");
                }
                break;
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
        clean_job(job);
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
