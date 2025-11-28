#include "vm_factory.h"

#include "essentials/memory.h"

#include "vm_types.h"
#include "obj.h"

NativeFn *vm_factory_native_fn_create(
    const Allocator *allocator,
    uint8_t core,
    const char *name,
    uint8_t arity,
    RawNativeFn raw_native
){
    char *cloned_name = memory_clone_cstr(allocator, name, NULL);
    NativeFn *native_fn = MEMORY_ALLOC(allocator, NativeFn, 1);

    if(!cloned_name || !native_fn){
        memory_destroy_cstr(allocator, cloned_name);
        MEMORY_DEALLOC(allocator, NativeFn, 1, native_fn);

        return NULL;
    }

    native_fn->core = core;
    native_fn->arity = arity;
    native_fn->name = cloned_name;
    native_fn->raw_fn = raw_native;
    native_fn->allocator = allocator;

    return native_fn;

}

void vm_factory_native_fn_destroy(NativeFn *native_fn){
    if(!native_fn){
        return;
    }

    const Allocator *allocator = native_fn->allocator;

    memory_destroy_cstr(allocator, native_fn->name);
    MEMORY_DEALLOC(allocator, NativeFn, 1, native_fn);
}

Fn *vm_factory_fn_create(const Allocator *allocator, const char *name, uint8_t arity){
    char *cloned_name = memory_clone_cstr(allocator, name, NULL);
    DynArr *chunks = MEMORY_DYNARR_TYPE(allocator, uint8_t);
    DynArr *iconsts = MEMORY_DYNARR_TYPE(allocator, int64_t);
    DynArr *fconsts = MEMORY_DYNARR_TYPE(allocator, double);
    DynArr *locations = MEMORY_DYNARR_TYPE(allocator, OPCodeLocation);
    Fn *fn = MEMORY_ALLOC(allocator, Fn, 1);

    MEMORY_CHECK(cloned_name);
    MEMORY_CHECK(chunks);
    MEMORY_CHECK(iconsts);
    MEMORY_CHECK(fconsts);
    MEMORY_CHECK(locations);
    MEMORY_CHECK(fn);

    *fn = (Fn){
        .arity = arity,
        .name = cloned_name,
        .chunks = chunks,
        .iconsts = iconsts,
        .fconsts = fconsts,
        .locations = locations,
        .module = NULL,
        .allocator = allocator
    };

    goto OK;

ERROR:
    memory_destroy_cstr(allocator, cloned_name);
    dynarr_destroy(chunks);
    dynarr_destroy(iconsts);
    dynarr_destroy(fconsts);
    dynarr_destroy(locations);
    MEMORY_DEALLOC(allocator, Fn, 1, fn);

    return NULL;

OK:
    return fn;
}

void vm_factory_fn_destroy(Fn *fn){
    if(!fn){
        return;
    }

    const Allocator *allocator = fn->allocator;

    memory_destroy_cstr(allocator, fn->name);
    dynarr_destroy(fn->chunks);
    dynarr_destroy(fn->iconsts);
    dynarr_destroy(fn->fconsts);
    dynarr_destroy(fn->locations);
    MEMORY_DEALLOC(allocator, Fn, 1, fn);
}

SubModule *vm_factory_submodule_create(const Allocator *allocator){
    LZOHTable *globals = MEMORY_LZOHTABLE_LEN(allocator, 64);
    DynArr *static_strs = MEMORY_DYNARR_TYPE(allocator, VmStaticStr);
    DynArr *symbols = MEMORY_DYNARR_TYPE(allocator, SubModuleSymbol);
    SubModule *submodule = MEMORY_ALLOC(allocator, SubModule, 1);

    MEMORY_CHECK(globals);
    MEMORY_CHECK(static_strs);
    MEMORY_CHECK(symbols);
    MEMORY_CHECK(submodule);

    *submodule = (SubModule){
        .resolved = 0,
        .globals = globals,
        .static_strs = static_strs,
        .symbols = symbols,
        .allocator = allocator
    };

    goto OK;

ERROR:
    LZOHTABLE_DESTROY(globals);
    dynarr_destroy(static_strs);
    dynarr_destroy(symbols);
    MEMORY_DEALLOC(allocator, SubModule, 1, submodule);

    return NULL;

OK:
    return submodule;
}

void vm_factory_submodule_destroy(SubModule *submodule){
    if(!submodule){
        return;
    }

    LZOHTABLE_DESTROY(submodule->globals);
    dynarr_destroy(submodule->static_strs);
    dynarr_destroy(submodule->symbols);
    MEMORY_DEALLOC(submodule->allocator, SubModule, 1, submodule);
}

