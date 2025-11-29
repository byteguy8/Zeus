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

typedef struct lzflist LZFList;

LZFList *lzflist_create(LZFListAllocator *allocator);
void lzflist_destroy(LZFList *list);

size_t lzflist_ptr_size(const void *ptr);
int lzflist_prealloc(LZFList *list, size_t size);
size_t lzflist_regions_count(const LZFList *list);
size_t lzflist_free_areas_count(const LZFList *list);
size_t lzflist_allocable_used_bytes(const LZFList *list);
size_t lzflist_allocable_reserved_bytes(const LZFList *list);
size_t lzflist_allocable_free_bytes(const LZFList *list);

void *lzflist_alloc(LZFList *list, size_t size);
void *lzflist_calloc(LZFList *list, size_t size);
void *lzflist_realloc(LZFList *list, void *ptr, size_t new_size);
void lzflist_dealloc(LZFList *list, void *ptr);

#endif