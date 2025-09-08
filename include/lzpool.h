#ifndef LZPOOL_H
#define LZPOOL_H

#include <stddef.h>

#define LZPOOL_DEFAULT_ALIGNMENT 16

typedef struct lzpool_allocator{
    void *ctx;
    void *(*alloc)(size_t size, void *ctx);
    void *(*realloc)(void *ptr, size_t old_size, size_t new_size, void *ctx);
    void (*dealloc)(void *ptr, size_t size, void *ctx);
}LZPoolAllocator;

typedef struct lzsubpool{
    size_t slots_used;
    size_t slots_count;
    void *slots;
    struct lzsubpool *prev;
    struct lzsubpool *next;
}LZSubPool;

typedef struct lzpool_header{
    size_t magic;
    char used;
    void *pool;
    LZSubPool *subpool;
    struct lzpool_header *prev;
    struct lzpool_header *next;
}LZPoolHeader;

typedef struct lzpool_header_list{
    size_t len;
    LZPoolHeader *head;
    LZPoolHeader *tail;
}LZPoolHeaderList;

typedef struct lzsubpool_list{
    size_t len;
    LZSubPool *head;
    LZSubPool *tail;
}LZSubPoolList;

typedef struct lzpool{
    size_t header_size;
    size_t slot_size;
    LZPoolHeaderList slots;
    LZSubPoolList subpools;
    LZPoolAllocator *allocator;
}LZPool;

void lzpool_init(size_t slot_size, LZPoolAllocator *allocator, LZPool *pool);
void lzpool_destroy_deinit(LZPool *pool);

LZPool *lzpool_create(size_t slot_size, LZPoolAllocator *allocator);
void lzpool_destroy(LZPool *pool);

int lzpool_prealloc(size_t slots_count, LZPool *pool);

void *lzpool_alloc(LZPool *pool);
void *lzpool_alloc_x(size_t slots_count, LZPool *pool);
void lzpool_dealloc(void *ptr);
void lzpool_dealloc_release(void *ptr);

#endif