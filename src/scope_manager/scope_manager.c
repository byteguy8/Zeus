#include "scope_manager.h"
#include "scope.h"

#include "essentials/lzohtable.h"
#include "essentials/lzpool.h"
#include "essentials/memory.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

static void error(ScopeManager *manager, const Token *token, const char *fmt, ...);
static void internal_error(ScopeManager *manager, const Token *token, const char *fmt, ...);
static void internal_error_nctx(ScopeManager *manager, const char *fmt, ...);

static Symbol *exists(Scope *scope, const Token *identifier);
static Symbol *exists_local(Scope *scope, const Token *identifier);

static int init_scope(ScopeManager *manager, Scope *scope, ScopeType type);
static local_t create_locals_counter(ScopeManager *manager);
static local_t generate_local_offset(ScopeManager *manager, Scope *scope, const Token *ref_token);
static Scope *create_global_scope(const Allocator *allocator);

void error(ScopeManager *manager, const Token *token, const char *fmt, ...){
    va_list args;
	va_start(args, fmt);

	fprintf(stderr, "SCOPE ERROR at line %d:\n\t", token->line);
	vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

	va_end(args);

	longjmp(*manager->buf, 1);
}

void internal_error(ScopeManager *manager, const Token *token, const char *fmt, ...){
    va_list args;
	va_start(args, fmt);

	fprintf(stderr, "INTERNAL SCOPE ERROR at line %d:\n\t", token->line);
	vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

	va_end(args);

	longjmp(*manager->buf, 1);
}

void internal_error_nctx(ScopeManager *manager, const char *fmt, ...){
    va_list args;
	va_start(args, fmt);

	fprintf(stderr, "INTERNAL SCOPE ERROR:\n\t");
	vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

	va_end(args);

	longjmp(*manager->buf, 1);
}

inline Symbol *exists(Scope *scope, const Token *identifier){
    Symbol *symbol = NULL;

	if(lzohtable_lookup(
		identifier->lexeme_len,
		identifier->lexeme,
		scope->symbols,
		(void **)(&symbol)
	)){
		return symbol;
	}

    Scope *prev = scope->prev;

    if(prev){
        return exists(prev, identifier);
    }

	return NULL;
}

inline Symbol *exists_local(Scope *scope, const Token *identifier){
	Symbol *symbol = NULL;

	if(lzohtable_lookup(
		identifier->lexeme_len,
		identifier->lexeme,
		scope->symbols,
		(void **)(&symbol)
	)){
		return symbol;
	}

	return NULL;
}


inline int init_scope(ScopeManager *manager, Scope *scope, ScopeType type){
	void *arena_state = lzarena_save(manager->manager_arena);
    LZOHTable *symbols = MEMORY_LZOHTABLE_LEN(manager->manager_arena_allocator, 256);

    scope->type = type;
	scope->symbols = symbols;
    scope->arena_state = arena_state;
    scope->prev = NULL;

    return 0;
}

inline local_t create_locals_counter(ScopeManager *manager){
    Scope *current = scope_manager_peek(manager);
    return IS_LOCAL_SCOPE(current) ? current->content.local_scope.locals : 0;
}

inline local_t generate_local_offset(ScopeManager *manager, Scope *scope, const Token *ref_token){
    assert(IS_LOCAL_SCOPE(scope) && "Expect local scope");
    LocalScope *local_scope = AS_LOCAL_SCOPE(scope);

    if(local_scope->locals == LOCAL_T_MAX){
        internal_error(
            manager,
            ref_token,
            "Cannot declare more than %"LOCAL_T_PRINT" locals per scope",
            local_scope->locals
        );
    }

    return local_scope->locals++;
}

Scope *create_global_scope(const Allocator *allocator){
    LZOHTable *global_symbols = MEMORY_LZOHTABLE_LEN(allocator, 64);
    Scope *global_scope = MEMORY_ALLOC(allocator, Scope, 1);

    MEMORY_CHECK(global_symbols);
    MEMORY_CHECK(global_scope);

    *global_scope = (Scope){
        .type = GLOBAL_SCOPE_TYPE,
        .symbols = global_symbols,
        .arena_state = NULL
    };

    goto OK;

ERROR:
    LZOHTABLE_DESTROY(global_symbols);
    MEMORY_DEALLOC(allocator, Scope, 1, global_scope);

    return NULL;

OK:
    return global_scope;
}

