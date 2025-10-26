#ifndef MEMORY_H
#define MEMORY_H

#include "dynarr.h"
#include "lzarena.h"
#include "lzflist.h"
#include "types.h"

#include <stddef.h>

typedef struct allocator{
    void *ctx;
    void *(*alloc)(size_t size, void *ctx);
    void *(*realloc)(void *ptr, size_t old_size, size_t new_size, void *ctx);
    void (*dealloc)(void *ptr, size_t size, void *ctx);
    void *extra;
}Allocator;

typedef struct complex_context{
    void *arg0;
    void *arg1;
}ComplexContext;

#define MEMORY_KIBIBYTES(_count)((_count) * 1024)
#define MEMORY_MIBIBYTES(_count)(MEMORY_KIBIBYTES((_count) * 1024))

#define MEMORY_INIT_ALLOCATOR(_ctx, _alloc, _realloc, _dealloc, _allocator){ \
    (_allocator)->ctx     = (_ctx);                                          \
    (_allocator)->alloc   = (_alloc);                                        \
    (_allocator)->realloc = (_realloc);                                      \
    (_allocator)->dealloc = (_dealloc);                                      \
    (_allocator)->extra   = NULL;                                            \
}

#define MEMORY_ALLOC(allocator, type, count)                       ((type *)((allocator)->alloc(sizeof(type) * count, ((allocator)->ctx))))
#define MEMORY_REALLOC(allocator, type, old_count, new_count, ptr) ((type *)((allocator)->realloc(ptr, (sizeof(type) * old_count), (sizeof(type) * new_count), (allocator)->ctx)))
#define MEMORY_DEALLOC(allocator, type, count, ptr)                ((allocator)->dealloc((ptr), (sizeof(type) * count), (allocator)->ctx))

#define MEMORY_LZBSTR(_allocator)                                  (lzbstr_create((LZBStrAllocator *)(_allocator)))
#define MEMORY_LZARENA(_allocator)                                 (lzarena_create((LZArenaAllocator *)(_allocator)))

Allocator *memory_arena_allocator(Allocator *allocator, LZArena **out_arena);
void memory_destroy_arena_allocator(Allocator *allocator);

#endif
