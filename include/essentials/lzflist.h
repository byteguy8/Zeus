#ifndef LZFLIST_H
#define LZFLIST_H

#include <stddef.h>

#define LZFLIST_KIBIBYTES(_qty)((size_t)((_qty) * 1024))
#define LZFLIST_MEBIBYTES(_qty)((size_t)(LZFLIST_KIBIBYTES(_qty) * 1024))
#define LZFLIST_GIBIBYTES(_qty)((size_t)(LZFLIST_MEBIBYTES(_qty) * 1024))

#define LZFLIST_BACKEND_MALLOC 0
#define LZFLIST_BACKEND_MMAP 1
#define LZFLIST_BACKEND_VIRTUALALLOC 2

#ifndef LZFLIST_BACKEND
    #ifdef _WIN32
        #define LZFLIST_BACKEND LZFLIST_BACKEND_VIRTUALALLOC
    #elif __linux__
        #define LZFLIST_BACKEND LZFLIST_BACKEND_MMAP
    #else
        #define LZFLIST_BACKEND LZFLIST_BACKEND_MALLOC
    #endif
#endif

#define LZFLIST_DEFAULT_ALIGNMENT 16

typedef struct lzflist_allocator{
    void *ctx;
    void *(*alloc)(size_t size, void *ctx);
    void *(*realloc)(void *ptr, size_t old_size, size_t new_size, void *ctx);
    void (*dealloc)(void *ptr, size_t size, void *ctx);
}LZFListAllocator;

typedef struct lzflregion{
    size_t used_bytes;
    size_t consumed_bytes;
    size_t subregion_size;
    void *offset;
    void *subregion;
    struct lzflregion *prev;
    struct lzflregion *next;
}LZFLRegion;

typedef struct lzflregion_list{
    size_t len;
    struct lzflregion *head;
    struct lzflregion *tail;
}LZFLRegionList;

typedef struct lzflheader{
    size_t magic;
    size_t size;
    char used;
    struct lzflheader *prev;
    struct lzflheader *next;
    struct lzflregion *region;
}LZFLHeader;

typedef struct lzflarea_list{
    size_t len;
    struct lzflheader *head;
    struct lzflheader *tail;
}LZFLAreaList;

typedef struct lzflist{
    LZFLRegionList regions;
    LZFLAreaList free_areas;
    LZFLRegion *current_region;
    LZFListAllocator *allocator;
}LZFList;

LZFList *lzflist_create(LZFListAllocator *allocator);
void lzflist_destroy(LZFList *list);

size_t lzflist_ptr_size(void *ptr);
int lzflist_prealloc(size_t size, LZFList *list);

void *lzflist_alloc(size_t size, LZFList *list);
void *lzflist_calloc(size_t size, LZFList *list);
void *lzflist_realloc(void *ptr, size_t new_size, LZFList *list);
void lzflist_dealloc(void *ptr, LZFList *list);

#endif