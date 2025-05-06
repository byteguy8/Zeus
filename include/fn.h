#ifndef FN_H
#define FN_H

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
    DynArr *chunks;
    DynArr *params;
	DynArr *locations;
    DynArr *integers;
    DynArr *floats;
    Module *module;
}Fn;

#endif