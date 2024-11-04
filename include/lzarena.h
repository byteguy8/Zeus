#ifndef LZARENA_H
#define LZARENA_H

#include <stddef.h>

#define LZREGION_DEFAULT_ALIGNMENT 8

typedef struct lzregion{
	char *buff;
	size_t used;
	size_t buff_size;
}LZRegion;

#define CONCAT(a, b) a ## b
#define LZREGION_STACK(buff_size, name) \
	char CONCAT(name, _buff)[buff_size]; \
	LZRegion name = {0}; \
    lzregion_create_raw(buff_size, CONCAT(name, _buff), &name);

void lzregion_create_raw(size_t buff_size, char *buff, LZRegion *region);
LZRegion *lzregion_create(size_t buff_size);
void lzregion_destroy(LZRegion *region);
void *lzregion_alloc_align(size_t size, size_t alignment, LZRegion *region);
#define LZREGION_ALLOC(size, region) (lzregion_alloc_align(size, LZREGION_DEFAULT_ALIGNMENT, region))

#endif