ScopeManager *scope_manager_create(const Allocator *allocator){
    Scope *global_scope = create_global_scope(allocator);

	LZPool *global_symbols_pool = MEMORY_LZPOOL(allocator, GlobalSymbol);
    LZPool *native_fn_symbols_pool = MEMORY_LZPOOL(allocator, NativeFnSymbol);
    LZPool *fn_symbols_pool = MEMORY_LZPOOL(allocator, FnSymbol);
	LZPool *module_symbols_pool = MEMORY_LZPOOL(allocator, ModuleSymbol);
	LZPool *scopes_pool = MEMORY_LZPOOL(allocator, Scope);
    Allocator *manager_arena_allocator = memory_arena_allocator(allocator, NULL);
    ScopeManager *manager = MEMORY_ALLOC(allocator, ScopeManager, 1);

    MEMORY_CHECK(global_scope);
    MEMORY_CHECK(global_symbols_pool);
    MEMORY_CHECK(native_fn_symbols_pool);
    MEMORY_CHECK(fn_symbols_pool);
    MEMORY_CHECK(module_symbols_pool);
    MEMORY_CHECK(scopes_pool);
    MEMORY_CHECK(manager_arena_allocator);
    MEMORY_CHECK(manager);

    *manager = (ScopeManager){
        .depth = 0,
        .scope_stack = NULL,
        .fn_scope_stack = NULL,
        .global_scope = global_scope,
        .local_symbols_pool = NULL,
        .global_symbols_pool = global_symbols_pool,
        .native_fn_symbols_pool = native_fn_symbols_pool,
        .fn_symbols_pool = fn_symbols_pool,
        .module_symbols_pool = module_symbols_pool,
        .scopes_pool = scopes_pool,
        .manager_arena = manager_arena_allocator->ctx,
        .manager_arena_allocator = manager_arena_allocator,
        .allocator = allocator
    };

    goto OK;

ERROR:
    MEMORY_DEALLOC(allocator, Scope, 1, global_scope);
    lzpool_dealloc(global_symbols_pool);
    lzpool_dealloc(native_fn_symbols_pool);
    lzpool_dealloc(fn_symbols_pool);
    lzpool_dealloc(module_symbols_pool);
    lzpool_dealloc(scopes_pool);
    memory_destroy_arena_allocator(manager_arena_allocator);
    MEMORY_DEALLOC(allocator, ScopeManager, 1, manager);

    return NULL;

OK:
    return manager;
}

void scope_manager_destroy(ScopeManager *manager){
    if(!manager){
        return;
    }

    const Allocator *allocator = manager->allocator;

    MEMORY_DEALLOC(allocator, Scope, 1, manager->global_scope);
    lzpool_dealloc(manager->global_symbols_pool);
    lzpool_dealloc(manager->fn_symbols_pool);
    lzpool_dealloc(manager->module_symbols_pool);
    lzpool_dealloc(manager->scopes_pool);
    memory_destroy_arena_allocator(manager->manager_arena_allocator);
    MEMORY_DEALLOC(allocator, ScopeManager, 1, manager);
}

inline Scope *scope_manager_peek(const ScopeManager *manager){
	Scope *top = manager->scope_stack;
	return top ? top : manager->global_scope;
}

