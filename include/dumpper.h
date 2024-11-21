#ifndef DUMPPER_H
#define DUMPPER_H

#include "dynarr.h"
#include "lzhtable.h"

typedef struct dumpper{
    size_t ip;
	DynArr *constants;
    LZHTable *strings;
    DynArr *chunks;
}Dumpper;

Dumpper *dumpper_create();
void dumpper_dump(
    DynArr *constants,
    LZHTable *strings,
    DynArrPtr *functions,
    Dumpper *dumpper
);

#endif
