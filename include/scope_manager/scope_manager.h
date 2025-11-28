#ifndef SCOPE_MANAGER_H
#define SCOPE_MANAGER_H

#include "essentials/lzpool.h"
#include "essentials/memory.h"

#include "scope.h"
#include "token.h"

#include <setjmp.h>

typedef struct scope_manager{
    depth_t         depth;
	jmp_buf         *buf;
    Scope           *scope_stack;
    Scope           *fn_scope_stack;
    Scope           *global_scope;

	LZPool          *local_symbols_pool;
    LZPool          *global_symbols_pool;
    LZPool          *native_fn_symbols_pool;
	LZPool          *fn_symbols_pool;
	LZPool          *module_symbols_pool;
	LZPool          *scopes_pool;

    LZArena         *manager_arena;
    Allocator       *manager_arena_allocator;
	const Allocator *allocator;
}ScopeManager;

ScopeManager *scope_manager_create(const Allocator *ctallocator);
void scope_manager_destroy(ScopeManager *manager);

Scope *scope_manager_peek(const ScopeManager *scope_manager);
Scope *scope_manager_push(ScopeManager *manager, ScopeType type);
void scope_manager_pop(ScopeManager *scope_manager);

int scope_manager_is_global_scope(const ScopeManager *manager);
int scope_manager_is_scope_type(const ScopeManager *manager, ScopeType type);
int scope_manager_exists_procedure_name(const ScopeManager *manager, size_t name_len, const char *name);
size_t scope_manager_locals_count(const ScopeManager *manager);

Symbol *scope_manager_get_symbol(ScopeManager *scope_manager, Token *identifier);

LocalSymbol *scope_manager_define_local(
    ScopeManager *manager,
    uint8_t is_mutable,
    uint8_t is_initialized,
    const Token *identifier_token
);
GlobalSymbol *scope_manager_define_global(
    ScopeManager *manager,
    uint8_t is_mutable,
    const Token *identifier_token
);
NativeFnSymbol *scope_manager_define_native_fn(
    ScopeManager *manager,
    uint8_t arity,
    const char *name
);
FnSymbol *scope_manager_define_fn(
    ScopeManager *manager,
    uint8_t arity,
    const Token *identifier_token
);
ModuleSymbol *scope_manager_define_module(
    ScopeManager *manager,
    const Token *identifier_token
);

#endif
