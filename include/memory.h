#ifndef MEMORY_H
#define MEMORY_H

#include "dynarr.h"
#include "lzhtable.h"
#include <stddef.h>

#define COMPILE_ARENA 0
#define RUNTIME_ARENA 1

int memory_init();
void memory_deinit();

void memory_report();
void memory_free_compile();

void *memory_arena_alloc(size_t size, int type);
#define A_COMPILE_ALLOC(size)(memory_arena_alloc(size, COMPILE_ARENA))
#define A_RUNTIME_ALLOC(size)(memory_arena_alloc(size, RUNTIME_ARENA))

DynArr *compile_dynarr(size_t size);
DynArrPtr *compile_dynarr_ptr();
LZHTable *compile_lzhtable();
char *compile_clone_str(char *str);

DynArr *runtime_dynarr(size_t size);
DynArrPtr *runtime_dynarr_ptr();
LZHTable *runtime_lzhtable();
char *runtime_clone_str(char *str);

#endif
