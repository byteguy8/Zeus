#ifndef MEMORY_H
#define MEMORY_H

#include "dynarr.h"
#include "lzhtable.h"
#include <stddef.h>

int memory_init();
void memory_deinit();
void *memory_alloc(size_t size);
DynArr *memory_dynarr(size_t size);
DynArrPtr *memory_dynarr_ptr();
LZHTable *memory_lzhtable();
char *memory_clone_str(char *str);

#endif
