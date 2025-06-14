#include "factory.h"
#include "memory.h"

char *factory_clone_raw_str(char *raw_str, Allocator *allocator){
    size_t len = strlen(raw_str);
    char *cloned_raw_str = MEMORY_ALLOC(char, len + 1, allocator);

    memcpy(cloned_raw_str, raw_str, len);
    cloned_raw_str[len] = '\0';

    return cloned_raw_str;
}

char *factory_clone_raw_str_range(size_t start, size_t len, char *str, Allocator *allocator){
    char *clone_raw_str = MEMORY_ALLOC(char, len + 1, allocator);

    memcpy(clone_raw_str, str + start, len);
    clone_raw_str[len] = '\0';

    return clone_raw_str;
}

void factory_destroy_raw_str(char *raw_str, Allocator *allocator){
    if(!raw_str){return;}
    size_t raw_str_len = strlen(raw_str);
    MEMORY_DEALLOC(char, raw_str_len + 1, raw_str, allocator);
}

NativeFn *factory_create_native_fn(uint8_t core, char *name, uint8_t arity, Value *target, RawNativeFn raw_native, Allocator *allocator){
    char *cloned_name = factory_clone_raw_str(name, allocator);
    NativeFn *native_fn = MEMORY_ALLOC(NativeFn, 1, allocator);

    native_fn->core = core;
    native_fn->arity = arity;
    native_fn->name = cloned_name;
    native_fn->target = target ? *target : (Value){0};
    native_fn->raw_fn = raw_native;

    return native_fn;
}

void factory_destroy_native_fn(NativeFn *native_fn, Allocator *allocator){
    if(!native_fn){return;}

    factory_destroy_raw_str(native_fn->name, allocator);
    MEMORY_DEALLOC(NativeFn, 1, native_fn, allocator);
}

void factory_add_native_fn(char *name, uint8_t arity, RawNativeFn raw_native, NativeModule *module, Allocator *allocator){
	NativeFn *native = factory_create_native_fn(1, name, arity, NULL, raw_native, allocator);
	NativeModuleSymbol *symbol = MEMORY_ALLOC(NativeModuleSymbol, 1, allocator);

	symbol->type = NATIVE_FUNCTION_NMSYMTYPE;
	symbol->value.native_fn = native;

    lzhtable_put((uint8_t *)name, strlen(name), symbol, module->symbols, NULL);
}

void factory_add_native_fn_info(char *name, uint8_t arity, RawNativeFn raw_native, LZHTable *natives, Allocator *allocator){
    size_t name_len = strlen(name);
    NativeFnInfo *info = MEMORY_ALLOC(NativeFnInfo, 1, allocator);

    info->arity = arity;
    info->raw_native = raw_native;

    lzhtable_put((uint8_t *)name, name_len, info, natives, NULL);
}

Fn *factory_create_fn(char *name, Module *module, Allocator *allocator){
    char *fn_name = factory_clone_raw_str(name, allocator);
    DynArr *params = FACTORY_DYNARR_PTR(allocator);
    DynArr *chunks = FACTORY_DYNARR(sizeof(uint8_t), allocator);
	DynArr *locations = FACTORY_DYNARR(sizeof(OPCodeLocation), allocator);
    DynArr *constants = FACTORY_DYNARR(sizeof(int64_t), allocator);
    DynArr *float_values = FACTORY_DYNARR(sizeof(double), allocator);
    Fn *fn = MEMORY_ALLOC(Fn, 1, allocator);

    fn->name = fn_name;
    fn->params = params;
    fn->chunks = chunks;
	fn->locations = locations;
    fn->integers = constants;
    fn->floats = float_values;
    fn->module = module;

    return fn;
}

OutValue *factory_out_values(uint8_t length, Allocator *allocator){
    return MEMORY_ALLOC(OutValue, (size_t)length, allocator);
}

void factory_destroy_out_values(uint8_t length, OutValue *values, Allocator *allocator){
    if(!values){return;}
    MEMORY_DEALLOC(OutValue, (size_t)length, values, allocator);
}

Closure *factory_closure(OutValue *values, MetaClosure *meta, Allocator *allocator){
    Closure *closure = MEMORY_ALLOC(Closure, 1, allocator);

    closure->out_values = values;
    closure->meta = meta;

    return closure;
}

void factory_destroy_closure(Closure *closure, Allocator *allocator){
    if(!closure){return;}
    MEMORY_DEALLOC(Closure, 1, closure, allocator);
}

NativeModule *factory_native_module(char *name, Allocator *allocator){
	char *module_name = factory_clone_raw_str(name, allocator);
	LZHTable *symbols = FACTORY_LZHTABLE(allocator);
	NativeModule *module = MEMORY_ALLOC(NativeModule, 1, allocator);

	module->name = module_name;
	module->symbols = symbols;

	return module;
}

Module *factory_module(char *name, char *filepath, Allocator *allocator){
    char *module_name = factory_clone_raw_str(name, allocator);
    char *module_pathname = factory_clone_raw_str(filepath, allocator);
    LZHTable *strings = FACTORY_LZHTABLE(allocator);
	DynArr *symbols = FACTORY_DYNARR(sizeof(SubModuleSymbol), allocator);
    LZHTable *tries = FACTORY_LZHTABLE(allocator);
    LZHTable *globals = FACTORY_LZHTABLE(allocator);
    SubModule *submodule = MEMORY_ALLOC(SubModule, 1, allocator);
    Module *module = MEMORY_ALLOC(Module, 1, allocator);

    submodule->resolved = 0;
    submodule->strings = strings;
    submodule->symbols = symbols;
    submodule->tries = tries;
    submodule->globals = globals;

    module->shadow = 0;
    module->name = module_name;
    module->pathname = module_pathname;
    module->submodule = submodule;

    return module;
}

Module *factory_clone_module(char *new_name, char *filepath, Module *module, Allocator *allocator){
    char *module_name = factory_clone_raw_str(new_name, allocator);
    char *module_pathname = factory_clone_raw_str(filepath, allocator);
    Module *new_module = MEMORY_ALLOC(Module, 1, allocator);

    new_module->shadow = 1;
    new_module->name = module_name;
    new_module->pathname = module_pathname;
    new_module->submodule = module->submodule;

    return new_module;
}