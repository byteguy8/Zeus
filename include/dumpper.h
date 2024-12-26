#ifndef DUMPPER_H
#define DUMPPER_H

#include "dynarr.h"
#include "lzhtable.h"
#include "types.h"

typedef struct dumpper{
    size_t ip;
    LZHTable *modules;
    Module *main_module;
    Module *current_module;
    Fn *current_fn;
}Dumpper;

Dumpper *dumpper_create();
void dumpper_dump(
    LZHTable *modules,
    Module *main_module,
    Dumpper *dumpper
);

#endif
