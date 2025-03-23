#ifndef COMPILER_H
#define COMPILER_H

#include "dynarr.h"
#include "token.h"
#include "rtypes.h"
#include "types.h"
#include "lzhtable.h"
#include <setjmp.h>

#define PATHS_LENGTH 256
#define SCOPES_LENGTH 32
#define SYMBOLS_LENGTH 255
#define SYMBOL_NAME_LENGTH 32
#define FUNCTIONS_LENGTH 16
#define LOOP_MARK_LENGTH 16

typedef enum symbol_type{
    MUT_SYMTYPE,
    IMUT_SYMTYPE,
    FN_SYMTYPE,
    CLOSURE_SYMTYPE,
    NATIVE_FN_SYMTYPE,
    MODULE_SYMTYPE,
    NATIVE_MODULE_SYMTYPE,
}SymbolType;

typedef struct symbol{
    int depth;
    int local;
    int index;
    SymbolType type;
    size_t name_len;
    char name[SYMBOL_NAME_LENGTH];
    Token *identifier_token;
}Symbol;

typedef enum scope_type{
    BLOCK_SCOPE,
	WHILE_SCOPE,
    TRY_SCOPE,
    CATCH_SCOPE,
    FUNCTION_SCOPE
}ScopeType;

typedef struct scope{
    int depth;
    int locals;
    TryBlock *try;
    ScopeType type;

    uint8_t symbols_len;
    Symbol symbols[SYMBOLS_LENGTH];

    int captured_symbols_len;
    Symbol *captured_symbols[SYMBOLS_LENGTH];
}Scope;

typedef struct loop_mark{
    uint8_t id;
    size_t len;
    size_t index;
}LoopMark;

typedef struct compiler{
	char is_err;
    jmp_buf err_jmp;

    int paths_len;
    char *paths[PATHS_LENGTH];

    int symbols;

    uint8_t depth;
    Scope scopes[SCOPES_LENGTH];

    uint8_t fn_ptr;
    Fn *fn_stack[FUNCTIONS_LENGTH];

	uint8_t while_counter;
	uint8_t stop_ptr;
	LoopMark stops[LOOP_MARK_LENGTH];

    uint8_t continue_ptr;
    LoopMark continues[LOOP_MARK_LENGTH];

    LZHTable *keywords;
    LZHTable *natives;
    DynArrPtr *stmts;
    Module *previous_module;
    Module *current_module;
    LZHTable *modules;

    Allocator *ctallocator;
    Allocator *rtallocator;
}Compiler;

Compiler *compiler_create(Allocator *ctallocator, Allocator *rtallocator);

int compiler_compile(
    LZHTable *keywords,
    LZHTable *natives,
    DynArrPtr *stmts,
    Module *module,
    LZHTable *modules,
    Compiler *compiler
);

int compiler_import(
    LZHTable *keywords,
    LZHTable *natives,
    DynArrPtr *stmts,
    Module *previous_module,
    Module *module,
    LZHTable *modules,
    Compiler *compiler
);

#endif
