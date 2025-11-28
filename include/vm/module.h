#ifndef MODULE_H
#define MODULE_H

#include "essentials/memory.h"
#include "essentials/dynarr.h"
#include "essentials/lzohtable.h"

typedef struct try_block{
    size_t try;
    size_t catch;
	uint8_t local;
    struct try_block *outer;
}TryBlock;

typedef enum submodule_symbol_type{
    FUNCTION_SUBMODULE_SYM_TYPE,
    CLOSURE_SUBMODULE_SYM_TYPE,
	NATIVE_MODULE_SUBMODULE_SYM_TYPE,
    MODULE_SUBMODULE_SYM_TYPE
}SubModuleSymbolType;

typedef struct submodule_symbol{
	SubModuleSymbolType type;
    void *value;
}SubModuleSymbol;

typedef struct submodule{
    char            resolved;
    LZOHTable       *globals;
    DynArr          *static_strs;
    DynArr          *symbols;
    const Allocator *allocator;
}SubModule;

typedef struct module{
    uint8_t       original;
    char          *name;
    char          *pathname;
    void          *entry_fn;
    SubModule     *submodule;
    struct module *prev;
}Module;

#define MODULE_SYMBOLS(_module)((_module)->submodule->symbols)
#define MODULE_STRINGS(_module)((_module)->submodule->static_strs)
#define MODULE_GLOBALS(_module)((_module)->submodule->globals)

#endif