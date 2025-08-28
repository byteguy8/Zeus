#ifndef FN_H
#define FN_H

#include "memory.h"
#include "module.h"
#include "dynarr.h"

#define NAME_LEN 256

typedef struct opcode_location{
	size_t offset;
	int line;
    char *filepath;
}OPCodeLocation;

typedef struct fn{
    char *name;
    DynArr *params;
    DynArr *chunks;
	DynArr *locations;
    DynArr *iconsts;
    DynArr *fconsts;
    Module *module;
    Allocator *allocator;
}Fn;

#endif