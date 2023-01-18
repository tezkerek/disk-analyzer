#ifndef UTILS_HEADER
#define UTILS_HEADER

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

int min(int a, int b);

int max(int a, int b);

/**
 * Print `str` `count` times to the stream.
 */
int fputs_repeated(const char *restrict str, FILE *restrict stream, int count);

/**
 * malloc that exits on error
 */
void *da_malloc(size_t size);

struct ByteArray {
    int64_t len;
    char *bytes;
};

void bytearray_init(struct ByteArray *arr, int64_t len);

void bytearray_destroy(struct ByteArray *arr);

#endif
