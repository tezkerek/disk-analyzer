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

void clean_dir(struct Directory *this_dir) {
    free(this_dir->path);
    for (uint32_t i = 0; i < this_dir->children_capacity; i++) {
        if (this_dir->children[i] != NULL) {
            clean_dir(this_dir->children[i]);
            free(this_dir->children[i]);
        }
    }
    free(this_dir->children);
}

void clean_job(struct Job *this_job) {
    clean_dir(this_job->root);
    free(this_job->root);
    free(this_job);
    this_job = NULL;
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
struct Directory *create_directory(char *path, struct Directory *parent) {
    struct Directory *new_directory = malloc(sizeof(struct Directory));
    new_directory->path = malloc((strlen(path) + 1) * sizeof(char));
    strcpy(new_directory->path, path);
    new_directory->parent = parent;
    new_directory->bytes = new_directory->number_files = 0;
    new_directory->children_capacity = new_directory->number_subdir = 0;
    new_directory->children = NULL;

    if (parent != NULL) {
        // Add the new directory to the list of his parents children
        if (parent->children_capacity == parent->number_subdir) {
            if (parent->children_capacity == 0)
                parent->children_capacity = 1;
            else
                parent->children_capacity *= 2;

            parent->children =
                realloc(parent->children,
                        parent->children_capacity * sizeof(struct Directory *));
        }

        parent->children[parent->number_subdir++] = new_directory;
    }

    return new_directory;
}

void *traverse(void *args) {
    char *path = ((struct traverse_args *)args)->path;
    struct Job *this_job = ((struct traverse_args *)args)->this_job;
    this_job->status = JOB_STATUS_IN_PROGRESS;

    struct Directory *current_directory = create_directory(path, NULL);
    this_job->root = current_directory;

    FTS *ftsp;
    FTSENT *p, *chp;

    char **fts_args = malloc(2 * sizeof(char *));
    fts_args[0] = path;
    fts_args[1] = NULL;

    if ((ftsp = fts_open(fts_args, FTS_NOCHDIR | FTS_PHYSICAL, NULL)) == NULL) {
        perror("fts_open");
        return NULL;
    }

    int is_root = 1;
    while ((p = fts_read(ftsp)) != NULL) {
        check_suspend(this_job);

        switch (p->fts_info) {
        case FTS_F:
            // It's a file, add its size to the total
            current_directory->bytes += p->fts_statp->st_size;
            current_directory->number_files += 1;
            break;
        case FTS_D:
            // It's a directory, traverse its contents

            // if it's the root directory, we shouldn't create it again
            if (!is_root) {
                current_directory = create_directory(p->fts_path, current_directory);
                current_directory->bytes += p->fts_statp->st_size;
            } else {
                is_root = 0;
            }

            chp = fts_children(ftsp, 0);
            if (chp == NULL) {
                break;
            }
            break;
        default:
            // if it's the root directory, we shouldn't go to its parent
            if (current_directory->parent != NULL) {
                current_directory->parent->bytes += current_directory->bytes;
                current_directory = current_directory->parent;
            }
            break;
        }
    }

    fts_close(ftsp);
    if (this_job->status == JOB_STATUS_REMOVED) {
        clean_job(this_job);
    } else
        this_job->status = JOB_STATUS_DONE;

    return 0;
}

int pause_job(struct Job *job_to_pause) {
    pthread_mutex_t *status_mutex = &job_to_pause->status_mutex;

    pthread_mutex_lock(status_mutex);
    if (job_to_pause->status != JOB_STATUS_IN_PROGRESS) {
        pthread_mutex_unlock(status_mutex);
        return -1;
    }
    job_to_pause->status = JOB_STATUS_PAUSED;
    pthread_mutex_unlock(status_mutex);

    return 0;
}

int resume_job(struct Job *job_to_resume) {
    pthread_mutex_t *status_mutex = &job_to_resume->status_mutex;
    pthread_cond_t *resume_cond = &job_to_resume->mutex_resume_cond;

    pthread_mutex_lock(status_mutex);
    if (job_to_resume->status != JOB_STATUS_PAUSED) {
        pthread_mutex_unlock(status_mutex);
        return -1;
    }
    job_to_resume->status = JOB_STATUS_IN_PROGRESS;
    pthread_cond_broadcast(resume_cond);
    pthread_mutex_unlock(status_mutex);

    return 0;
}

int remove_job(struct Job *job_to_remove) {
    pthread_mutex_t *status_mutex = &job_to_remove->status_mutex;

    pthread_mutex_lock(status_mutex);
    job_to_remove->status = JOB_STATUS_REMOVED;
    pthread_mutex_unlock(status_mutex);

    return 0;
}