Module *vm_factory_module_create(const Allocator *allocator, const char *name, const char *pathname){
    char *cloned_name = memory_clone_cstr(allocator, name, NULL);
    char *cloned_pathname = memory_clone_cstr(allocator, pathname, NULL);
    SubModule *submodule = vm_factory_submodule_create(allocator);
    Module *module = MEMORY_ALLOC(allocator, Module, 1);

    MEMORY_CHECK(cloned_name);
    MEMORY_CHECK(cloned_pathname);
    MEMORY_CHECK(submodule);
    MEMORY_CHECK(module);

    *module = (Module){
        .original = 1,
        .name = cloned_name,
        .pathname = cloned_pathname,
        .submodule = submodule,
        .prev = NULL
    };

    goto OK;

ERROR:
    memory_destroy_cstr(allocator, cloned_name);
    memory_destroy_cstr(allocator, cloned_pathname);
    vm_factory_submodule_destroy(submodule);
    MEMORY_DEALLOC(allocator, Module, 1, module);

    return NULL;

OK:
    return module;
}

void vm_factory_module_destroy(Module *module){
    if(!module){
        return;
    }

    const Allocator *allocator = module->submodule->allocator;

    memory_destroy_cstr(allocator, module->name);
    memory_destroy_cstr(allocator, module->pathname);
    vm_factory_submodule_destroy(module->submodule);
    MEMORY_DEALLOC(allocator, Module, 1, module);
}

int vm_factory_module_add_fn(Module *module, Fn *fn, size_t *out_idx){
    SubModule *submodule = module->submodule;
    DynArr *symbols = submodule->symbols;
    SubModuleSymbol symbol = {
        .type = FUNCTION_SUBMODULE_SYM_TYPE,
        .value = fn
    };

    if(dynarr_insert(symbols, &symbol)){
        return 1;
    }

    fn->module = module;

    if(out_idx){
        *out_idx = dynarr_len(symbols) - 1;
    }

    return 0;
}

int vm_factory_module_add_closure(Module *module, MetaClosure *closure, size_t *out_idx){
    SubModule *submodule = module->submodule;
    DynArr *symbols = submodule->symbols;
    SubModuleSymbol symbol = {
        .type = CLOSURE_SUBMODULE_SYM_TYPE,
        .value = closure
    };

    if(dynarr_insert(symbols, &symbol)){
        return 1;
    }

    if(out_idx){
        *out_idx = dynarr_len(symbols) - 1;
    }

    return 0;
}

int vm_factory_module_add_module(Module *target_module, Module *module){
    SubModule *submodule = target_module->submodule;
    DynArr *symbols = submodule->symbols;

    SubModuleSymbol symbol = {
        .type = MODULE_SUBMODULE_SYM_TYPE,
        .value = module
    };

    if(dynarr_insert(symbols, &symbol)){
        return 1;
    }

    return 0;
}

int vm_factory_module_globals_add_obj(
	Module *module,
	Obj *obj,
	const char *name,
	GlobalValueAccessType access_type
){
	return lzohtable_put_ckv(
        strlen(name),
        name,
        sizeof(GlobalValue),
        &((GlobalValue){
            .access = PRIVATE_GLOVAL_VALUE_TYPE,
            .value = {
                .type = OBJ_VALUE_TYPE,
                .content.obj_val = obj
            }
        }),
        module->submodule->globals,
        NULL
    );
}

NativeModule *vm_factory_native_module_create(const Allocator *allocator, const char *name){
    char *module_name = memory_clone_cstr(allocator, name, NULL);
	LZOHTable *symbols = MEMORY_LZOHTABLE(allocator);
	NativeModule *module = MEMORY_ALLOC(allocator, NativeModule, 1);

    if(!module_name || !symbols || !module){
        memory_destroy_cstr(allocator, module_name);
        LZOHTABLE_DESTROY(symbols);
        MEMORY_DEALLOC(allocator, NativeModule, 1, module);

        return NULL;
    }

	module->name = module_name;
	module->symbols = symbols;
    module->allocator = allocator;

	return module;
}

void vm_factory_native_module_destroy(NativeModule *module){
    if(!module){
        return;
    }

    const Allocator *allocator = module->allocator;

    memory_destroy_cstr(allocator, module->name);
    LZOHTABLE_DESTROY(module->symbols);
    MEMORY_DEALLOC(allocator, NativeModule, 1, module);
}

inline int vm_factory_native_module_add_value(
    NativeModule *native_module,
    const char *name,
    Value value
){
    if(lzohtable_put_ckv(
        strlen(name),
        name,
        sizeof(Value),
        (const void *)&value,
        native_module->symbols,
        NULL
    )){
        return 1;
    }

    return 0;
}

