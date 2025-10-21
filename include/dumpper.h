#ifndef DUMPPER_H
#define DUMPPER_H

#include "dynarr.h"
#include "fn.h"
#include "module.h"

typedef struct dumpper{
    size_t ip;
    LZOHTable *modules;
    Module *main_module;
    Module *current_module;
    Fn *current_fn;
    Allocator *allocator;
}Dumpper;

Dumpper *dumpper_create(Allocator *allocator);
void dumpper_dump(LZOHTable *modules, Module *main_module, Dumpper *dumpper);

#endif
