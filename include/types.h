#ifndef TYPES_H
#define TYPES_H

#include "dynarr.h"
#include "lzhtable.h"
#include <stddef.h>

#define NAME_LEN 256

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
    Module *module;
}Fn;

typedef Value (*RawNativeFn)(uint8_t argc, Value *values, void *target, VM *vm);

typedef struct native_fn{
    char unique;
    int arity;
    size_t name_len;
    char name[NAME_LEN];
    void *target;
    RawNativeFn native;
}NativeFn;

typedef struct record{
	LZHTable *key_values;
}Record;

typedef struct module{
    char to_resolve;
    char *name;
    char *filepath;
    LZHTable *strings;
    DynArr *constants;
	LZHTable *symbols;
    LZHTable *tries;
    LZHTable *globals;
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

#endif