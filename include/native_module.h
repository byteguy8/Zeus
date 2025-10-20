#ifndef NATIVE_MODULE_H
#define NATIVE_MODULE_H

#include "lzohtable.h"
#include "memory.h"

typedef struct native_module{
	char *name;
	LZOHTable *symbols;
    Allocator *allocator;
}NativeModule;

#endif
