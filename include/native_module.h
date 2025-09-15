#ifndef NATIVE_MODULE_H
#define NATIVE_MODULE_H

#include "memory.h"
#include "native_fn.h"

typedef struct native_module{
	char *name;
	LZOHTable *symbols;
    Allocator *allocator;
}NativeModule;

#endif