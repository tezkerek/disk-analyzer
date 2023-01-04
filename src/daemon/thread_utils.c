#include <daemon/thread_utils.h>
#include <errno.h>    // nsfw related
#include <ftw.h>      // nsfw related
#include <libgen.h>   // nsfw related
#include <sched.h>    // nsfw related
#include <stdio.h>    // perror
#include <stdlib.h>   // malloc, free etc.
#include <string.h>   // strncpy
#include <sys/stat.h> // nsfw related
#include <unistd.h>   // sockets

struct Job *find_job_by_id(int64_t id) {
    if (id >= 0 && id < job_count)
        return jobs[id];
    return NULL;
}

void check_suspend(struct Job *job_to_check) {
    pthread_mutex_t status_mutex = job_to_check->status_mutex;
    int8_t status = job_to_check->status;
    pthread_cond_t resume_cond = job_to_check->mutex_resume_cond;

    pthread_mutex_lock(&status_mutex);
    while (status == JOB_STATUS_PAUSED)
        pthread_cond_wait(&resume_cond, &status_mutex);
    pthread_mutex_unlock(&status_mutex);
}

static int build_tree(const char *fpath,
                      const struct stat *sb,
                      int typeflag,
                      struct FTW *ftwbuf) {

    if (!strcmp(fpath, last[job_count]->path))
        return 0;

    char *aux = malloc(strlen(fpath) * sizeof(char) + 1);
    strcpy(aux, fpath);
    strcpy(aux, dirname(aux));
    while (strcmp(aux, last[job_count]->path)) {
        last[job_count]->parent->bytes += last[job_count]->bytes;
        last[job_count] = last[job_count]->parent;
    }

    if (typeflag == FTW_D) {
        struct Directory *current = malloc(sizeof(*current));
        current->parent = last[job_count];
        last[job_count]->bytes += sb->st_size;
        current->number_subdir = 0;
        current->path = malloc(strlen(fpath) * sizeof(char) + 1);
        strcpy(current->path, fpath);
        current->bytes = 0;
        current->number_files = 0;
        last[job_count]->children[last[job_count]->number_subdir] = current;
        last[job_count]->number_subdir++;
        last[job_count] = current;
    } else {
        last[job_count]->number_files++;
        last[job_count]->bytes += sb->st_size;
    }

    return 0;
}

void *traverse(void *path) {

    static int nopenfd = 20; // ne gandim cat vrem sa punem
    char *p = path;

    struct Directory *root = malloc(sizeof(*root));
    root->path = malloc(strlen(p) * sizeof(char) + 1);
    strcpy(root->path, p);
    root->parent = NULL;
    root->bytes = root->number_files = root->number_subdir = 0;
    last[job_count] = root;

    if (nftw(p, build_tree, nopenfd, FTW_PHYS) == -1) {
        perror("nsfw"); // modif
    }

    while (last[job_count]->parent != NULL) {
        last[job_count]->parent->bytes += last[job_count]->bytes;
        last[job_count] = last[job_count]->parent;
    }

    return 0;
}