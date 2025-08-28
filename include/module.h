#ifndef MODULE_H
#define MODULE_H

#include "memory.h"
#include "dynarr.h"
#include "lzhtable.h"
#include "lzohtable.h"

typedef struct try_block{
    size_t try;
    size_t catch;
	uint8_t local;
    struct try_block *outer;
}TryBlock;

typedef enum submodule_symbol_type{
    FUNCTION_MSYMTYPE,
    CLOSURE_MSYMTYPE,
	NATIVE_MODULE_MSYMTYPE,
    MODULE_MSYMTYPE
}SubModuleSymbolType;

typedef struct submodule_symbol{
	SubModuleSymbolType type;
    void *value;
}SubModuleSymbol;

typedef struct submodule{
    char resolved;
    DynArr *symbols;
    LZHTable *tries;
    LZOHTable *globals;
    DynArr *static_strs;
}SubModule;

typedef struct module{
    char original;
    char *name;
    char *pathname;
    SubModule *submodule;
    Allocator *allocator;
}Module;

#define MODULE_TRIES(_module)((_module)->submodule->tries)
#define MODULE_SYMBOLS(_module)((_module)->submodule->symbols)
#define MODULE_STRINGS(_module)((_module)->submodule->static_strs)
#define MODULE_GLOBALS(_module)((_module)->submodule->globals)

#endif