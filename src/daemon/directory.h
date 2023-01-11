#ifndef DIRECTORY_HEADER
#define DIRECTORY_HEADER

#include <stdint.h>

struct DirList {
    struct Directory *dir;
    struct DirList *next;
};

/**
 * Frees all nodes and their dirs.
 */
void dirlist_destroy(struct DirList *head);

/**
 * Push dir to the front of the list and return the new head.
 */
struct DirList *dirlist_push_front(struct DirList *head, struct Directory *dir);

struct Directory {
    char *path;
    struct Directory *parent;
    struct DirList *subdirs;
    int64_t bytes;
};

/**
 * Initializes a Directory for the given path.
 */
int directory_init(struct Directory *dir, const char *path);

void directory_destroy(struct Directory *dir);

#endif
