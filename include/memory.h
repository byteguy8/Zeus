#ifndef MEMORY_H
#define MEMORY_H

#include "types.h"
#include "bstr.h"
#include "lzarena.h"
#include "lzflist.h"
#include "dynarr.h"
#include "lzhtable.h"
#include <stddef.h>

#define MEMORY_KIBIBYTES(_count)((_count) * 1024)
#define MEMORY_MIBIBYTES(_count)(MEMORY_KIBIBYTES((_count) * 1024))

#define MEMORY_INIT_ALLOCATOR(_ctx, _alloc, _realloc, _dealloc, _allocator){ \
    (_allocator)->ctx = (_ctx);                                              \
    (_allocator)->alloc = (_alloc);                                          \
    (_allocator)->realloc = (_realloc);                                      \
    (_allocator)->dealloc = (_dealloc);                                      \
}

#define MEMORY_INIT_FAKE_ALLOCATOR(_alloc, _realloc, _dealloc, _args, _allocator, _fake_allocator){ \
    (_fake_allocator)->ctx = (_allocator)->ctx;                                                     \
    (_fake_allocator)->alloc = (_alloc);                                                            \
    (_fake_allocator)->realloc = (_realloc);                                                        \
    (_fake_allocator)->dealloc = (_dealloc);                                                        \
    (_fake_allocator)->args = (_args);                                                              \
}

#define MEMORY_ALLOC(type, count, allocator) ((type *)((allocator)->alloc(sizeof(type) * count, ((allocator)->ctx))))
#define MEMORY_REALLOC(type, old_count, new_count, ptr, allocator)((type *)((allocator)->realloc(ptr, (sizeof(type) * old_count), (sizeof(type) * new_count), (allocator)->ctx)))
#define MEMORY_DEALLOC(type, count, ptr, allocator) ((allocator)->dealloc((ptr), (sizeof(type) * count), (allocator)->ctx))

int memory_init();
void memory_deinit();
Allocator *memory_allocator();
Allocator *memory_create_arena_allocator(LZArena **out_arena);
void memory_destroy_arena_allocator(Allocator *allocator);

void *memory_alloc(size_t size);
void *memory_realloc(void *ptr, size_t size);
void memory_dealloc(void *ptr);

#endif
