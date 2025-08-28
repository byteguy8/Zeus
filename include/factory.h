#ifndef FACTORY_H
#define FACTORY_H

#include "lzbstr.h"
#include "lzohtable.h"
#include "obj.h"
#include "native_fn.h"
#include "fn.h"
#include "closure.h"
#include "types.h"
#include "native_fn.h"
#include "native_module.h"
#include "module.h"
#include <stddef.h>

#define FACTORY_BSTR(_allocator)(bstr_create_empty((BStrAllocator *)(_allocator)))
#define FACTORY_LZBSTR(_allocator)(lzbstr_create((LZBStrAllocator *)(_allocator)))
#define FACTORY_DYNARR(_size, _allocator)(dynarr_create((_size), (DynArrAllocator *)(_allocator)))
#define FACTORY_DYNARR_TYPE(_type, _allocator)(dynarr_create(sizeof(_type), (DynArrAllocator *)(_allocator)))
#define FACTORY_DYNARR_PTR(_allocator)(dynarr_create(sizeof(uintptr_t), (DynArrAllocator *)(_allocator)))
#define FACTORY_LZHTABLE(_allocator)(lzhtable_create(16, (LZHTableAllocator *)(_allocator)))
#define FACTORY_LZHTABLE_LEN(_len, _allocator)(lzhtable_create((_len), (LZHTableAllocator *)(_allocator)))
#define FACTORY_LZOHTABLE(_allocator)(lzohtable_create(16, 0.5f, (LZOHTableAllocator *)(_allocator)))
#define FACTORY_LZOHTABLE_LEN(_len, _allocator)(lzohtable_create((_len), 0.5f, (LZOHTableAllocator *)(_allocator)))

char *factory_clone_raw_str(char *raw_str, Allocator *allocator, size_t *out_len);
void factory_destroy_raw_str(char *raw_str, Allocator *allocator);

NativeFn *factory_create_native_fn(uint8_t core, char *name, uint8_t arity, Value *target, RawNativeFn raw_native, Allocator *allocator);
void factory_destroy_native_fn(NativeFn *native_fn);

int factory_add_native_fn(char *name, uint8_t arity, RawNativeFn raw_native, NativeModule *module, Allocator *allocator);
int factory_add_native_fn_info_n(char *name, uint8_t arity, RawNativeFn raw_native, LZOHTable *natives, Allocator *allocator);

Fn *factory_create_fn(char *name, Module *module, Allocator *allocator);
void factory_destroy_fn(Fn *fn);

NativeModule *factory_create_native_module(char *name, Allocator *allocator);
void factory_destroy_native_module(NativeModule *native_module);

Module *factory_create_module(char *name, char *filepath, Allocator *allocator);
Module *factory_create_clone_module(char *new_name, char *filepath, Module *module, Allocator *allocator);
void factory_destroy_module(Module *module);

#endif