Scope *scope_manager_push(ScopeManager *manager, ScopeType type){
    assert(type != GLOBAL_SCOPE_TYPE);

    if(manager->depth == DEPTH_T_MAX){
        internal_error_nctx(
            manager,
            "Scopes depth exceeded max capacity (%"DEPTH_T_PRINT")",
            DEPTH_T_MAX
        );
    }

    Scope *current_scope = scope_manager_peek(manager);

    if(current_scope->type == GLOBAL_SCOPE_TYPE){
        current_scope->arena_state = lzarena_save(manager->manager_arena);

        manager->local_symbols_pool = MEMORY_LZPOOL(
            manager->manager_arena_allocator,
            LocalSymbol
        );

        lzpool_prealloc(1024, manager->local_symbols_pool);
    }

    Scope *scope = lzpool_alloc_x(1024, manager->scopes_pool);

    switch (type){
        case BLOCK_SCOPE_TYPE:
        case IF_SCOPE_TYPE:
        case ELIF_SCOPE_TYPE:
        case ELSE_SCOPE_TYPE:
        case WHILE_SCOPE_TYPE:
        case FOR_SCOPE_TYPE:
        case TRY_SCOPE_TYPE:
        case CATCH_SCOPE_TYPE:{
            scope->content.local_scope = (LocalScope){
            	.depth = manager->depth,
                .locals = create_locals_counter(manager),
                .returned = 0,
                .fn_scope = manager->fn_scope_stack
            };

            break;
        }case FN_SCOPE_TYPE:{
            scope->content.fn_scope = (FnScope){
            	.depth = manager->depth += 1,
                .locals = create_locals_counter(manager),
                .returned = 0,
                .prev_fn = manager->fn_scope_stack
            };

            AS_FN_SCOPE(scope)->prev_fn = manager->fn_scope_stack;
            manager->fn_scope_stack = scope;
        }case GLOBAL_SCOPE_TYPE:{
            break;
        }default:{
            assert(0 && "Illegal scope type");
            break;
        }
    }

    init_scope(manager, scope, type);

    scope->prev = scope_manager_peek(manager);
    manager->scope_stack = scope;

	return scope;
}

void scope_manager_pop(ScopeManager *manager){
	assert(manager->scope_stack);

	Scope *scope = manager->scope_stack;

    manager->scope_stack = IS_GLOBAL_SCOPE(scope->prev) ? NULL : scope->prev;

    if(IS_FN_SCOPE(scope)){
        assert(manager->depth > 0 && "depth must be greater than 0");

        manager->depth--;
        manager->fn_scope_stack = AS_FN_SCOPE(scope)->prev_fn;
    }

    lzarena_restore(manager->manager_arena, scope->arena_state);

    Scope *current_scope = scope_manager_peek(manager);

    if(IS_GLOBAL_SCOPE(current_scope)){
        assert(manager->fn_scope_stack == NULL && "procedure scopes must be empty");

        lzarena_restore(manager->manager_arena, current_scope->arena_state);
        manager->local_symbols_pool = NULL;
    }

    lzpool_dealloc(scope);
}

inline int scope_manager_is_global_scope(const ScopeManager *manager){
    return manager->scope_stack == NULL;
}

inline int scope_manager_is_scope_type(const ScopeManager *manager, ScopeType type){
    Scope *current = scope_manager_peek(manager);

    do{
    	if(current->type == FN_SCOPE_TYPE){
   			return 0;
     	}

     	if(current->type == type){
            return 1;
        }

        current = current->prev;
    }while(current);

    return 0;
}

inline int scope_manager_exists_procedure_name(
    const ScopeManager *manager,
    size_t name_len,
    const char *name
){
    Scope *global_scope = manager->global_scope;
    LZOHTable *symbols = global_scope->symbols;

    return lzohtable_lookup(
        name_len,
        name,
        symbols,
        NULL
    );
}

inline size_t scope_manager_locals_count(const ScopeManager *manager){
    Scope *current = scope_manager_peek(manager);
    assert(!IS_GLOBAL_SCOPE(current));
    return current->symbols->n;
}

Symbol *scope_manager_get_symbol(ScopeManager *manager, Token *identifier){
	Scope *current = scope_manager_peek(manager);
    Symbol *symbol = exists(current, identifier);

    if(!symbol){
        error(
            manager,
            identifier,
            "Symbol '%s' doesn't exists",
            identifier->lexeme
        );
    }

    return symbol;
}

