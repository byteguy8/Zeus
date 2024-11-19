#ifndef COMPILER_H
#define COMPILER_H

#include "dynarr.h"
#include "function.h"

#define SCOPES_LENGTH 32
#define SYMBOLS_LENGTH 255
#define SYMBOL_NAME_LENGTH 16
#define FUNCTIONS_LENGTH 16

typedef struct symbol{
    size_t local;
    char is_const;
    size_t name_len;
    char name[SYMBOL_NAME_LENGTH];
}Symbol;

typedef enum scope_type{
    BLOCK_SCOPE,
    FUNCTION_SCOPE
}ScopeType;

typedef struct scope{
    size_t depth;
    size_t locals;
    ScopeType type;
    size_t symbols_len;
    Symbol symbols[SYMBOLS_LENGTH];
}Scope;

typedef struct compiler{
    size_t depth;
    Scope scopes[SCOPES_LENGTH];
    
    size_t fn_ptr;
    Function *fn_stack[FUNCTIONS_LENGTH];
    
    DynArr *constants;
    DynArrPtr *functions;
    DynArrPtr *stmts;
}Compiler;

Compiler *compiler_create();
int compiler_compile(
    DynArr *constants,
    DynArrPtr *functions,
    DynArrPtr *stmts,
    Compiler *compiler
);

#endif
