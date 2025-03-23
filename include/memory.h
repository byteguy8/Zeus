#ifndef MEMORY_H
#define MEMORY_H

#include "rtypes.h"
#include "types.h"
#include "bstr.h"
#include "lzarena.h"
#include "lzflist.h"
#include "dynarr.h"
#include "lzhtable.h"
#include <stddef.h>

#define MEMORY_ALLOC(type, count, allocator) ((type *)((allocator)->alloc(sizeof(type) * count, ((allocator)->ctx))))
#define MEMORY_REALLOC(type, old_count, new_count, ptr, allocator)((type *)((allocator)->realloc(ptr, (sizeof(type) * old_count), (sizeof(type) * new_count), (allocator)->ctx)))
#define MEMORY_DEALLOC(type, count, ptr, allocator) ((allocator)->dealloc((ptr), (sizeof(type) * count), (allocator)->ctx))

int memory_init();
void memory_deinit();
Allocator *memory_allocator();
Allocator *memory_arena_allocator(LZArena **out_arena);

void *memory_alloc(size_t size);
void *memory_realloc(void *ptr, size_t size);
void memory_dealloc(void *ptr);

#endif