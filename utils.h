#ifndef UTILS_HEADER
#define UTILS_HEADER

#include <stddef.h>

int min(int a, int b);

int max(int a, int b);

/**
 * malloc that exits on error
 */
void *da_malloc(size_t size);

#endif