int vm_factory_native_module_add_native_fn(
    NativeModule *module,
    const char *name,
    uint8_t arity,
    RawNativeFn raw_native
){
    const Allocator *allocator = module->allocator;

    NativeFn *native_fn = vm_factory_native_fn_create(allocator, 1, name, arity, raw_native);
    NativeFnObj *native_fn_obj = MEMORY_ALLOC(allocator, NativeFnObj, 1);

    if(!native_fn || !native_fn_obj){
        vm_factory_native_fn_destroy(native_fn);
        MEMORY_DEALLOC(allocator, NativeFnObj, 1, native_fn_obj);

        return 1;
    }

    Obj *obj = (Obj *)native_fn_obj;

    obj->type = NATIVE_FN_OBJ_TYPE;
    obj->marked = 0;
    obj->color = TRANSPARENT_OBJ_COLOR;
    obj->prev = NULL;
    obj->next = NULL;
    obj->list = NULL;

    native_fn_obj->target = (Value){0};
    native_fn_obj->native_fn = native_fn;

    Value value = {
        .type = OBJ_VALUE_TYPE,
        .content.obj_val = native_fn_obj
    };

    if(vm_factory_native_module_add_value(module, name, value)){
        vm_factory_native_fn_destroy(native_fn);
        MEMORY_DEALLOC(allocator, NativeFnObj, 1, native_fn_obj);

        return 1;
    }

    return 0;

}

int vm_factory_native_fn_add_info(
    LZOHTable *natives,
    const Allocator *allocator,
    const char *name,
    uint8_t arity,
    RawNativeFn raw_native
){
    size_t name_len;
    char *cloned_name = memory_clone_cstr(allocator, name, &name_len);
    NativeFn *native_fn = MEMORY_ALLOC(allocator, NativeFn, 1);

    if(!cloned_name || !native_fn){
        memory_destroy_cstr(allocator, cloned_name);
        MEMORY_DEALLOC(allocator, NativeFn, 1, native_fn);

        return 1;
    }

    native_fn->core = 0;
    native_fn->arity = arity;
    native_fn->name = cloned_name;
    native_fn->raw_fn = raw_native;

    return lzohtable_put_ckv(name_len, name, sizeof(NativeFn), native_fn, natives, NULL);

}

FnObj *vm_factory_fn_obj_create(const Allocator *allocator, Fn *fn){
    FnObj *fn_obj = MEMORY_ALLOC(allocator, FnObj, 1);
    Obj *obj = (Obj *)fn_obj;

    if(!fn_obj){
        return NULL;
    }

    obj->type = FN_OBJ_TYPE;
    obj->marked = 0;
    obj->color = TRANSPARENT_OBJ_COLOR;
    obj->prev = NULL;
    obj->next = NULL;
    obj->list = NULL;
    fn_obj->fn = fn;

    return fn_obj;
}

NativeFnObj *vm_factory_native_fn_obj_create(const Allocator *allocator, NativeFn *native_fn){
    NativeFnObj *native_fn_obj = MEMORY_ALLOC(allocator, NativeFnObj, 1);
    Obj *obj = (Obj *)native_fn_obj;

    if(!native_fn_obj){
        return NULL;
    }

    obj->type = NATIVE_FN_OBJ_TYPE;
    obj->marked = 0;
    obj->color = TRANSPARENT_OBJ_COLOR;
    obj->prev = NULL;
    obj->next = NULL;
    obj->list = NULL;
    native_fn_obj->target = (Value){0};
    native_fn_obj->native_fn = native_fn;

    return native_fn_obj;
}

NativeModuleObj *vm_factory_native_module_obj_create(const Allocator *allocator, NativeModule *native_module){
	NativeModuleObj *native_module_obj = MEMORY_ALLOC(allocator, NativeModuleObj, 1);
    Obj *obj = (Obj *)native_module_obj;

    if(!native_module_obj){
        return NULL;
    }

    obj->type = NATIVE_MODULE_OBJ_TYPE;
    obj->marked = 0;
    obj->color = TRANSPARENT_OBJ_COLOR;
    obj->prev = NULL;
    obj->next = NULL;
    obj->list = NULL;
    native_module_obj->native_module = native_module;

    return native_module_obj;
}

ModuleObj *vm_factory_module_obj_create(const Allocator *allocator, Module *module){
    ModuleObj *module_obj = MEMORY_ALLOC(allocator, ModuleObj, 1);
    Obj *obj = (Obj *)module_obj;

    if(!module_obj){
        return NULL;
    }

    obj->type = MODULE_OBJ_TYPE;
    obj->marked = 0;
    obj->color = TRANSPARENT_OBJ_COLOR;
    obj->prev = NULL;
    obj->next = NULL;
    obj->list = NULL;
    module_obj->module = module;

    return module_obj;
}