LocalSymbol *scope_manager_define_local(
    ScopeManager *manager,
    uint8_t is_mutable,
    uint8_t is_initialized,
    const Token *identifier_token
){
    Scope *scope = scope_manager_peek(manager);

    assert(IS_LOCAL_SCOPE(scope) && "Scope must be local");

	if(exists_local(scope, identifier_token)){
		error(
			manager,
			identifier_token,
			"Already exists symbol '%s' in current scope",
			identifier_token->lexeme
		);
	}

	LocalSymbol *local_symbol = lzpool_alloc_x(1024, manager->local_symbols_pool);
	Symbol *symbol = &local_symbol->symbol;

	symbol->type = LOCAL_SYMBOL_TYPE;
	symbol->identifier = identifier_token;
    symbol->scope = scope;
	local_symbol->offset = generate_local_offset(manager, scope, identifier_token);
	local_symbol->is_mutable = is_mutable;
	local_symbol->is_initialized = is_initialized;

    lzohtable_put_ck(
        identifier_token->lexeme_len,
        identifier_token->lexeme,
        symbol,
        scope->symbols,
        NULL
    );

	return local_symbol;
}

GlobalSymbol *scope_manager_define_global(
    ScopeManager *manager,
    uint8_t is_mutable,
    const Token *identifier_token
){
    Scope *scope = scope_manager_peek(manager);

    assert(scope->type == GLOBAL_SCOPE_TYPE && "Scope must be global");

	if(exists_local(scope, identifier_token)){
		error(
			manager,
			identifier_token,
			"Already exists symbol '%s' in current scope",
			identifier_token->lexeme
		);
	}

	GlobalSymbol *global_symbol = lzpool_alloc_x(1024, manager->global_symbols_pool);
	Symbol *symbol = &global_symbol->symbol;

	symbol->type = GLOBAL_SYMBOL_TYPE;
	symbol->identifier = identifier_token;
    symbol->scope = scope;
	global_symbol->is_mutable = is_mutable;

    lzohtable_put_ck(
        identifier_token->lexeme_len,
        identifier_token->lexeme,
        symbol,
        scope->symbols,
        NULL
    );

	return global_symbol;
}

NativeFnSymbol *scope_manager_define_native_fn(
    ScopeManager *manager,
    uint8_t arity,
    const char *name
){
    Scope *scope = scope_manager_peek(manager);
	NativeFnSymbol *native_fn_symbol = lzpool_alloc_x(64, manager->native_fn_symbols_pool);
	Symbol *symbol = &native_fn_symbol->symbol;

	symbol->type = NATIVE_FN_SYMBOL_TYPE;
	symbol->identifier = NULL;
    symbol->scope = scope;
	native_fn_symbol->params_count = arity;
    native_fn_symbol->name = name;

    lzohtable_put_ck(
        strlen(name),
        name,
        symbol,
        scope->symbols,
        NULL
    );

	return native_fn_symbol;
}

FnSymbol *scope_manager_define_fn(
    ScopeManager *manager,
    uint8_t arity,
    const Token *identifier_token
){
    Scope *scope = scope_manager_peek(manager);

	if(exists_local(scope, identifier_token)){
		error(
			manager,
			identifier_token,
			"Already exists symbol '%s' in current scope",
			identifier_token->lexeme
		);
	}

	FnSymbol *fn_symbol = lzpool_alloc_x(1024, manager->fn_symbols_pool);
	Symbol *symbol = &fn_symbol->symbol;

	symbol->type = FN_SYMBOL_TYPE;
	symbol->identifier = identifier_token;
    symbol->scope = scope;
	fn_symbol->params_count = arity;

    lzohtable_put_ck(
        identifier_token->lexeme_len,
        identifier_token->lexeme,
        symbol,
        scope->symbols,
        NULL
    );

	return fn_symbol;
}

ModuleSymbol *scope_manager_define_module(
    ScopeManager *manager,
    const Token *identifier_token
){
    Scope *scope = scope_manager_peek(manager);

	if(exists_local(scope, identifier_token)){
		error(
			manager,
			identifier_token,
			"Already exists symbol '%s' in current scope",
			identifier_token->lexeme
		);
	}

	ModuleSymbol *module_symbol = lzpool_alloc_x(1024, manager->module_symbols_pool);
	Symbol *symbol = &module_symbol->symbol;

	symbol->type = MODULE_SYMBOL_TYPE;
	symbol->identifier = identifier_token;
    symbol->scope = scope;

    lzohtable_put_ck(
        identifier_token->lexeme_len,
        identifier_token->lexeme,
        symbol,
        scope->symbols,
        NULL
    );

	return module_symbol;
}
