#ifndef NATIVE_MODULE_H
#define NATIVE_MODULE_H

#include "native_fn.h"
#include "lzhtable.h"

typedef enum native_module_symbol_type{
	NATIVE_FUNCTION_NMSYMTYPE
}NativeModuleSymbolType;

typedef struct native_module_symbol{
	NativeModuleSymbolType type;
	union{
		NativeFn *fn;
	}value;
}NativeModuleSymbol;

typedef struct native_module{
	char *name;
	LZHTable *symbols;
}NativeModule;

#endif