#ifndef COMPILER_H
#define COMPILER_H

#include "dynarr.h"

typedef struct symbol
{
    size_t local;
    size_t name_len;
    char name[255];
}Symbol;

typedef struct scope
{
    size_t depth;
    size_t locals;
    size_t symbols_len;
    Symbol symbols[255];
}Scope;

typedef struct compiler{
    size_t depth;
    Scope scopes[255];

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