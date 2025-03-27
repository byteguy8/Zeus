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

Str *factory_create_str(char *buff, Allocator *allocator){
    Str *str = MEMORY_ALLOC(Str, 1, allocator);

    str->runtime = 1;
    str->len = strlen(buff);
    str->buff = buff;

    return str;
}

void factory_destroy_str(Str *str, Allocator *allocator){
    if(!str){return;}
    MEMORY_DEALLOC(Str, 1, str, allocator);
}

Array *factory_create_array(int32_t length, Allocator *allocator){
    Value *values = MEMORY_ALLOC(Value, (size_t)length, allocator);
    Array *array = MEMORY_ALLOC(Array, 1, allocator);

    array->len = length;
    array->values = values;

    return array;
}

void factory_destroy_array(Array* array, Allocator *allocator){
    if(!array){return;}
    MEMORY_DEALLOC(Value, array->len, array->values, allocator);
    MEMORY_DEALLOC(Array, 1, array, allocator);
}

Record *factory_create_record(uint32_t length, Allocator *allocator){
    LZHTable *attributes = length == 0 ? NULL : FACTORY_LZHTABLE(allocator);
    Record *record = MEMORY_ALLOC(Record, 1, allocator);

    record->attributes = attributes;

    return record;
}

void factory_destroy_record(void *extra, void (*clean_up)(void *, void *, void *), Record *record, Allocator *allocator){
    if(!record){return;}
    lzhtable_destroy(extra, clean_up, record->attributes);
    MEMORY_DEALLOC(Record, 1, record, allocator);
}

NativeFn *factory_create_native_fn(char *name, uint8_t arity, void *target, RawNativeFn raw_native, Allocator *allocator){
    char *cloned_name = factory_clone_raw_str(name, allocator);
    NativeFn *native_fn = MEMORY_ALLOC(NativeFn, 1, allocator);

    native_fn->arity = arity;
    native_fn->name = cloned_name;
    native_fn->target = target;
    native_fn->raw_fn = raw_native;

    return native_fn;
}

void factory_destroy_native_fn(NativeFn *native_fn, Allocator *allocator){
    if(!native_fn){return;}

    factory_destroy_raw_str(native_fn->name, allocator);
    MEMORY_DEALLOC(NativeFn, 1, native_fn, allocator);
}

void factory_add_native_fn(char *name, uint8_t arity, RawNativeFn raw_native, NativeModule *module, Allocator *allocator){
	NativeFn *native = factory_create_native_fn(name, arity, NULL, raw_native, allocator);
	NativeModuleSymbol *symbol = MEMORY_ALLOC(NativeModuleSymbol, 1, allocator);

	symbol->type = NATIVE_FUNCTION_NMSYMTYPE;
	symbol->value.fn = native;

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
    DynArrPtr *params = FACTORY_DYNARR_PTR(allocator);
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

    if(!closure){return NULL;}

    closure->values = values;
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

ForeignLib *factory_create_foreign_lib(void *handler, Allocator *allocator){
    ForeignLib *foreign_lib = MEMORY_ALLOC(ForeignLib, 1, allocator);
    if(!foreign_lib){return NULL;}
    foreign_lib->handler = handler;
    return foreign_lib;
}

void factory_destroy_foreign_lib(ForeignLib *lib, Allocator *allocator){
    if(!lib){return;}
    MEMORY_DEALLOC(ForeignLib, 1, lib, allocator);
}