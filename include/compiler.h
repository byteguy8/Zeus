#ifndef COMPILER_H
#define COMPILER_H

#include "dynarr.h"

typedef struct compiler{
    DynArr *constants;
    DynArr *chunks;
    DynArrPtr *stmts;
}Compiler;

Compiler *compiler_create();
int compiler_compile(
    DynArr *constants,
    DynArr *chunks,
    DynArrPtr *stmts, 
    Compiler *compiler
);

#endif