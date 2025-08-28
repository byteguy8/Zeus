#include "factory.h"
#include "memory.h"

typedef struct factory_str{
    char *buff;
    size_t len;
    Allocator *allocator;
}FactoryStr;

char *factory_clone_raw_str(char *raw_str, Allocator *allocator, size_t *out_len){
    size_t len = strlen(raw_str);
    char *cloned_raw_str = MEMORY_ALLOC(char, len + 1, allocator);

    if(!cloned_raw_str){
        return NULL;
    }

    memcpy(cloned_raw_str, raw_str, len);
    cloned_raw_str[len] = '\0';

    if(out_len){
        *out_len = len;
    }

    return cloned_raw_str;
}

void factory_destroy_raw_str(char *raw_str, Allocator *allocator){
    if(!raw_str){
        return;
    }

    size_t raw_str_len = strlen(raw_str);
    MEMORY_DEALLOC(char, raw_str_len + 1, raw_str, allocator);
}

NativeFn *factory_create_native_fn(uint8_t core, char *name, uint8_t arity, Value *target, RawNativeFn raw_native, Allocator *allocator){
    char *cloned_name = factory_clone_raw_str(name, allocator, NULL);
    NativeFn *native_fn = MEMORY_ALLOC(NativeFn, 1, allocator);

    if(!cloned_name || !native_fn){
        factory_destroy_raw_str(cloned_name, allocator);
        MEMORY_DEALLOC(NativeFn, 1, native_fn, allocator);

        return NULL;
    }

    native_fn->core = core;
    native_fn->arity = arity;
    native_fn->name = cloned_name;
    native_fn->raw_fn = raw_native;
    native_fn->allocator = allocator;

    return native_fn;
}

void factory_destroy_native_fn(NativeFn *native_fn){
    if(!native_fn){
        return;
    }

    Allocator *allocator = native_fn->allocator;

    factory_destroy_raw_str(native_fn->name, allocator);
    MEMORY_DEALLOC(NativeFn, 1, native_fn, allocator);
}

int factory_add_native_fn(char *name, uint8_t arity, RawNativeFn raw_native, NativeModule *module, Allocator *allocator){
	NativeFn *native_fn = factory_create_native_fn(1, name, arity, NULL, raw_native, allocator);

    if(!native_fn){
        factory_destroy_native_fn(native_fn);
        return 1;
    }

    NativeModuleSymbol symbol = (NativeModuleSymbol){
        .type = NATIVE_FUNCTION_NMSYMTYPE,
        .content.native_fn = native_fn
    };

    if(lzohtable_put_ckv(name, strlen(name), &symbol, sizeof(NativeModuleSymbol), module->symbols, NULL)){
        factory_destroy_native_fn(native_fn);
        return 1;
    }

    return 0;
}

int factory_add_native_fn_info_n(char *name, uint8_t arity, RawNativeFn raw_native, LZOHTable *natives, Allocator *allocator){
    size_t name_len;
    char *cloned_name = factory_clone_raw_str(name, allocator, &name_len);
    NativeFn *native_fn = MEMORY_ALLOC(NativeFn, 1, allocator);

    if(!cloned_name || !native_fn){
        factory_destroy_raw_str(cloned_name, allocator);
        MEMORY_DEALLOC(NativeFn, 1, native_fn, allocator);

        return 1;
    }

    native_fn->core = 0;
    native_fn->arity = arity;
    native_fn->name = cloned_name;
    native_fn->raw_fn = raw_native;

    return lzohtable_put_ckv(name, name_len, native_fn, sizeof(NativeFn), natives, NULL);
}

Fn *factory_create_fn(char *name, Module *module, Allocator *allocator){
    char *fn_name = factory_clone_raw_str(name, allocator, NULL);
    DynArr *params = FACTORY_DYNARR_PTR(allocator);
    DynArr *chunks = FACTORY_DYNARR(sizeof(uint8_t), allocator);
	DynArr *locations = FACTORY_DYNARR(sizeof(OPCodeLocation), allocator);
    DynArr *iconsts = FACTORY_DYNARR(sizeof(int64_t), allocator);
    DynArr *fconsts = FACTORY_DYNARR(sizeof(double), allocator);
    Fn *fn = MEMORY_ALLOC(Fn, 1, allocator);

    if(!fn_name || !params || !chunks || !locations || !iconsts || !fconsts || !fn){
        factory_destroy_raw_str(fn_name, allocator);
        dynarr_destroy(params);
        dynarr_destroy(chunks);
        dynarr_destroy(locations);
        dynarr_destroy(iconsts);
        dynarr_destroy(fconsts);

        MEMORY_DEALLOC(Fn, 1, fn, allocator);

        return NULL;
    }

    fn->name = fn_name;
    fn->params = params;
    fn->chunks = chunks;
	fn->locations = locations;
    fn->iconsts = iconsts;
    fn->fconsts = fconsts;
    fn->module = module;
    fn->allocator = allocator;

    return fn;
}

