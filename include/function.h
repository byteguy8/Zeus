#ifndef FUNCTION_H
#define FUNCTION_H

#include "dynarr.h"

typedef struct function
{
    char *name;
    DynArr *chunks;
    DynArrPtr *params;
}Function;

#endif