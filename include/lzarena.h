#ifndef LZARENA_H
#define LZARENA_H

#include <stddef.h>

#ifndef LZREGION_DEFAULT_ALIGNMENT
#define LZREGION_DEFAULT_ALIGNMENT 8
#endif

#ifndef LZREGION_DEFAULT_LENGTH
#define LZREGION_DEFAULT_LENGTH 4096
#endif

#define LZREGION_SIZE (sizeof(LZRegion))
#define LZARENA_SIZE (sizeof(LZArena))

typedef struct lzregion{
	size_t offset;
    size_t len;
    char *buff;
    struct lzregion *next;
}LZRegion;

typedef struct lzarena
{
    size_t len;
    size_t used;
    LZRegion *region_stack;
}LZArena;

#define CONCAT(a, b) a ## b
#define LZREGION_STACK(len, name) \
	char CONCAT(name, _buff)[LZREGION_SIZE + len]; \
	LZRegion name = {0}; \
    lzregion_create_raw(len, CONCAT(name, _buff), &name);

LZRegion *lzregion_create_raw(size_t len, char *buff);
LZRegion *lzregion_create(size_t len);
void lzregion_destroy(LZRegion *region);

void *lzregion_align_alloc(size_t size, size_t alignment, LZRegion *region);
#define LZREGION_ALLOC(size, region) (lzregion_alloc_align(size, LZREGION_DEFAULT_ALIGNMENT, region))

void *lzregion_align_realloc(
    void *ptr,
    size_t new_size,
    size_t old_size,
    size_t alignment,
    LZRegion *region
);
#define LZREGION_REALLOC(ptr, new_size, old_size, region) \
    (lzregion_align_realloc(ptr, new_size, old_size, LZREGION_DEFAULT_ALIGNMENT, region))

LZArena *lzarena_create();
void lzarena_destroy(LZArena *arena);

#define LZARENA_LEN(a) (a->len)
#define LZARENA_FREE_BYTES(a) (a->len - a->used)

void *lzarena_align_alloc(size_t size, size_t alignment, LZArena *arena);
#define LZARENA_ALLOC(size, arena) (lzarena_align_alloc(size, LZREGION_DEFAULT_ALIGNMENT, arena))

void *lzarena_align_realloc(void *ptr, size_t new_size, size_t old_size, size_t alignment, LZArena *arena);
#define LZARENA_REALLOC(ptr, new_size, old_size, arena) (lzarena_align_realloc(ptr, new_size, old_size, LZREGION_DEFAULT_ALIGNMENT, arena))

#endif
