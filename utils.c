#include "utils.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

int min(int a, int b) { return a < b ? a : b; }

int max(int a, int b) { return a > b ? a : b; }

/**
 * malloc that exits on error
 */
void *da_malloc(size_t size) {
    void *alloc_len = malloc(size);

    if (alloc_len == NULL && size > 0) {
        // malloc failed
        perror("Failed alloc");
        exit(1);
    }

    return alloc_len;
}
