#ifndef FN_H
#define FN_H

#include "essentials/memory.h"
#include "essentials/dynarr.h"

#include "module.h"

#define NAME_LEN 256

typedef struct opcode_location{
	size_t offset;
	int line;
    char *filepath;
}OPCodeLocation;

typedef struct fn{
    uint8_t arity;
    char *name;
    DynArr *chunks;
    DynArr *iconsts;
    DynArr *fconsts;
    DynArr *locations;
    Module *module;
    const Allocator *allocator;
}Fn;

#endif