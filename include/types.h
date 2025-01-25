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

typedef struct value Value;
typedef struct vm VM;
typedef struct module Module;

typedef struct rawstr{
	size_t size;
	char *buff;
}RawStr;

typedef struct str{
	char core;
	size_t len;
	char *buff;
}Str;

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

typedef struct fn{
    char *name;
    DynArr *chunks;
    DynArrPtr *params;
	DynArr *locations;
    DynArr *constants;
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
    char *filepath;
    SubModule *submodule;
}Module;

typedef enum module_symbol_type{
	FUNCTION_MSYMTYPE,
	MODULE_MSYMTYPE,
}ModuleSymbolType;

typedef struct module_symbol{
	ModuleSymbolType type;
	union{
		Fn *fn;
		Module *module;
	}value;
}ModuleSymbol;

typedef struct native_module{
    void *handler;  
}NativeLib;

#endif