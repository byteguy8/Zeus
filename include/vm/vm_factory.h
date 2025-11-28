#ifndef VM_FACTORY_H
#define VM_FACTORY_H

#include "essentials/memory.h"

#include "native_fn.h"
#include "fn.h"
#include "native_module.h"
#include "obj.h"
#include "value.h"
#include "vm/module.h"

#include <stdint.h>

NativeFn *vm_factory_native_fn_create(
    const Allocator *allocator,
    uint8_t core,
    const char *name,
    uint8_t arity,
    RawNativeFn raw_native
);
void vm_factory_native_fn_destroy(NativeFn *native_fn);

Fn *vm_factory_fn_create(const Allocator *allocator, const char *name, uint8_t arity);
void vm_factory_fn_destroy(Fn *fn);

SubModule *vm_factory_submodule_create(const Allocator *allocator);
void vm_factory_submodule_destroy(SubModule *submodule);

Module *vm_factory_module_create(const Allocator *allocator, const char *name, const char *pathname);
void vm_factory_module_destroy(Module *module);

int vm_factory_module_add_fn(Module *module, Fn *fn, size_t *out_idx);
int vm_factory_module_add_closure(Module *module, MetaClosure *closure, size_t *out_idx);
int vm_factory_module_add_module(Module *target_module, Module *module);

int vm_factory_module_globals_add_obj(
	Module *module,
	Obj *obj,
	const char *name,
	GlobalValueAccessType access_type
);

NativeModule *vm_factory_native_module_create(const Allocator *allocator, const char *name);
void vm_factory_native_module_destroy(NativeModule *module);
int vm_factory_native_module_add_value(
    NativeModule *native_module,
    const char *name,
    Value value
);
int vm_factory_native_module_add_native_fn(
    NativeModule *module,
    const char *name,
    uint8_t arity,
    RawNativeFn raw_native
);

int vm_factory_native_fn_add_info(
    LZOHTable *natives,
    const Allocator *allocator,
    const char *name,
    uint8_t arity,
    RawNativeFn raw_native
);

FnObj *vm_factory_fn_obj_create(const Allocator *allocator, Fn *fn);
NativeFnObj *vm_factory_native_fn_obj_create(const Allocator *allocator, NativeFn *native_fn);
NativeModuleObj *vm_factory_native_module_obj_create(const Allocator *allocator, NativeModule *native_module);
ModuleObj *vm_factory_module_obj_create(const Allocator *allocator, Module *module);

#endif
