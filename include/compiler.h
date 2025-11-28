#ifndef COMPILER_H
#define COMPILER_H

#include "essentials/dynarr.h"
#include "essentials/lzarena.h"
#include "essentials/lzflist.h"
#include "essentials/lzohtable.h"
#include "essentials/lzpool.h"
#include "essentials/memory.h"

#include "scope_manager/scope_manager.h"

#include "token.h"
#include "vm/fn.h"
#include "vm/module.h"

#include <stdint.h>
#include <setjmp.h>

typedef struct label{
	size_t offset;
	size_t name_len;
	char   *name;
}Label;

typedef struct jmp{
	size_t update_offset;
	size_t jump_offset;
	size_t label_name_len;
	char   *label_name;
}Jmp;

typedef struct mark{
    size_t update_offset;
	size_t label_name_len;
	char   *label_name;
}Mark;

typedef struct loop{
    int32_t     id;
    struct loop *prev;
}Loop;

typedef struct block{
	size_t       stmts_len;
	size_t       current_stmt;
	struct block *prev;
}Block;

typedef struct unit{
	int32_t     counter;

	LZOHTable   *labels;
	DynArr      *jmps;
    DynArr      *marks;
    Loop        *loops;
    Block       *blocks;
    LZOHTable   *captured_symbols;

	Fn          *fn;

    LZPool      *labels_pool;
	LZPool      *jmps_pool;
    LZPool      *marks_pool;
    LZPool      *loops_pool;
    LZPool      *blocks_pool;

    void        *arena_state;

	Allocator   *lzarena_allocator;
    Allocator   *lzflist_allocator;

	struct unit *prev;
}Unit;

typedef struct compiler{
    jmp_buf         buf;
	Unit            *units_stack;

    LZOHTable       *keywords;
    DynArr          *search_pathnames;
    LZOHTable       *default_natives;
    ScopeManager    *manager;
    Module          *module;

    LZArena         *compiler_arena;
    LZPool          *units_pool;

    Allocator       *arena_allocator;
	const Allocator *ctallocator;
	const Allocator *rtallocator;
}Compiler;

Compiler *compiler_create(const Allocator *ctallocator, const Allocator *rtallocator);
void compiler_destroy(Compiler *compiler);

Module *compiler_compile(
    Compiler *compiler,
    LZOHTable *keywords,
    DynArr *search_pathnames,
    LZOHTable *default_natives,
    ScopeManager *manager,
    DynArr *stmts,
    const char *pathname
);

Module *compiler_import(
    Compiler *compiler,
    LZArena *compiler_arena,
    Allocator *arena_allocator,
    LZOHTable *keywords,
    DynArr *search_pathnames,
    LZOHTable *default_natives,
    ScopeManager *manager,
    DynArr *stmts,
    const char *pathname,
    const char *name
);

#endif
