#ifndef MODULE_H
#define MODULE_H

#include "dynarr.h"
#include "lzhtable.h"

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

#define MODULE_TRIES(m)(m->submodule->tries)
#define MODULE_SYMBOLS(m)(m->submodule->symbols)
#define MODULE_STRINGS(m)(m->submodule->strings)
#define MODULE_GLOBALS(m)(m->submodule->globals)

#endif