void factory_destroy_fn(Fn *fn){
    if(!fn){
        return;
    }

    Allocator *allocator = fn->allocator;

    factory_destroy_raw_str(fn->name, allocator);
    dynarr_destroy(fn->params);
    dynarr_destroy(fn->chunks);
    dynarr_destroy(fn->locations);
    dynarr_destroy(fn->iconsts);
    dynarr_destroy(fn->fconsts);

    MEMORY_DEALLOC(Fn, 1, fn, allocator);
}

NativeModule *factory_create_native_module(char *name, Allocator *allocator){
	char *module_name = factory_clone_raw_str(name, allocator, NULL);
	LZOHTable *symbols = FACTORY_LZOHTABLE(allocator);
	NativeModule *module = MEMORY_ALLOC(NativeModule, 1, allocator);

    if(!module_name || !symbols || !module){
        factory_destroy_raw_str(module_name, allocator);
        LZOHTABLE_DESTROY(symbols);
        MEMORY_DEALLOC(NativeModule, 1, module, allocator);

        return NULL;
    }

	module->name = module_name;
	module->symbols = symbols;
    module->allocator = allocator;

	return module;
}

void factory_destroy_native_module(NativeModule *native_module){
    if(!native_module){
        return;
    }

    Allocator *allocator = native_module->allocator;

    factory_destroy_raw_str(native_module->name, allocator);
    LZOHTABLE_DESTROY(native_module->symbols);
    MEMORY_DEALLOC(NativeModule, 1, native_module, allocator);
}

Module *factory_create_module(char *name, char *filepath, Allocator *allocator){
	DynArr *symbols = FACTORY_DYNARR_TYPE(SubModuleSymbol, allocator);
    LZHTable *tries = FACTORY_LZHTABLE(allocator);
    LZOHTable *globals = FACTORY_LZOHTABLE_LEN(64, allocator);
    DynArr *static_strs = FACTORY_DYNARR_TYPE(RawStr, allocator);
    SubModule *submodule = MEMORY_ALLOC(SubModule, 1, allocator);

    char *module_name = factory_clone_raw_str(name, allocator, NULL);
    char *module_pathname = factory_clone_raw_str(filepath, allocator, NULL);
    Module *module = MEMORY_ALLOC(Module, 1, allocator);

    if(!symbols || !tries || !globals || !static_strs || !submodule || !module_name || !module_pathname || !module){
        dynarr_destroy(symbols);
        lzhtable_destroy(NULL, NULL, tries);
        LZOHTABLE_DESTROY(globals);
        dynarr_destroy(static_strs);
        MEMORY_DEALLOC(SubModule, 1, submodule, allocator);

        factory_destroy_raw_str(module_name, allocator);
        factory_destroy_raw_str(module_pathname, allocator);
        MEMORY_DEALLOC(Module, 1, module, allocator);

        return NULL;
    }

    submodule->resolved = 0;
    submodule->symbols = symbols;
    submodule->tries = tries;
    submodule->globals = globals;
    submodule->static_strs = static_strs;

    module->original = 1;
    module->name = module_name;
    module->pathname = module_pathname;
    module->submodule = submodule;
    module->allocator = allocator;

    return module;
}

Module *factory_create_clone_module(char *new_name, char *filepath, Module *module, Allocator *allocator){
    char *module_name = factory_clone_raw_str(new_name, allocator, NULL);
    char *module_pathname = factory_clone_raw_str(filepath, allocator, NULL);
    Module *new_module = MEMORY_ALLOC(Module, 1, allocator);

    new_module->original = 0;
    new_module->name = module_name;
    new_module->pathname = module_pathname;
    new_module->submodule = module->submodule;

    return new_module;
}

void factory_destroy_module(Module *module){
    if(!module){
        return;
    }

    Allocator *allocator = module->allocator;

    if(module->original){
        SubModule *submodule = module->submodule;

        dynarr_destroy(submodule->symbols);
        lzhtable_destroy(NULL, NULL, submodule->tries);
        LZOHTABLE_DESTROY(submodule->globals);
        dynarr_destroy(submodule->static_strs);
        MEMORY_DEALLOC(SubModule, 1, submodule, allocator);
    }

    factory_destroy_raw_str(module->name, allocator);
    factory_destroy_raw_str(module->pathname, allocator);
    MEMORY_DEALLOC(Module, 1, module, allocator);
}