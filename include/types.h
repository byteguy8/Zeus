#ifndef TYPES_H
#define TYPES_H

#include "dynarr.h"
#include "lzhtable.h"
#include <stddef.h>

#define NAME_LEN 256
#define MODULE_TRIES(m)(m->submodule->tries)
#define MODULE_SYMBOLS(m)(m->submodule->symbols)
#define MODULE_STRINGS(m)(m->submodule->strings)
#define MODULE_GLOBALS(m)(m->submodule->globals)

typedef struct vm VM;
typedef struct value Value;
typedef struct module Module;

typedef struct scape_info{
    int from;
    int len;
    char scape;
}ScapeInfo;

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
    size_t len;
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

typedef Value (*RawNativeFn)(uint8_t argc, Value *values, void *target, VM *vm);
typedef Value (*RawForeignFn)(Value *values);

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
	LZHTable *key_values;
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
	LZHTable *symbols;
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
	NATIVE_MODULE_MSYMTYPE,
	FUNCTION_MSYMTYPE,
	MODULE_MSYMTYPE
}ModuleSymbolType;

typedef enum module_symbol_access_type{
    PRIVATE_MSYMATYPE,
    PUBLIC_MSYMATYPE
}ModuleSymbolAccessType;

typedef struct module_symbol{
	ModuleSymbolType type;
    ModuleSymbolAccessType access;
	union{
		NativeModule *native_module;
		Fn *fn;
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
