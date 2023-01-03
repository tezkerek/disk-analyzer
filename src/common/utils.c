#include <common/utils.h>
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

void bytearray_init(struct ByteArray *arr, int64_t len) {
    arr->len = len;
    arr->bytes = da_malloc(len * sizeof(*arr->bytes));
}

void bytearray_destroy(struct ByteArray *arr) {
    free(arr->bytes);
    arr->bytes = NULL;
}
