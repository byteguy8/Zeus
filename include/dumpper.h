#ifndef DUMPPER_H
#define DUMPPER_H

#include "dynarr.h"
#include "lzhtable.h"
#include "types.h"

typedef struct dumpper{
    size_t ip;
	Module *module;
    Module *current_module;
    DynArr *chunks;
}Dumpper;

Dumpper *dumpper_create();
void dumpper_dump(
    Module *module,
    Dumpper *dumpper
);

#endif
