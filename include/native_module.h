#ifndef NATIVE_MODULE_H
#define NATIVE_MODULE_H

#include "memory.h"
#include "native_fn.h"

typedef enum native_module_symbol_type{
	NATIVE_FUNCTION_NMSYMTYPE
}NativeModuleSymbolType;

typedef struct native_module_symbol{
	NativeModuleSymbolType type;
    Value value;
}NativeModuleSymbol;

typedef struct native_module{
	char *name;
	LZOHTable *symbols;
    Allocator *allocator;
}NativeModule;

#endif