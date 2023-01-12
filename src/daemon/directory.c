#include "directory.h"
#include <common/utils.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

struct DirList *dirlist_push_front(struct DirList *head, struct Directory *dir) {
    struct DirList *new_node = da_malloc(sizeof(*new_node));
    new_node->dir = dir;
    new_node->next = NULL;

    if (head == NULL) {
        head = new_node;
    } else {
        new_node->next = head;
    }

    return new_node;
}

void dirlist_destroy(struct DirList *head) {
    while (head != NULL) {
        directory_destroy(head->dir);
        free(head->dir);
        struct DirList *next = head->next;
        free(head);
        head = next;
    }
}

int directory_init(struct Directory *dir, const char *path) {
    dir->path = da_malloc((strlen(path) + 1) * sizeof(*dir->path));
    strcpy(dir->path, path);
    dir->parent = NULL;
    dir->subdirs = NULL;
    dir->bytes = 0;

    return 0;
}

void directory_destroy(struct Directory *dir) {
    if (dir == NULL)
        return;

    free(dir->path);
    dirlist_destroy(dir->subdirs);
}

/**
 * Writes dir to buf and returns a pointer to continue writing.
 */
char *serialize_helper(struct Directory *root, char *buf) {
    if (root == NULL)
        return buf;

    int64_t path_len = strlen(root->path);
    char *ptr = buf;

    memcpy(ptr, &path_len, sizeof(path_len));
    ptr += sizeof(path_len);

    memcpy(ptr, root->path, path_len);
    ptr += path_len;

    memcpy(ptr, &root->bytes, sizeof(root->bytes));
    ptr += sizeof(root->bytes);

    struct DirList *head = root->subdirs;
    while (head != NULL) {
        ptr = serialize_helper(head->dir, ptr);
        head = head->next;
    }

    return ptr;
}

void directory_serialize(struct Directory *root, char *buf) {
    serialize_helper(root, buf);
}
