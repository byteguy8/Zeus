#ifndef MEMORY_H
#define MEMORY_H

#include "types.h"
#include "bstr.h"
#include "lzarena.h"
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

BStr *compile_bstr();
DynArr *compile_dynarr(size_t size);
DynArrPtr *compile_dynarr_ptr();
LZHTable *compile_lzhtable();
char *compile_clone_str(char *str);
char *compile_clone_str_range(size_t start, size_t len, char *str);

DynArr *runtime_dynarr(size_t size);
DynArrPtr *runtime_dynarr_ptr();
LZHTable *runtime_lzhtable();
char *runtime_clone_str(char *str);
char *runtime_clone_str_range(size_t start, size_t len, char *str);

//> NATIVE MODULE RELATED
NativeModule *runtime_native_module(char *name);
void runtime_add_native_fn_info(char *name, uint8_t arity, RawNativeFn raw_native, LZHTable *natives);
void runtime_add_native_fn(char *name, uint8_t arity, RawNativeFn raw_native, NativeModule *module);
//< NATIVE MODULE RELATED

Fn *runtime_fn(char *name, Module *module);
Module *runtime_module(char *name, char *filepath);
Module *runtime_clone_module(char *new_name, char *filepath, Module *module);

void *memory_alloc(size_t size);
void *memory_realloc(void *ptr, size_t size);
void *memory_dealloc(void *ptr);

#endif
