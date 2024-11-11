#ifndef COMPILER_H
#define COMPILER_H

#include "dynarr.h"
#define SCOPES_LENGTH 32
#define SYMBOLS_LENGTH 255
#define SYMBOL_NAME_LENGTH 16

typedef struct symbol
{
    size_t local;
    size_t name_len;
    char name[SYMBOL_NAME_LENGTH];
}Symbol;

typedef struct scope
{
    size_t depth;
    size_t locals;
    size_t symbols_len;
    Symbol symbols[SYMBOLS_LENGTH];
}Scope;

typedef struct compiler{
    size_t depth;
    Scope scopes[SCOPES_LENGTH];

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
