#ifndef DUMPPER_H
#define DUMPPER_H

#include "dynarr.h"

typedef struct dumpper{
    size_t ip;
	DynArr *constants;
	DynArr *chunks;
}Dumpper;

Dumpper *dumpper_create();
void dumpper_dump(DynArr *constants, DynArr *chunks, Dumpper *dumpper);

#endif
