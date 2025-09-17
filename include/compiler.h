#ifndef COMPILER_H
#define COMPILER_H

#include "dynarr.h"
#include "fn.h"
#include "module.h"
#include "token.h"
#include "types.h"
#include <setjmp.h>

#define PATHS_LENGTH 256
#define LABEL_NAME_LENGTH 64
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
    char name[SYMBOL_NAME_LENGTH];
    Token *identifier_token;
}Symbol;

typedef enum scope_type{
    BLOCK_SCOPE,
    IF_SCOPE,
	WHILE_SCOPE,
    FOR_SCOPE,
    TRY_SCOPE,
    CATCH_SCOPE,
    FUNCTION_SCOPE
}ScopeType;

typedef struct label{
    char *name;
    size_t location;
    struct label *prev;
    struct label *next;
}Label;

typedef struct label_list{
    Label *head;
    Label *tail;
}LabelList;

typedef struct jmp{
    size_t bef;
    size_t af;
    size_t idx;
    char *label;
    struct jmp *prev;
    struct jmp *next;
    void *scope;
}JMP;

typedef struct jmp_list{
    JMP *head;
    JMP *tail;
}JMPList;

typedef struct scope{
    int32_t id;

    int depth;
    int locals;
    int scope_locals;
    ScopeType type;

    uint8_t symbols_len;
    Symbol symbols[SYMBOLS_LENGTH];

    int captured_symbols_len;
    Symbol *captured_symbols[SYMBOLS_LENGTH];

    LabelList labels;
    JMPList jmps;
}Scope;

typedef struct compiler{
    char is_err;
    jmp_buf err_jmp;

    char is_importing;
    DynArr *search_paths;
    LZOHTable *import_paths;

    int32_t counter_id;
    uint32_t trycatch_counter;

    int symbols;

    uint8_t depth;
    Scope scopes[SCOPES_LENGTH];

    uint8_t fn_ptr;
    Fn *fn_stack[FUNCTIONS_LENGTH];

    LZOHTable *keywords;
    LZOHTable *natives_fns;
    DynArr *fns_prototypes;
    DynArr *stmts;
    Module *previous_module;
    Module *current_module;
    LZOHTable *modules;

    Allocator *ctallocator;
    Allocator *rtallocator;
    Allocator *labels_allocator;
    Allocator fake_rtallocator;
    Allocator fake_ctallocator;
}Compiler;

Compiler *compiler_create(Allocator *ctallocator, Allocator *rtallocator);
void compiler_destroy(Compiler *compiler);

int compiler_compile(
    DynArr *search_paths,
    LZOHTable *import_paths,
    LZOHTable *keywords,
    LZOHTable *native_fns,
    DynArr *fns_prototypes,
    DynArr *stmts,
    Module *module,
    LZOHTable *modules,
    Compiler *compiler
);

int compiler_import(
    DynArr *search_paths,
    LZOHTable *import_paths,
    LZOHTable *keywords,
    LZOHTable *native_fns,
    DynArr *fns_prototypes,
    DynArr *stmts,
    Module *previous_module,
    Module *module,
    LZOHTable *modules,
    Compiler *compiler
);

#endif
