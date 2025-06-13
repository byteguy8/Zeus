#ifndef FACTORY_H
#define FACTORY_H

#include "obj.h"
#include "native_fn.h"
#include "fn.h"
#include "closure.h"
#include "types.h"
#include "native_fn.h"
#include "native_module.h"
#include "module.h"
#include <stddef.h>

char *factory_clone_raw_str(char *raw_str, Allocator *allocator);
char *factory_clone_raw_str_range(size_t start, size_t len, char *str, Allocator *allocator);
void factory_destroy_raw_str(char *raw_str, Allocator *allocator);

#define FACTORY_BSTR(_allocator)(bstr_create_empty((BStrAllocator *)(_allocator)))
#define FACTORY_DYNARR(_size, _allocator)(dynarr_create((_size), (DynArrAllocator *)(_allocator)))
#define FACTORY_DYNARR_TYPE(_type, _allocator)(dynarr_create(sizeof(_type), (DynArrAllocator *)(_allocator)))
#define FACTORY_DYNARR_PTR(_allocator)(dynarr_create(sizeof(uintptr_t), (DynArrAllocator *)(_allocator)))
#define FACTORY_LZHTABLE(_allocator)(lzhtable_create(16, (LZHTableAllocator *)(_allocator)))
#define FACTORY_LZHTABLE_LEN(_len, _allocator)(lzhtable_create((_len), (LZHTableAllocator *)(_allocator)))

NativeFn *factory_create_native_fn(uint8_t core, char *name, uint8_t arity, Value *target, RawNativeFn raw_native, Allocator *allocator);
void factory_destroy_native_fn(NativeFn *native_fn, Allocator *allocator);

void factory_add_native_fn(char *name, uint8_t arity, RawNativeFn raw_native, NativeModule *module, Allocator *allocator);
void factory_add_native_fn_info(char *name, uint8_t arity, RawNativeFn raw_native, LZHTable *natives, Allocator *allocator);
Fn *factory_create_fn(char *name, Module *module, Allocator *allocator);

OutValue *factory_out_values(uint8_t length, Allocator *allocator);
void factory_destroy_out_values(uint8_t length, OutValue *values, Allocator *allocator);

Closure *factory_closure(OutValue *values, MetaClosure *meta, Allocator *allocator);
void factory_destroy_closure(Closure *closure, Allocator *allocator);

NativeModule *factory_native_module(char *name, Allocator *allocator);
Module *factory_module(char *name, char *filepath, Allocator *allocator);
Module *factory_clone_module(char *new_name, char *filepath, Module *module, Allocator *allocator);

#endif