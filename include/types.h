#ifndef TYPES_H
#define TYPES_H

#include "dynarr.h"
#include "lzhtable.h"
#include <stddef.h>

typedef struct runtime_allocator_interface{
    void *ctx;
    void *(*alloc)(size_t size, void *ctx);
    void *(*realloc)(void *ptr, size_t old_size, size_t new_size, void *ctx);
    void (*dealloc)(void *ptr, size_t size, void *ctx);
} Allocator;

#define NAME_LEN 256
#define OUT_VALUES_LENGTH 255

#define MODULE_TRIES(m)(m->submodule->tries)
#define MODULE_SYMBOLS(m)(m->submodule->symbols)
#define MODULE_STRINGS(m)(m->submodule->strings)
#define MODULE_GLOBALS(m)(m->submodule->globals)

typedef struct vm VM;
typedef struct value Value;
typedef struct module Module;
typedef Value (*RawNativeFn)(uint8_t argc, Value *values, void *target, VM *vm);
typedef Value *(*RawForeignFn)(Value *values);

typedef struct scape_info{
    int from;
    int len;
    char scape;
}ScapeInfo;

typedef struct native_fn_info{
    uint8_t arity;
    RawNativeFn raw_native;
}NativeFnInfo;

typedef struct rawstr{
	size_t size;
	char *buff;
}RawStr;

typedef struct try_block{
    size_t try;
    size_t catch;
	uint8_t local;
    struct try_block *outer;
}TryBlock;

typedef struct opcode_location{
	size_t offset;
	int line;
    char *filepath;
}OPCodeLocation;

// TYPES RELATED TO TYPE SYSTEM
typedef struct str{
	char core;
	size_t len;
	char *buff;
}Str;

typedef struct array{
    int32_t len;
    Value *values;
}Array;

typedef struct fn{
    char *name;
    DynArr *chunks;
    DynArrPtr *params;
	DynArr *locations;
    DynArr *constants;
    DynArr *float_values;
    Module *module;
}Fn;

typedef struct meta_out_value{
    uint8_t at;
}MetaOutValue;

typedef struct meta_closure{
    int values_len;
    MetaOutValue values[OUT_VALUES_LENGTH];
    Fn *fn;
}MetaClosure;

typedef struct out_value{
    uint8_t linked;
    uint8_t at;
    Value *value;
    struct out_value *prev;
    struct out_value *next;
}OutValue;

typedef struct closure{
    int values_len;
    OutValue *values;
    MetaClosure *meta;
}Closure;

typedef struct native_fn{
    char unique;
    int arity;
    size_t name_len;
    char name[NAME_LEN];
    void *target;
    RawNativeFn raw_fn;
}NativeFn;

typedef struct foreign_fn{
    RawForeignFn raw_fn;
}ForeignFn;

typedef struct record{
	LZHTable *attributes;
}Record;

//> NATIVE MODULE RELATED
typedef struct native_module{
	char *name;
	LZHTable *symbols;
}NativeModule;

typedef enum native_module_symbol_type{
	NATIVE_FUNCTION_NMSYMTYPE
}NativeModuleSymbolType;

typedef struct native_module_symbol{
	NativeModuleSymbolType type;
	union{
		NativeFn *fn;
	}value;
}NativeModuleSymbol;
//< NATIVE MODULE RELATED

//> MODULE RELATED
typedef struct submodule{
    char resolve;
    LZHTable *tries;
	DynArr *symbols;
    LZHTable *strings;
    LZHTable *globals;
}SubModule;

typedef struct module{
    char shadow;
    char *name;
    char *pathname;
    SubModule *submodule;
}Module;

typedef enum module_symbol_type{
    FUNCTION_MSYMTYPE,
    CLOSURE_MSYMTYPE,
	NATIVE_MODULE_MSYMTYPE,
    MODULE_MSYMTYPE
}ModuleSymbolType;

typedef struct module_symbol{
	ModuleSymbolType type;
	union{
        Fn *fn;
        MetaClosure *meta_closure;
		NativeModule *native_module;
		Module *module;
	}value;
}ModuleSymbol;
//< MODULE RELATED

//> NATIVE LIBRARY RELATED
typedef struct native_lib{
    void *handler;  
}NativeLib;
//< NATIVE LIBRARY RELATED

#endif
