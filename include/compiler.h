#ifndef COMPILER_H
#define COMPILER_H

#include "dynarr.h"
#include "token.h"
#include "types.h"
#include "lzhtable.h"
#include <setjmp.h>

#define SCOPES_LENGTH 32
#define SYMBOLS_LENGTH 255
#define SYMBOL_NAME_LENGTH 32
#define FUNCTIONS_LENGTH 16
#define LOOP_MARK_LENGTH 16

typedef enum symbol_type{
    MUT_SYMTYPE,
    IMUT_SYMTYPE,
    FN_SYMTYPE,
    NATIVE_FN_SYMTYPE,
}SymbolType;

typedef struct symbol{
    int depth;
    int local;
    SymbolType type;
    size_t name_len;
    char name[SYMBOL_NAME_LENGTH];
    Token *identifier_token;
}Symbol;

typedef enum scope_type{
    BLOCK_SCOPE,
	WHILE_SCOPE,
    FUNCTION_SCOPE
}ScopeType;

typedef struct scope{
    int depth;
    int locals;
    ScopeType type;
    size_t symbols_len;
    Symbol symbols[SYMBOLS_LENGTH];
}Scope;

typedef struct loop_mark{
    uint8_t id;
    size_t len;
    size_t index;
}LoopMark;

typedef struct compiler{
    jmp_buf err_jmp;
	char import;

    int symbols;
    
    uint8_t depth;
    Scope scopes[SCOPES_LENGTH];
    
    uint8_t fn_ptr;
    Function *fn_stack[FUNCTIONS_LENGTH];

	uint8_t while_counter;
	uint8_t stop_ptr;
	LoopMark stops[LOOP_MARK_LENGTH];

    uint8_t continue_ptr;
    LoopMark continues[LOOP_MARK_LENGTH];
    
    LZHTable *keywords;
    LZHTable *natives;
    DynArr *constants;
    LZHTable *strings;
	DynArr *chunks;
    DynArrPtr *functions;
    DynArrPtr *stmts;
}Compiler;

Compiler *compiler_create();
int compiler_compile(
    LZHTable *keywords,
    LZHTable *natives,
    DynArr *constants,
    LZHTable *strings,
    DynArrPtr *functions,
    DynArrPtr *stmts,
    Compiler *compiler
);

int compiler_import(
	size_t symbols,
    LZHTable *keywords,
    LZHTable *natives,
    DynArr *constants,
    LZHTable *strings,
	DynArr *chunks,
    DynArrPtr *functions,
    DynArrPtr *stmts,
    Compiler *compiler
);

#endif
