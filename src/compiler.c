#include "compiler.h"
#include "memory.h"
#include "factory.h"
#include "native_module/native_module_nbarray.h"
#include "utils.h"
#include "token.h"
#include "expr.h"
#include "stmt.h"
#include "opcode.h"
#include "lexer.h"
#include "parser.h"
#include "native_module/native_module_math.h"
#include "native_module/native_module_random.h"
#include "native_module/native_module_time.h"
#include "native_module/native_module_io.h"
#include "native_module/native_module_os.h"
#include <stdint.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#define CTALLOCATOR (compiler->ctallocator)
#define RTALLOCATOR (compiler->rtallocator)
#define LABELS_ALLOCATOR (compiler->labels_allocator)
#define CURRENT_MODULE (compiler->current_module)
//--------------------------------------------------------------------------//
//                            PRIVATE INTERFACE                             //
//--------------------------------------------------------------------------//
//--------------------------------  ERROR  ---------------------------------//
static void error(Compiler *compiler, Token *token, char *msg, ...);
static void fatal_error(Compiler *compiler, char *fmt, ...);
//--------------------------------  SCOPE  ---------------------------------//
Scope *scope_in(ScopeType type, Compiler *compiler);
Scope *scope_in_soft(ScopeType type, Compiler *compiler);
Scope *scope_in_fn(char *name, Compiler *compiler, Fn **out_function, size_t *out_fn_index);
void scope_out(Compiler *compiler);
void scope_out_fn(Compiler *compiler);
Scope *inside_loop(Compiler *compiler);
Scope *inside_fn(Compiler *compiler);
Scope *inside_fn_from_symbol(Symbol *symbol, Compiler *compiler);
int count_fn_scopes(int from, int to, Compiler *compiler);
Scope *current_scope(Compiler *compiler);
Scope *previous_scope(Scope *scope, Compiler *compiler);
//--------------------------------  LABEL  ---------------------------------//
void insert_label(Scope *scope, Label *label);
Label *label(Compiler *compiler, char *fmt, ...);
void insert_jmp(Scope *scope, JMP *jmp);
void remove_jmp(JMP *jmp);
JMP *jif(Compiler *compiler, Token *ref_token, char *fmt, ...);
JMP *jmp(Compiler *compiler, Token *ref_token, char *fmt, ...);
void resolve_jmps(Scope *current, Compiler *compiler);
//--------------------------------  SYMBOL  --------------------------------//
int resolve_symbol(Token *identifier, Symbol *symbol, Compiler *compiler);
Symbol *exists_scope(char *name, Scope *scope, Compiler *compiler);
Symbol *exists_local(char *name, Compiler *compiler);
int exists_global(char *name, Compiler *compiler);
Symbol *get(Token *identifier_token, Compiler *compiler);
void to_local(Symbol *symbol, Compiler *compiler);
void from_symbols_to_global(size_t symbol_index, Token *location_token, char *name, Compiler *compiler);
Symbol *declare_native_fn(char *identifier, Compiler *compiler);
Symbol *declare(SymbolType type, Token *identifier_token, Compiler *compiler);
DynArr *current_chunks(Compiler *compiler);
DynArr *current_locations(Compiler *compiler);
DynArr *current_constants(Compiler *compiler);
DynArr *current_float_values(Compiler *compiler);
//--------------------------------  CHUNKS  --------------------------------//
void descompose_i16(int16_t value, uint8_t *bytes);
void descompose_i32(int32_t value, uint8_t *bytes);
size_t chunks_len(Compiler *compiler);
size_t write_chunk(uint8_t chunk, Compiler *compiler);
void write_location(Token *token, Compiler *compiler);
void update_chunk(size_t index, uint8_t chunk, Compiler *compiler);
size_t write_i16(int16_t i16, Compiler *compiler);
size_t write_i32(int32_t i32, Compiler *compiler);
size_t write_iconst(int64_t i64, Compiler *compiler);
size_t write_fconst(double value, Compiler *compiler);
void write_str(size_t len, char *rstr, Compiler *compiler);
void write_str_alloc(size_t len, char *rstr, Compiler *compiler);
void update_i16(size_t index, int16_t i16, Compiler *compiler);
void update_i32(size_t index, int32_t i32, Compiler *compiler);
//--------------------------------  OTHER  ---------------------------------//
int32_t generate_id(Compiler *compiler);
uint32_t generate_trycatch_id(Compiler *compiler);
char *names_to_pathname(DynArr *names, Compiler *compiler, size_t *out_name_len, Token **out_name_token);
NativeModule *resolve_native_module(char *module_name, Compiler *compiler);
char *resolve_module_location(
    Token *import_token,
    char *module_name,
    Module *previous_module,
    Module *current_module,
    Compiler *compiler,
    size_t *out_module_name_len,
    size_t *out_module_pathname_len
);
void compile_expr(Expr *expr, Compiler *compiler);
size_t add_native_module_symbol(NativeModule *module, DynArr *symbols);
size_t add_module_symbol(Module *module, DynArr *symbols);
void compile_stmt(Stmt *stmt, Compiler *compiler);
void define_natives(Compiler *compiler);
void define_fns_prototypes(DynArr *fns_prototypes, Compiler *compiler);
void add_captured_symbol(Token *identifier, Symbol *symbol, Scope *scope, Compiler *compiler);
void set_fn(size_t index, Fn *fn, Compiler *compiler);
void set_closure(size_t index, MetaClosure *closure, Compiler *compiler);
//--------------------------------------------------------------------------//
//                          PRIVATE IMPLEMENTATION                          //
//--------------------------------------------------------------------------//
static void error(Compiler *compiler, Token *token, char *msg, ...){
    va_list args;
    va_start(args, msg);

    fprintf(stderr, "Compiler error at line %d in file '%s':\n\t", token->line, token->pathname);
    vfprintf(stderr, msg, args);
    fprintf(stderr, "\n");

    va_end(args);
    longjmp(compiler->err_jmp, 1);
}

static void fatal_error(Compiler *compiler, char *fmt, ...){
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "COMPILER FATAL ERROR:\n\t");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");

    va_end(args);
    longjmp(compiler->err_jmp, 1);
}

Scope *scope_in(ScopeType type, Compiler *compiler){
    Scope *scope = &compiler->scopes[compiler->depth++];

    scope->id = -1;
    scope->depth = compiler->depth;
    scope->locals = 0;
    scope->scope_locals = 0;
	scope->type = type;
    scope->symbols_len = 0;
    scope->captured_symbols_len = 0;

    return scope;
}

Scope *scope_in_soft(ScopeType type, Compiler *compiler){
    Scope *enclosing = &compiler->scopes[compiler->depth - 1];
    Scope *scope = &compiler->scopes[compiler->depth++];

    scope->id = -1;
    scope->depth = compiler->depth;
    scope->locals = enclosing->locals;
    scope->scope_locals = 0;
	scope->type = type;
    scope->symbols_len = 0;

    return scope;
}

Scope *scope_in_fn(char *name, Compiler *compiler, Fn **out_function, size_t *out_symbol_index){
    assert(compiler->fn_ptr < FUNCTIONS_LENGTH);

    Fn *fn = factory_create_fn(name, compiler->current_module, RTALLOCATOR);
    compiler->fn_stack[compiler->fn_ptr++] = fn;

    DynArr *symbols = MODULE_SYMBOLS(compiler->current_module);

    SubModuleSymbol module_symbol = {0};
    dynarr_insert(&module_symbol, symbols);

    if(out_function){
        *out_function = fn;
    }

    if(out_symbol_index){
        *out_symbol_index = DYNARR_LEN(symbols) - 1;
    }

    return scope_in(FUNCTION_SCOPE, compiler);
}

void scope_out(Compiler *compiler){
    Scope *current = current_scope(compiler);

    resolve_jmps(current, compiler);

    if(current->depth > 1){
        for (uint8_t i = 0; i < current->scope_locals; i++){
            write_chunk(POP_OPCODE, compiler);
        }
    }

    compiler->depth--;
}

void scope_out_fn(Compiler *compiler){
    assert(compiler->fn_ptr > 0);

    Scope *scope = current_scope(compiler);

    resolve_jmps(scope, compiler);

    // A scope with depth 2 is any function which parent scope is GLOBAL.
    // Iff scope's depth == 2, we clean up labels arena to be able to reuse it
    if(scope->depth == 2){
        lzarena_free_all((LZArena *)LABELS_ALLOCATOR->ctx);
    }

    compiler->fn_ptr--;
    compiler->depth--;
}

Scope *inside_loop(Compiler *compiler){
	assert(compiler->depth > 0);

	for(int i = (int)compiler->depth - 1; i >= 0; i--){
		Scope *scope = compiler->scopes + i;

		if(scope->type == WHILE_SCOPE || scope->type == FOR_SCOPE){
            return scope;
        }

		if(scope->type == FUNCTION_SCOPE){
            return NULL;
        }
	}

	return NULL;
}

Scope *inside_fn(Compiler *compiler){
	assert(compiler->depth > 0);

	for(int i = (int)compiler->depth - 1; i >= 0; i--){
		Scope *scope = compiler->scopes + i;

		if(scope->type == FUNCTION_SCOPE){
            if(i == 0){
                return NULL;
            }

            return scope;
        }
	}

	return NULL;
}

Scope *inside_fn_from_symbol(Symbol *symbol, Compiler *compiler){
    assert(compiler->depth > 0);

	for(int i = (int)symbol->depth - 1; i >= 0; i--){
		Scope *scope = compiler->scopes + i;

		if(scope->type == FUNCTION_SCOPE){
            if(i == 0){
                return NULL;
            }

            return scope;
        }
	}

	return NULL;
}

int count_fn_scopes(int from, int to, Compiler *compiler){
    assert(compiler->depth > 1);

    assert(from < to);
    assert(to < compiler->depth);
    assert(compiler->scopes[from].type == FUNCTION_SCOPE);
    assert(compiler->scopes[to].type == FUNCTION_SCOPE);

    int count = 0;

    for(int i = from + 1; i < to; i++){
		Scope *scope = compiler->scopes + i;

        if(scope->type == FUNCTION_SCOPE){
            count++;
        }
	}

    return count;
}

Scope *current_scope(Compiler *compiler){
    assert(compiler->depth > 0);
    return &compiler->scopes[compiler->depth - 1];
}

Scope *previous_scope(Scope *scope, Compiler *compiler){
    if(scope->depth == 1) return NULL;
    return &compiler->scopes[scope->depth - 1];
}

void insert_label(Scope *scope, Label *label){
    LabelList *list = &scope->labels;

    if(list->tail){
        list->tail->next = label;
        label->prev = list->tail;
    }else{
        list->head = label;
    }

    list->tail = label;
}

Label *label(Compiler *compiler, char *fmt, ...){
    va_list args;
    va_start(args, fmt);

    Scope *scope = current_scope(compiler);
    Allocator *allocator = scope->depth == 1 ? CTALLOCATOR : LABELS_ALLOCATOR;

    char *name = MEMORY_ALLOC(allocator, char, LABEL_NAME_LENGTH);
    int out_count = vsnprintf(name, LABEL_NAME_LENGTH, fmt, args);

    va_end(args);
    assert(out_count < LABEL_NAME_LENGTH);

    Label *label = MEMORY_ALLOC(allocator, Label, 1);

    label->name = name;
    label->location = chunks_len(compiler);
    label->prev = NULL;
    label->next = NULL;

    insert_label(scope, label);

    return label;
}

void insert_jmp(Scope *scope, JMP *jmp){
    JMPList *list = &scope->jmps;

    if(list->tail){
        list->tail->next = jmp;
        jmp->prev = list->tail;
    }else{
        list->head = jmp;
    }

    list->tail = jmp;
    jmp->scope = scope;
}

void remove_jmp(JMP *jmp){
    Scope *scope = (Scope *)jmp->scope;
    JMPList *list = &scope->jmps;

    if(jmp == list->head){
        list->head = jmp->next;
    }
    if(jmp == list->tail){
        list->tail = jmp->prev;
    }

    if(jmp->prev){
        jmp->prev->next = jmp->next;
    }
    if(jmp->next){
        jmp->next->prev = jmp->prev;
    }
}

JMP *jif(Compiler *compiler, Token *ref_token, char *fmt, ...){
    size_t bef = chunks_len(compiler);

    write_chunk(JIF_OPCODE, compiler);
    write_location(ref_token, compiler);

    size_t idx = write_i16(0, compiler);
    size_t af = chunks_len(compiler);

    va_list args;
    va_start(args, fmt);

    Scope *scope = current_scope(compiler);
    Allocator *allocator = scope->depth == 1 ? CTALLOCATOR : LABELS_ALLOCATOR;

    char *label = MEMORY_ALLOC(allocator, char, LABEL_NAME_LENGTH);
    int out_count = vsnprintf(label, LABEL_NAME_LENGTH, fmt, args);

    va_end(args);
    assert(out_count < LABEL_NAME_LENGTH);

    JMP *jmp = (JMP *)MEMORY_ALLOC(allocator, JMP, 1);

    jmp->bef = bef;
    jmp->af = af;
    jmp->idx = idx;
    jmp->label = label;
    jmp->prev = NULL;
    jmp->next = NULL;

    insert_jmp(scope, jmp);

    return jmp;
}

JMP *jit(Compiler *compiler, Token *ref_token, char *fmt, ...){
    size_t bef = chunks_len(compiler);

    write_chunk(JIT_OPCODE, compiler);
    write_location(ref_token, compiler);

    size_t idx = write_i16(0, compiler);
    size_t af = chunks_len(compiler);

    va_list args;
    va_start(args, fmt);

    Scope *scope = current_scope(compiler);
    Allocator *allocator = LABELS_ALLOCATOR;

    char *label = MEMORY_ALLOC(allocator, char, LABEL_NAME_LENGTH);
    int out_count = vsnprintf(label, LABEL_NAME_LENGTH, fmt, args);

    va_end(args);
    assert(out_count < LABEL_NAME_LENGTH);

    JMP *jmp = (JMP *)MEMORY_ALLOC(allocator, JMP, 1);

    jmp->bef = bef;
    jmp->af = af;
    jmp->idx = idx;
    jmp->label = label;
    jmp->prev = NULL;
    jmp->next = NULL;

    insert_jmp(scope, jmp);

    return jmp;
}

JMP *jmp(Compiler *compiler, Token *ref_token, char *fmt, ...){
    size_t bef = chunks_len(compiler);

    write_chunk(JMP_OPCODE, compiler);
    write_location(ref_token, compiler);

    size_t idx = write_i16(0, compiler);
    size_t af = chunks_len(compiler);

    va_list args;
    va_start(args, fmt);

    Scope *scope = current_scope(compiler);
    Allocator *allocator = scope->depth == 1 ? CTALLOCATOR : LABELS_ALLOCATOR;

    char *label = MEMORY_ALLOC(allocator, char, LABEL_NAME_LENGTH);
    int out_count = vsnprintf(label, LABEL_NAME_LENGTH, fmt, args);

    va_end(args);
    assert(out_count < LABEL_NAME_LENGTH);

    JMP *jmp = (JMP *)MEMORY_ALLOC(allocator, JMP, 1);

    jmp->bef = bef;
    jmp->af = af;
    jmp->idx = idx;
    jmp->label = label;
    jmp->prev = NULL;
    jmp->next = NULL;

    insert_jmp(scope, jmp);

    return jmp;
}

void resolve_jmps(Scope *current, Compiler *compiler){
    LabelList *labels = &current->labels;
    JMPList *jmps = &current->jmps;

    for(JMP *jmp = jmps->head; jmp; jmp = jmp->next){
        Label *selected = NULL;

        for (Label *label = labels->head; label; label = label->next){
            if(strcmp(jmp->label, label->name) == 0){
                selected = label;
                break;
            }
        }

        if(selected){
            if(jmp->idx > selected->location){
                update_i16(jmp->idx, -(jmp->bef - selected->location), compiler);
            }else{
                update_i16(jmp->idx, selected->location - jmp->af + 1, compiler);
            }

            remove_jmp(jmp);
        }else{
            assert(compiler->depth > 1 && "All jmps must be resolved");

            Scope *enclosing = &compiler->scopes[current->depth - 2];

            remove_jmp(jmp);
            insert_jmp(enclosing, jmp);
        }
    }

    labels->head = NULL;
    labels->tail = NULL;
}

int resolve_symbol(Token *identifier, Symbol *symbol, Compiler *compiler){
    int is_out = 0;

    if(symbol->depth > 1 && compiler->depth >= 2){
        // depth 1 means we are in the global scope or the main function
        Scope *fn_scope = inside_fn(compiler);

        if(fn_scope && symbol->depth < fn_scope->depth){
            is_out = 1;
            add_captured_symbol(identifier, symbol, fn_scope, compiler);
        }
    }

    return is_out;
}

Symbol *exists_scope(char *name, Scope *scope, Compiler *compiler){
    for (int i = scope->symbols_len - 1; i >= 0; i--){
        Symbol *symbol = &scope->symbols[i];

        if(strcmp(symbol->name, name) == 0)
            return symbol;
    }

    return NULL;
}

Symbol *exists_local(char *name, Compiler *compiler){
    Scope *scope = current_scope(compiler);
    return exists_scope(name, scope, compiler);
}

int exists_global(char *name, Compiler *compiler){
    assert(compiler->depth > 0);

    Scope *scope = &compiler->scopes[0];

    return exists_scope(name, scope, compiler) != NULL;
}

Symbol *get(Token *identifier_token, Compiler *compiler){
    assert(compiler->depth > 0);

    char *identifier = identifier_token->lexeme;

    for (int i = compiler->depth - 1; i >= 0; i--){
        Scope *scope = &compiler->scopes[i];
        Symbol *symbol = exists_scope(identifier, scope, compiler);
        if(symbol != NULL) return symbol;
    }

    error(compiler, identifier_token, "Symbol '%s' doesn't exists.", identifier);

    return NULL;
}

void to_local(Symbol *symbol, Compiler *compiler){
    assert(symbol->identifier_token);

    Scope *scope = &compiler->scopes[symbol->depth - 1];

    if(scope->symbols_len >= SYMBOLS_LENGTH){
        error(compiler, symbol->identifier_token, "Cannot define more than %d symbols per scope", SYMBOLS_LENGTH);
    }
    if(scope->locals >= LOCALS_LENGTH){
        error(compiler, symbol->identifier_token, "Cannot define more than %d locals", LOCALS_LENGTH);
    }

    symbol->local = scope->locals++;
}

void from_symbols_to_global(
    size_t symbol_index,
    Token *location_token,
    char *name,
    Compiler *compiler
){
    write_chunk(SGET_OPCODE, compiler);
    write_location(location_token, compiler);
    write_i32(symbol_index, compiler);

    write_chunk(GDEF_OPCODE, compiler);
    write_location(location_token, compiler);
    write_str_alloc(strlen(name), name, compiler);
}

Symbol *declare_native_fn(char *identifier, Compiler *compiler){
    size_t identifier_len = strlen(identifier);
    Scope *scope = current_scope(compiler);
    Symbol *symbol = &scope->symbols[scope->symbols_len++];

    symbol->depth = scope->depth;
    symbol->local = -1;
    symbol->type = NATIVE_FN_SYMTYPE;
    memcpy(symbol->name, identifier, identifier_len);
    symbol->name[identifier_len] = '\0';
    symbol->identifier_token = NULL;

    return symbol;
}

Symbol *declare_unknown(Token *ref_token, SymbolType type, Compiler *compiler){
    Scope *scope = current_scope(compiler);

    if(scope->symbols_len >= SYMBOLS_LENGTH){
        error(compiler, ref_token, "Cannot define more than %d symbols per scope", SYMBOLS_LENGTH);
    }

    int local = 0;
    Symbol *symbol = &scope->symbols[scope->symbols_len++];

	if(type == MUT_SYMTYPE || type == IMUT_SYMTYPE){
        if(scope->depth == 1){
            local = -1;
        }else{
            if(scope->locals >= LOCALS_LENGTH){
                error(compiler, ref_token, "Cannot define more than %d locals", LOCALS_LENGTH);
            }

            local = scope->locals++;
        }
    }

    symbol->depth = scope->depth;
    symbol->local = local;
    symbol->index = -1;
    symbol->type = type;
    symbol->name[0] = 0;
    symbol->identifier_token = NULL;

    scope->scope_locals++;

    return symbol;
}

Symbol *declare(SymbolType type, Token *identifier_token, Compiler *compiler){
    Scope *scope = current_scope(compiler);

    if(scope->symbols_len >= SYMBOLS_LENGTH){
        error(compiler, identifier_token, "Cannot define more than %d symbols per scope", SYMBOLS_LENGTH);
    }

    char *identifier = identifier_token->lexeme;
    size_t identifier_len = strlen(identifier);

	if(identifier_len + 1 >= SYMBOL_NAME_LENGTH){
        error(compiler, identifier_token, "Symbol name '%s' too long.", identifier);
    }
    if(exists_local(identifier, compiler) != NULL){
        error(compiler, identifier_token, "Already exists a symbol named as '%s'.", identifier);
    }

    int local = 0;
    Symbol *symbol = &scope->symbols[scope->symbols_len++];

	if(type == MUT_SYMTYPE || type == IMUT_SYMTYPE){
        if(scope->depth == 1){
            local = -1;
        }else{
            if(scope->locals >= LOCALS_LENGTH){
                error(compiler, identifier_token, "Cannot define more than %d locals", LOCALS_LENGTH);
            }

            local = scope->locals++;
        }
    }

    symbol->depth = scope->depth;
    symbol->local = local;
    symbol->index = -1;
    symbol->type = type;
    memcpy(symbol->name, identifier, identifier_len);
    symbol->name[identifier_len] = '\0';
    symbol->identifier_token = identifier_token;

    scope->scope_locals++;

    return symbol;
}

DynArr *current_chunks(Compiler *compiler){
	assert(compiler->fn_ptr > 0 && compiler->fn_ptr < FUNCTIONS_LENGTH);
	Fn *fn = compiler->fn_stack[compiler->fn_ptr - 1];

    return fn->chunks;
}

DynArr *current_locations(Compiler *compiler){
	assert(compiler->fn_ptr > 0 && compiler->fn_ptr < FUNCTIONS_LENGTH);
	Fn *fn = compiler->fn_stack[compiler->fn_ptr - 1];

    return fn->locations;
}

DynArr *current_constants(Compiler *compiler){
    assert(compiler->fn_ptr > 0 && compiler->fn_ptr < FUNCTIONS_LENGTH);
    Fn *fn = compiler->fn_stack[compiler->fn_ptr - 1];
    return fn->iconsts;
}

DynArr *current_float_values(Compiler *compiler){
    assert(compiler->fn_ptr > 0 && compiler->fn_ptr < FUNCTIONS_LENGTH);
    Fn *fn = compiler->fn_stack[compiler->fn_ptr - 1];
    return fn->fconsts;
}

void descompose_i16(int16_t value, uint8_t *bytes){
    uint8_t mask = 0b11111111;

    for (size_t i = 0; i < 2; i++){
        uint8_t r = value & mask;
        value = value >> 8;
        bytes[i] = r;
    }
}

void descompose_i32(int32_t value, uint8_t *bytes){
    uint8_t mask = 0b11111111;

    for (size_t i = 0; i < 4; i++){
        uint8_t r = value & mask;
        value = value >> 8;
        bytes[i] = r;
    }
}

size_t chunks_len(Compiler *compiler){
	return current_chunks(compiler)->used;
}

size_t write_chunk(uint8_t chunk, Compiler *compiler){
    DynArr *chunks = current_chunks(compiler);
	size_t index = chunks->used;
    dynarr_insert(&chunk, chunks);
	return index;
}

void write_location(Token *token, Compiler *compiler){
	DynArr *chunks = current_chunks(compiler);
	DynArr *locations = current_locations(compiler);
	OPCodeLocation location = {0};

	location.offset = chunks->used - 1;
	location.line = token->line;
    location.filepath = factory_clone_raw_str(token->pathname, RTALLOCATOR, NULL);

	dynarr_insert(&location, locations);
}

void update_chunk(size_t index, uint8_t chunk, Compiler *compiler){
	DynArr *chunks = current_chunks(compiler);
    dynarr_set_at(index, &chunk, chunks);
}

size_t write_i16(int16_t i16, Compiler *compiler){
    size_t index = chunks_len(compiler);

	uint8_t bytes[2];
	descompose_i16(i16, bytes);

	for(size_t i = 0; i < 2; i++)
		write_chunk(bytes[i], compiler);

	return index;
}

size_t write_i32(int32_t i32, Compiler *compiler){
	size_t index = chunks_len(compiler);

	uint8_t bytes[4];
	descompose_i32(i32, bytes);

	for(size_t i = 0; i < 4; i++)
		write_chunk(bytes[i], compiler);

	return index;
}

size_t write_iconst(int64_t i64, Compiler *compiler){
    DynArr *constants = current_constants(compiler);
    int32_t used = (int32_t) constants->used;

    assert(used < 32767 && "Too many constants");

    dynarr_insert(&i64, constants);

	return write_i16(used, compiler);
}

size_t write_fconst(double value, Compiler *compiler){
	DynArr *float_values = current_float_values(compiler);
	int32_t length = (int32_t) float_values->used;

	assert(length < 32767 && "Too many constants");

	dynarr_insert(&value, float_values);

	return write_i16(length, compiler);
}

void write_str(size_t len, char *rstr, Compiler *compiler){
    DynArr *static_strs = MODULE_STRINGS(CURRENT_MODULE);
    size_t idx = DYNARR_LEN(static_strs);

    if(idx >= UINT16_MAX){
        fatal_error(compiler, "Only " PRIu16 " strings literals per module are allowed", UINT16_MAX);
    }

    RawStr str = (RawStr){
        .len = len,
        .buff = rstr
    };

    dynarr_insert(&str, static_strs);
    write_i16((int16_t)idx, compiler);
}

void write_str_alloc(size_t len, char *rstr, Compiler *compiler){
    assert(len > 0 && "'len' must be greater than 0");

    char *new_rstr = MEMORY_ALLOC(RTALLOCATOR, char, len + 1);
    DynArr *static_strs = MODULE_STRINGS(CURRENT_MODULE);
    size_t idx = DYNARR_LEN(static_strs);

    memcpy(new_rstr, rstr, len);
    new_rstr[len] = 0;

    if(idx >= UINT16_MAX){
        fatal_error(compiler, "Only " PRIu16 " strings literals per module are allowed", UINT16_MAX);
    }

    RawStr str = (RawStr){
        .len = len,
        .buff = new_rstr
    };

    dynarr_insert(&str, static_strs);
    write_i16((int16_t)idx, compiler);
}

void update_i16(size_t index, int16_t i16, Compiler *compiler){
    uint8_t bytes[2];
	DynArr *chunks = current_chunks(compiler);

	descompose_i16(i16, bytes);

	for(size_t i = 0; i < 2; i++){
        dynarr_set_at(index + i, bytes + i, chunks);
    }
}

void update_i32(size_t index, int32_t i32, Compiler *compiler){
	uint8_t bytes[4];
	DynArr *chunks = current_chunks(compiler);

	descompose_i32(i32, bytes);

	for(size_t i = 0; i < 4; i++){
        dynarr_set_at(index + i, bytes + i, chunks);
    }
}

inline int32_t generate_id(Compiler *compiler){
    return ++compiler->counter_id;
}

inline uint32_t generate_trycatch_id(Compiler *compiler){
    return ++(compiler->trycatch_counter);
}

char *names_to_pathname(DynArr *names, Compiler *compiler, size_t *out_name_len, Token **out_name_token){
    size_t names_len = DYNARR_LEN(names);

    if(names_len == 1){
        Token *name_token = (Token *)dynarr_get_raw(0, names);

        *out_name_len = name_token->lexeme_len;
        *out_name_token = name_token;

        return factory_clone_raw_str(name_token->lexeme, CTALLOCATOR, out_name_len);
    }

    LZBStr *str_buff = FACTORY_LZBSTR(CTALLOCATOR);
    Token *name_token;

    for (size_t i = 0; i < names_len; i++){
        name_token = (Token *)dynarr_get_raw(i, names);

        lzbstr_append(name_token->lexeme, str_buff);

        if(i + 1 < names_len){
            lzbstr_append("/", str_buff);
        }
    }

    *out_name_token = name_token;

    return lzbstr_rclone_buff((LZBStrAllocator *)CTALLOCATOR, str_buff, out_name_len);
}

NativeModule *resolve_native_module(char *module_name, Compiler *compiler){
	if(strcmp("math", module_name) == 0){
		if(!math_native_module){
			math_module_init(RTALLOCATOR);
		}

		return math_native_module;
	}

    if(strcmp("random", module_name) == 0){
        if(!random_native_module){
            random_module_init(RTALLOCATOR);
        }

        return random_native_module;
    }

    if(strcmp("time", module_name) == 0){
        if(!time_native_module){
            time_module_init(RTALLOCATOR);
        }

        return time_native_module;
    }

    if(strcmp("io", module_name) == 0){
        if(!io_native_module){
            io_module_init(RTALLOCATOR);
        }

        return io_native_module;
    }

    if(strcmp("os", module_name) == 0){
        if(!os_native_module){
            os_module_init(RTALLOCATOR);
        }

        return os_native_module;
    }

    if(strcmp("nbarray", module_name) == 0){
    	if(!nbarray_native_module){
   			nbarray_module_init(RTALLOCATOR);
     	}

     	return nbarray_native_module;
    }

	return NULL;
}

char *resolve_module_location(
    Token *import_token,
    char *module_name,
    Module *previous_module,
    Module *current_module,
    Compiler *compiler,
    size_t *out_module_name_len,
    size_t *out_module_pathname_len
){
    size_t module_name_len = strlen(module_name);
    DynArr *search_paths = compiler->search_paths;
    size_t search_paths_len = DYNARR_LEN(search_paths);

    for (size_t i = 0; i < search_paths_len; i++){
        RawStr search_path_rstr = DYNARR_GET_AS(RawStr, i, search_paths);
        size_t search_pathname_len = search_path_rstr.len;
        char *search_pathname = search_path_rstr.buff;

        size_t module_pathname_len = search_pathname_len + module_name_len + 4;
        char module_pathname[module_pathname_len + 1];

        memcpy(module_pathname, search_pathname, search_pathname_len);
        memcpy(module_pathname + search_pathname_len, "/", 1);
        memcpy(module_pathname + search_pathname_len + 1, module_name, module_name_len);
        memcpy(module_pathname + search_pathname_len + 1 + module_name_len, ".ze", 3);
        module_pathname[module_pathname_len] = '\0';

        if(!UTILS_FILES_EXISTS(module_pathname)){
            continue;
        }

		if(!UTILS_FILES_CAN_READ(module_pathname)){
			error(compiler, import_token, "File at '%s' do not exists or cannot be read", module_pathname);
		}

        if(!utils_files_is_regular(module_pathname)){
            error(compiler, import_token, "File at '%s' is not a regular file", module_pathname);
        }

        // detect self module import
        if(strcmp(module_pathname, current_module->pathname) == 0){
            error(compiler, import_token, "Trying to import the current module '%s'", module_name);
        }

        if(previous_module){
            // detect circular dependency
            if(strcmp(module_pathname, previous_module->pathname) == 0){
                error(compiler, import_token, "Circular dependency between '%s' and '%s'", current_module->pathname, previous_module->pathname);
            }
        }

        if(out_module_name_len){
            *out_module_name_len = module_name_len;
        }

        if(out_module_pathname_len){
            *out_module_pathname_len = module_pathname_len;
        }

        return factory_clone_raw_str(module_pathname, CTALLOCATOR, NULL);
    }

    error(compiler, import_token, "Module %s not found", module_name);

    return NULL;
}

void compile_expr(Expr *expr, Compiler *compiler){
    switch (expr->type){
        case EMPTY_EXPRTYPE:{
			EmptyExpr *empty_expr = (EmptyExpr *)expr->sub_expr;
            write_chunk(EMPTY_OPCODE, compiler);
			write_location(empty_expr->empty_token, compiler);
            break;
        }case BOOL_EXPRTYPE:{
            BoolExpr *bool_expr = (BoolExpr *)expr->sub_expr;
            uint8_t value = bool_expr->value;

            write_chunk(value ? TRUE_OPCODE : FALSE_OPCODE, compiler);
			write_location(bool_expr->bool_token, compiler);

            break;
        }case INT_EXPRTYPE:{
            IntExpr *int_expr = (IntExpr *)expr->sub_expr;
            Token *token = int_expr->token;
            int64_t value = *(int64_t *)token->literal;

            write_chunk(INT_OPCODE, compiler);
			write_location(token, compiler);

            write_iconst(value, compiler);

            break;
        }case FLOAT_EXPRTYPE:{
			FloatExpr *float_expr = (FloatExpr *)expr->sub_expr;
			Token *float_token = float_expr->token;
			double value = *(double *)float_token->literal;

			write_chunk(FLOAT_OPCODE, compiler);
			write_location(float_token, compiler);

			write_fconst(value, compiler);

			break;
		}case STRING_EXPRTYPE:{
			StrExpr *str_expr = (StrExpr *)expr->sub_expr;
            Token *str_token = str_expr->str_token;

			write_chunk(STRING_OPCODE, compiler);
			write_location(str_token, compiler);
			write_str(str_token->literal_size, str_token->literal, compiler);

			break;
		}case TEMPLATE_EXPRTYPE:{
            TemplateExpr *template_expr = (TemplateExpr *)expr->sub_expr;
            Token *template_token = template_expr->template_token;
            DynArr *exprs = template_expr->exprs;

            write_chunk(STTE_OPCODE, compiler);
            write_location(template_token, compiler);

            if(exprs){
                size_t len = DYNARR_LEN(exprs);

                for (size_t i = 0; i < len; i++){
                    Expr *expr = (Expr *)dynarr_get_ptr(i, exprs);
                    compile_expr(expr, compiler);

                    write_chunk(WTTE_OPCODE, compiler);
                    write_location(template_token, compiler);
                }
            }

            write_chunk(ETTE_OPCODE, compiler);
            write_location(template_token, compiler);

            break;
        }case ANON_EXPRTYPE:{
            AnonExpr *anon_expr = (AnonExpr *)expr->sub_expr;
            Token *anon_token = anon_expr->anon_token;
            DynArr *params = anon_expr->params;
            DynArr *stmts = anon_expr->stmts;

            Fn *fn = NULL;
            size_t symbol_index = 0;
            Scope *fn_scope = scope_in_fn("anonymous", compiler, &fn, &symbol_index);

            if(params){
                for (size_t i = 0; i < DYNARR_LEN(params); i++){
                    Token *param_identifier = (Token *)dynarr_get_ptr(i, params);
                    declare(MUT_SYMTYPE, param_identifier, compiler);

                    char *name = factory_clone_raw_str(param_identifier->lexeme, RTALLOCATOR, NULL);
                    dynarr_insert_ptr(name, fn->params);
                }
            }

            char returned = 0;

            for (size_t i = 0; i < DYNARR_LEN(stmts); i++){
                Stmt *stmt = (Stmt *)dynarr_get_ptr(i, stmts);
                compile_stmt(stmt, compiler);

                if(i + 1 >= DYNARR_LEN(stmts) && stmt->type == RETURN_STMTTYPE){
                    returned = 1;
                }
            }

            if(!returned){
                write_chunk(EMPTY_OPCODE, compiler);
                write_location(anon_token, compiler);

                write_chunk(RET_OPCODE, compiler);
                write_location(anon_token, compiler);
            }

            scope_out_fn(compiler);

            if(fn_scope->captured_symbols_len > 0){
                MetaClosure *closure = MEMORY_ALLOC(RTALLOCATOR, MetaClosure, 1);

                closure->meta_out_values_len = fn_scope->captured_symbols_len;

                for (int i = 0; i < fn_scope->captured_symbols_len; i++){
                    closure->meta_out_values[i].at = fn_scope->captured_symbols[i]->local;
                }

                closure->fn = fn;

                set_closure(symbol_index, closure, compiler);
            }else{
                set_fn(symbol_index, fn, compiler);
            }

            write_chunk(SGET_OPCODE, compiler);
            write_location(anon_token, compiler);
            write_i32((int32_t)symbol_index, compiler);

            break;
        }case IDENTIFIER_EXPRTYPE:{
            IdentifierExpr *identifier_expr = (IdentifierExpr *)expr->sub_expr;
            Token *identifier = identifier_expr->identifier_token;

            Symbol *symbol = get(identifier, compiler);
            int is_out = resolve_symbol(identifier, symbol, compiler);

            if(symbol->type == NATIVE_FN_SYMTYPE){
                write_chunk(NGET_OPCODE, compiler);
                write_location(identifier, compiler);
                write_str_alloc(identifier->lexeme_len, identifier->lexeme, compiler);

                break;
            }

            if(symbol->depth == 1){
                write_chunk(GGET_OPCODE, compiler);
                write_location(identifier, compiler);
                write_str_alloc(identifier->lexeme_len, identifier->lexeme, compiler);
            }else{
                uint8_t opcode = is_out ? OGET_OPCODE : LGET_OPCODE;

                write_chunk(opcode, compiler);
                write_location(identifier, compiler);
                write_chunk((uint8_t)symbol->local, compiler);
            }

            break;
        }case GROUP_EXPRTYPE:{
            GroupExpr *group_expr = (GroupExpr *)expr->sub_expr;
            Expr *expr = group_expr->expr;

            compile_expr(expr, compiler);

            break;
        }case CALL_EXPRTYPE:{
            CallExpr *call_expr = (CallExpr *)expr->sub_expr;
            Expr *left = call_expr->left;
            DynArr *args = call_expr->args;

            compile_expr(left, compiler);

            if(args){
                size_t len = DYNARR_LEN(args);

                for (size_t i = 0; i < len; i++){
                    compile_expr((Expr *)dynarr_get_ptr(i, args), compiler);
                }
            }

            write_chunk(CALL_OPCODE, compiler);
            write_location(call_expr->left_paren, compiler);

            uint8_t args_count = args ? (uint8_t)DYNARR_LEN(args) : 0;
            write_chunk(args_count, compiler);

            break;
        }case ACCESS_EXPRTYPE:{
            AccessExpr *access_expr = (AccessExpr *)expr->sub_expr;
            Expr *left = access_expr->left;
            Token *symbol_token = access_expr->symbol_token;

            compile_expr(left, compiler);

            write_chunk(ACCESS_OPCODE, compiler);
			write_location(access_expr->dot_token, compiler);
            write_str_alloc(symbol_token->lexeme_len, symbol_token->lexeme, compiler);

            break;
        }case INDEX_EXPRTYPE:{
            IndexExpr *index_expr = (IndexExpr *)expr->sub_expr;
            Expr *target = index_expr->target;
            Token *left_square_token = index_expr->left_square_token;
            Expr *index = index_expr->index_expr;

            compile_expr(index, compiler);
            compile_expr(target, compiler);

            write_chunk(INDEX_OPCODE, compiler);
            write_location(left_square_token, compiler);

            break;
        }case UNARY_EXPRTYPE:{
            UnaryExpr *unary_expr = (UnaryExpr *)expr->sub_expr;
            Token *operator = unary_expr->operator;
            Expr *right = unary_expr->right;

            compile_expr(right, compiler);

            switch (operator->type){
                case MINUS_TOKTYPE:{
                    write_chunk(NNOT_OPCODE, compiler);
                    break;
                }case EXCLAMATION_TOKTYPE:{
                    write_chunk(NOT_OPCODE, compiler);
                    break;
                }case NOT_BITWISE_TOKTYPE:{
                    write_chunk(BNOT_OPCODE, compiler);
                    break;
                }default:{
                    assert("Illegal token type");
                }
            }

			write_location(operator, compiler);

            break;
        }case BINARY_EXPRTYPE:{
            BinaryExpr *binary_expr = (BinaryExpr *)expr->sub_expr;
            Expr *left = binary_expr->left;
            Token *operator = binary_expr->operator;
            Expr *right = binary_expr->right;

            compile_expr(left, compiler);
            compile_expr(right, compiler);

            switch (operator->type){
                case PLUS_TOKTYPE:{
                    write_chunk(ADD_OPCODE, compiler);
                    break;
                }
                case MINUS_TOKTYPE:{
                    write_chunk(SUB_OPCODE, compiler);
                    break;
                }
                case ASTERISK_TOKTYPE:{
                    write_chunk(MUL_OPCODE, compiler);
                    break;
                }
                case SLASH_TOKTYPE:{
                    write_chunk(DIV_OPCODE, compiler);
                    break;
                }
				case MOD_TOKTYPE:{
					write_chunk(MOD_OPCODE, compiler);
					break;
				}
                default:{
                    assert("Illegal token type");
                }
            }

			write_location(operator, compiler);

            break;
        }case MULSTR_EXPRTYPE:{
            MulStrExpr *mulstr_expr = (MulStrExpr *)expr->sub_expr;
            Expr *left = mulstr_expr->left;
            Token *operator = mulstr_expr->operator;
            Expr *right = mulstr_expr->right;

            compile_expr(left, compiler);
            compile_expr(right, compiler);

            write_chunk(MULSTR_OPCODE, compiler);
            write_location(operator, compiler);

            break;
        } case CONCAT_EXPRTYPE:{
            ConcatExpr *concat_expr = (ConcatExpr *)expr->sub_expr;
            Expr *left = concat_expr->left;
            Token *operator = concat_expr->operator;
            Expr *right = concat_expr->right;

            compile_expr(left, compiler);
            compile_expr(right, compiler);

            write_chunk(CONCAT_OPCODE, compiler);
            write_location(operator, compiler);

            break;
        }case BITWISE_EXPRTYPE:{
            BitWiseExpr *bitwise_expr = (BitWiseExpr *)expr->sub_expr;
            Expr *left = bitwise_expr->left;
            Token *operator = bitwise_expr->operator;
            Expr *right = bitwise_expr->right;

            compile_expr(left, compiler);
            compile_expr(right, compiler);

            switch (operator->type)
            {
                case LEFT_SHIFT_TOKTYPE:{
                    write_chunk(LSH_OPCODE, compiler);
                    break;
                }case RIGHT_SHIFT_TOKTYPE:{
                    write_chunk(RSH_OPCODE, compiler);
                    break;
                }case AND_BITWISE_TOKTYPE:{
                    write_chunk(BAND_OPCODE, compiler);
                    break;
                }case XOR_BITWISE_TOKTYPE:{
                    write_chunk(BXOR_OPCODE, compiler);
                    break;
                }case OR_BITWISE_TOKTYPE:{
                    write_chunk(BOR_OPCODE, compiler);
                    break;
                }default:{
                    assert("Illegal token type");
                }
            }

            write_location(operator, compiler);

            break;
        }case COMPARISON_EXPRTYPE:{
            ComparisonExpr *comparison_expr = (ComparisonExpr *)expr->sub_expr;
            Expr *left = comparison_expr->left;
            Token *operator = comparison_expr->operator;
            Expr *right = comparison_expr->right;

            compile_expr(left, compiler);
            compile_expr(right, compiler);

            switch (operator->type){
                case LESS_TOKTYPE:{
                    write_chunk(LT_OPCODE, compiler);
                    break;
                }case GREATER_TOKTYPE:{
                    write_chunk(GT_OPCODE, compiler);
                    break;
                }case LESS_EQUALS_TOKTYPE:{
                    write_chunk(LE_OPCODE, compiler);
                    break;
                }case GREATER_EQUALS_TOKTYPE:{
                    write_chunk(GE_OPCODE, compiler);
                    break;
                }case EQUALS_EQUALS_TOKTYPE:{
                    write_chunk(EQ_OPCODE, compiler);
                    break;
                }case NOT_EQUALS_TOKTYPE:{
                    write_chunk(NE_OPCODE, compiler);
                    break;
                }default:{
                    assert("Illegal token type");
                }
            }

			write_location(operator, compiler);

            break;
        }case LOGICAL_EXPRTYPE:{
            LogicalExpr *logical_expr = (LogicalExpr *)expr->sub_expr;
            Expr *left = logical_expr->left;
            Token *operator = logical_expr->operator;
            Expr *right = logical_expr->right;

            compile_expr(left, compiler);

            switch (operator->type){
                case OR_TOKTYPE:{
                    write_chunk(JIT_OPCODE, compiler);
                    write_location(operator, compiler);
                    size_t jit_index = write_i16(0, compiler);

                    size_t len_before = chunks_len(compiler);

                    write_chunk(FALSE_OPCODE, compiler);
                    write_location(operator, compiler);

                    compile_expr(right, compiler);
                    write_chunk(OR_OPCODE, compiler);

                    write_chunk(JMP_OPCODE, compiler);
                    write_location(operator, compiler);
                    size_t jmp_index = write_i16(0, compiler);

                    size_t len_after = chunks_len(compiler);
                    size_t len = len_after - len_before;

                    update_i16(jit_index, len + 1, compiler);

                    len_before = chunks_len(compiler);

                    write_chunk(TRUE_OPCODE, compiler);
                    write_location(operator, compiler);

                    len_after = chunks_len(compiler);
                    len = len_after - len_before;

                    update_i16(jmp_index, len + 1, compiler);

                    break;
                }
                case AND_TOKTYPE:{
                    write_chunk(JIF_OPCODE, compiler);
                    write_location(operator, compiler);
                    size_t jif_index = write_i16(0, compiler);

                    size_t len_before = chunks_len(compiler);

                    write_chunk(TRUE_OPCODE, compiler);
                    write_location(operator, compiler);

                    compile_expr(right, compiler);
                    write_chunk(AND_OPCODE, compiler);

                    write_chunk(JMP_OPCODE, compiler);
                    write_location(operator, compiler);
                    size_t jmp_index = write_i16(0, compiler);

                    size_t len_after = chunks_len(compiler);
                    size_t len = len_after - len_before;

                    update_i16(jif_index, len + 1, compiler);

                    len_before = chunks_len(compiler);

                    write_chunk(FALSE_OPCODE, compiler);
                    write_location(operator, compiler);

                    len_after = chunks_len(compiler);
                    len = len_after - len_before;

                    update_i16(jmp_index, len + 1, compiler);

                    break;
                }
                default:{
                    assert("Illegal token type");
                }
            }

			write_location(operator, compiler);

            break;
        }case ASSIGN_EXPRTYPE:{
            AssignExpr *assign_expr = (AssignExpr *)expr->sub_expr;
			Expr *left = assign_expr->left;
			Token *equals_token = assign_expr->equals_token;
            Expr *value_expr = assign_expr->value_expr;

			if(left->type == IDENTIFIER_EXPRTYPE){
				IdentifierExpr *identifier_expr = (IdentifierExpr *)left->sub_expr;
				Token *identifier = identifier_expr->identifier_token;
	            Symbol *symbol = get(identifier, compiler);

    	        if(symbol->type == IMUT_SYMTYPE){
                    error(compiler, identifier, "'%s' declared as constant. Can not change its value", identifier->lexeme);
                }
                if(symbol->type == FN_SYMTYPE){
                    error(compiler, identifier, "'%s' declared as function. Functions symbols are protected", identifier->lexeme);
                }
                if(symbol->type == MODULE_SYMTYPE){
                    error(compiler, identifier, "'%s' declared as module. Modules symbols are protected", identifier->lexeme);
                }

                int is_out = resolve_symbol(identifier, symbol, compiler);

            	compile_expr(value_expr, compiler);

	            if(symbol->depth == 1){
                	write_chunk(GSET_OPCODE, compiler);
   					write_location(equals_token, compiler);
                	write_str_alloc(identifier->lexeme_len, identifier->lexeme, compiler);
            	}else{
                    uint8_t opcode = is_out ? OSET_OPCODE : LSET_OPCODE;

                	write_chunk(opcode, compiler);
					write_location(equals_token, compiler);
                	write_chunk(symbol->local, compiler);
            	}

				break;
			}

			if(left->type == ACCESS_EXPRTYPE){
				AccessExpr *access_expr = (AccessExpr *)left->sub_expr;
				Expr *left = access_expr->left;
				Token *symbol_token = access_expr->symbol_token;

				compile_expr(value_expr, compiler);
                compile_expr(left, compiler);

				write_chunk(RSET_OPCODE, compiler);
				write_location(equals_token, compiler);
				write_str_alloc(symbol_token->lexeme_len, symbol_token->lexeme, compiler);

				break;
			}

            if(left->type == INDEX_EXPRTYPE){
                IndexExpr *index_expr = (IndexExpr *)left->sub_expr;
                Expr *target = index_expr->target;
                Expr *index = index_expr->index_expr;

                compile_expr(value_expr, compiler);
                compile_expr(index, compiler);
                compile_expr(target, compiler);

                write_chunk(ASET_OPCODE, compiler);
                write_location(equals_token, compiler);

                break;
            }

			error(compiler, equals_token, "Illegal assign target");

            break;
        }case COMPOUND_EXPRTYPE:{
			CompoundExpr *compound_expr = (CompoundExpr *)expr->sub_expr;
            Expr *left = compound_expr->left;
			Token *operator = compound_expr->operator;
			Expr *right = compound_expr->right;

            switch (left->type){
                case IDENTIFIER_EXPRTYPE:{
                    IdentifierExpr *identifier_expr = (IdentifierExpr *)left->sub_expr;
                    Token *identifier_token = identifier_expr->identifier_token;
                    Symbol *symbol = get(identifier_token, compiler);

                    if(symbol->type == IMUT_SYMTYPE){
                        error(
                            compiler,
                            identifier_token,
                            "'%s' declared as constant. Can't change its value.",
                            identifier_token->lexeme
                        );
                    }

                    if(symbol->depth == 1){
                        write_chunk(GGET_OPCODE, compiler);
                        write_location(identifier_token, compiler);
                        write_str_alloc(identifier_token->lexeme_len, identifier_token->lexeme, compiler);
                    }else{
                        write_chunk(LGET_OPCODE, compiler);
                        write_location(identifier_token, compiler);
                        write_chunk(symbol->local, compiler);
                    }

                    compile_expr(right, compiler);

                    switch(operator->type){
                        case COMPOUND_ADD_TOKTYPE:{
                            write_chunk(ADD_OPCODE, compiler);
                            break;
                        }case COMPOUND_SUB_TOKTYPE:{
                            write_chunk(SUB_OPCODE, compiler);
                            break;
                        }case COMPOUND_MUL_TOKTYPE:{
                            write_chunk(MUL_OPCODE, compiler);
                            break;
                        }case COMPOUND_DIV_TOKTYPE:{
                            write_chunk(DIV_OPCODE, compiler);
                            break;
                        }default:{
                            assert("Illegal compound type");
                        }
                    }

                    write_location(operator, compiler);

                    if(symbol->depth == 1){
                        write_chunk(GSET_OPCODE, compiler);
                        write_location(identifier_token, compiler);
                        write_str_alloc(identifier_token->lexeme_len, identifier_token->lexeme, compiler);
                    }else{
                        write_chunk(LSET_OPCODE, compiler);
                        write_location(identifier_token, compiler);
                        write_chunk(symbol->local, compiler);
                    }

                    break;
                }case ACCESS_EXPRTYPE:{
                    compile_expr(left, compiler);

                    AccessExpr *access_expr = (AccessExpr *)left->sub_expr;
                    Expr *left = access_expr->left;
                    Token *dot_token = access_expr->dot_token;
                    Token *symbol_token = access_expr->symbol_token;

                    compile_expr(right, compiler);

                    switch(operator->type){
                        case COMPOUND_ADD_TOKTYPE:{
                            write_chunk(ADD_OPCODE, compiler);
                            break;
                        }case COMPOUND_SUB_TOKTYPE:{
                            write_chunk(SUB_OPCODE, compiler);
                            break;
                        }case COMPOUND_MUL_TOKTYPE:{
                            write_chunk(MUL_OPCODE, compiler);
                            break;
                        }case COMPOUND_DIV_TOKTYPE:{
                            write_chunk(DIV_OPCODE, compiler);
                            break;
                        }default:{
                            assert("Illegal compound type");
                        }
                    }

                    write_location(operator, compiler);

                    compile_expr(left, compiler);

                    write_chunk(RSET_OPCODE, compiler);
                    write_location(dot_token, compiler);
                    write_str_alloc(symbol_token->lexeme_len, symbol_token->lexeme, compiler);

                    break;
                }default:{
                    error(compiler, operator, "Illegal compound operator left operand");
                }
            }

			break;
		}case ARRAY_EXPRTYPE:{
            ArrayExpr *array_expr = (ArrayExpr *)expr->sub_expr;
            Token *array_token = array_expr->array_token;
            Expr *len_expr = array_expr->len_expr;
            DynArr *values = array_expr->values;

            if(len_expr){
                compile_expr(len_expr, compiler);
                write_chunk(ARRAY_OPCODE, compiler);
                write_location(array_token, compiler);
            }else{
                const size_t array_len = values ? DYNARR_LEN(values) : 0;

                write_chunk(INT_OPCODE, compiler);
                write_location(array_token, compiler);
                write_iconst((int64_t)array_len, compiler);

                write_chunk(ARRAY_OPCODE, compiler);
                write_location(array_token, compiler);

                for (size_t i = 0 ; i < array_len; i++){
                    Expr *expr = (Expr *)dynarr_get_ptr(i, values);

                    compile_expr(expr, compiler);

                    write_chunk(IARRAY_OPCODE, compiler);
                    write_location(array_token, compiler);
                    write_i16((int16_t)i, compiler);
                }
            }

            break;
        }case LIST_EXPRTYPE:{
			ListExpr *list_expr = (ListExpr *)expr->sub_expr;
			Token *list_token = list_expr->list_token;
			DynArr *exprs = list_expr->exprs;

            write_chunk(LIST_OPCODE, compiler);
			write_location(list_token, compiler);

            if(exprs){
                const size_t len = DYNARR_LEN(exprs);

				for(size_t i = 0; i < len; i++){
                    Expr *expr = (Expr *)dynarr_get_ptr(i, exprs);

                    compile_expr(expr, compiler);

                    write_chunk(ILIST_OPCODE, compiler);
                    write_location(list_token, compiler);
				}
			}

			break;
		}case DICT_EXPRTYPE:{
            DictExpr *dict_expr = (DictExpr *)expr->sub_expr;
            DynArr *key_values = dict_expr->key_values;

            write_chunk(DICT_OPCODE, compiler);
			write_location(dict_expr->dict_token, compiler);

            if(key_values){
                size_t len = DYNARR_LEN(key_values);

                for (size_t i = 0; i < len; i++){
                    DictKeyValue *key_value = (DictKeyValue *)dynarr_get_ptr(i, key_values);
                    Expr *key = key_value->key;
                    Expr *value = key_value->value;

                    compile_expr(key, compiler);
                    compile_expr(value, compiler);

                    write_chunk(IDICT_OPCODE, compiler);
			        write_location(dict_expr->dict_token, compiler);
                }
            }

            break;
        }case RECORD_EXPRTYPE:{
			RecordExpr *record_expr = (RecordExpr *)expr->sub_expr;
            Token *record_token = record_expr->record_token;
			DynArr *key_values = record_expr->key_values;
            size_t key_values_len = key_values ? DYNARR_LEN(key_values) : 0;

            write_chunk(RECORD_OPCODE, compiler);
			write_location(record_token, compiler);
            write_i16((int16_t)key_values_len, compiler);

			for(size_t i = 0; i < key_values_len; i++){
                RecordExprValue *key_value = (RecordExprValue *)dynarr_get_ptr(i, key_values);
                Token *key = key_value->key;
                Expr *value = key_value->value;

                compile_expr(value, compiler);

                write_chunk(IRECORD_OPCODE, compiler);
                write_location(record_token, compiler);
                write_str_alloc(key->lexeme_len, key->lexeme, compiler);
            }

			break;
		}case IS_EXPRTYPE:{
			IsExpr *is_expr = (IsExpr *)expr->sub_expr;
			Expr *left_expr = is_expr->left_expr;
			Token *is_token = is_expr->is_token;
			Token *type_token = is_expr->type_token;

			compile_expr(left_expr, compiler);

			write_chunk(IS_OPCODE, compiler);
			write_location(is_token, compiler);

			switch(type_token->type){
				case EMPTY_TOKTYPE:{
					write_chunk(0, compiler);
					break;
				}case BOOL_TOKTYPE:{
					write_chunk(1, compiler);
					break;
				}case INT_TOKTYPE:{
					write_chunk(2, compiler);
					break;
				}case FLOAT_TOKTYPE:{
					write_chunk(3, compiler);
					break;
				}case STR_TOKTYPE:{
					write_chunk(4, compiler);
					break;
				}case ARRAY_TOKTYPE:{
					write_chunk(5, compiler);
					break;
				}case LIST_TOKTYPE:{
					write_chunk(6, compiler);
					break;
				}case DICT_TOKTYPE:{
					write_chunk(7, compiler);
					break;
				}case RECORD_TOKTYPE:{
                    write_chunk(8, compiler);
					break;
                }case PROC_TOKTYPE:{
                    write_chunk(9, compiler);
                    break;
                }default:{
					assert("Illegal type value");
				}
			}

			break;
		}case TENARY_EXPRTYPE:{
		     TenaryExpr *tenary_expr = (TenaryExpr *)expr->sub_expr;
		     Expr *condition = tenary_expr->condition;
		     Expr *left = tenary_expr->left;
		     Token *mark_token = tenary_expr->mark_token;
		     Expr *right = tenary_expr->right;

		     compile_expr(condition, compiler);

		     write_chunk(JIF_OPCODE, compiler);
		     write_location(mark_token, compiler);

             size_t jif_index = write_i16(0, compiler);

             size_t left_before = chunks_len(compiler);
		     compile_expr(left, compiler);

             write_chunk(JMP_OPCODE, compiler);
             write_location(mark_token, compiler);

             size_t jmp_index = write_i16(0, compiler);

		     size_t left_after = chunks_len(compiler);
		     size_t left_len = left_after - left_before;

		     size_t right_before =chunks_len(compiler);
		     compile_expr(right, compiler);
		     size_t right_after = chunks_len(compiler);
		     size_t right_len = right_after - right_before;

             update_i16(jmp_index, (int16_t)right_len + 1, compiler);
		     update_i16(jif_index, (int16_t)left_len + 1, compiler);

		     break;
		}default:{
            assert("Illegal expression type");
        }
    }
}

size_t add_native_module_symbol(NativeModule *module, DynArr *symbols){
    SubModuleSymbol module_symbol = (SubModuleSymbol ){
        .type = NATIVE_MODULE_SUBMODULE_SYM_TYPE,
        .value = module
    };

    dynarr_insert(&module_symbol, symbols);

    return DYNARR_LEN(symbols) - 1;
}

size_t add_module_symbol(Module *module, DynArr *symbols){
	SubModuleSymbol module_symbol = (SubModuleSymbol ){
        .type = MODULE_SUBMODULE_SYM_TYPE,
        .value = module
    };

    dynarr_insert(&module_symbol, symbols);

    return DYNARR_LEN(symbols) - 1;
}

void compile_stmt(Stmt *stmt, Compiler *compiler){
    switch (stmt->type){
        case EXPR_STMTTYPE:{
            ExprStmt *expr_stmt = (ExprStmt *)stmt->sub_stmt;
            Expr *expr = expr_stmt->expr;

            compile_expr(expr, compiler);
            write_chunk(POP_OPCODE, compiler);

            break;
        }case VAR_DECL_STMTTYPE:{
            VarDeclStmt *var_decl_stmt = (VarDeclStmt *)stmt->sub_stmt;
            char is_const = var_decl_stmt->is_const;
            char is_initialized = var_decl_stmt->is_initialized;
            Token *identifier_token = var_decl_stmt->identifier_token;
            Expr *initializer_expr = var_decl_stmt->initializer_expr;

            if(is_const && !is_initialized){
                error(compiler, identifier_token, "'%s' declared as constant, but is not being initialized.", identifier_token->lexeme);
            }

            if(initializer_expr == NULL){
                write_chunk(EMPTY_OPCODE, compiler);
            }else{
                compile_expr(initializer_expr, compiler);
            }

            Symbol *symbol = declare(is_const ? IMUT_SYMTYPE : MUT_SYMTYPE, identifier_token, compiler);

            if(symbol->depth == 1){
                write_chunk(GDEF_OPCODE, compiler);
				write_location(identifier_token, compiler);
                write_str_alloc(identifier_token->lexeme_len, identifier_token->lexeme, compiler);
            }

            break;
        }case BLOCK_STMTTYPE:{
            BlockStmt *block_stmt = (BlockStmt *)stmt->sub_stmt;
            DynArr *stmts = block_stmt->stmts;

            scope_in_soft(BLOCK_SCOPE, compiler);

            for (size_t i = 0; i < DYNARR_LEN(stmts); i++){
                compile_stmt((Stmt *)dynarr_get_ptr(i, stmts), compiler);
            }

            scope_out(compiler);

            break;
        }case IF_STMTTYPE:{
            int32_t id = generate_id(compiler);

			IfStmt *if_stmt = (IfStmt *)stmt->sub_stmt;
            IfStmtBranch *if_branch = if_stmt->if_branch;
            DynArr *elif_branches = if_stmt->elif_branches;
            DynArr *else_stmts = if_stmt->else_stmts;

            Token *if_branch_token = if_branch->branch_token;
            Expr *if_condition = if_branch->condition_expr;
            DynArr *if_stmts = if_branch->stmts;

            compile_expr(if_condition, compiler);
            jif(compiler, if_branch_token, "IFB_END_%d", id);

            scope_in_soft(IF_SCOPE, compiler);

            if(if_stmts){
                for (size_t i = 0; i < DYNARR_LEN(if_stmts); i++){
                    Stmt *stmt = (Stmt *)dynarr_get_ptr(i, if_stmts);
                    compile_stmt(stmt, compiler);
                }
            }

            scope_out(compiler);

            jmp(compiler, if_branch_token, "IF_END_%d", id);
            label(compiler, "IFB_END_%d", id);

            if(elif_branches){
                for (size_t branch_idx = 0; branch_idx < DYNARR_LEN(elif_branches); branch_idx++){
                    IfStmtBranch *elif_branch = (IfStmtBranch *)dynarr_get_ptr(branch_idx, elif_branches);

                    Token *elif_branch_token = elif_branch->branch_token;
                    Expr *elif_condition = elif_branch->condition_expr;
                    DynArr *elif_stmts = elif_branch->stmts;

                    compile_expr(elif_condition, compiler);
                    jif(compiler, elif_branch_token, "ELIF_END_%d_%zu", id, branch_idx);

                    scope_in_soft(IF_SCOPE, compiler);

                    for (size_t i = 0; i < DYNARR_LEN(elif_stmts); i++){
                        Stmt *stmt = (Stmt *)dynarr_get_ptr(i, elif_stmts);
                        compile_stmt(stmt, compiler);
                    }

                    scope_out(compiler);

                    jmp(compiler, elif_branch_token, "IF_END_%d", id);
                    label(compiler, "ELIF_END_%d_%zu", id, branch_idx);
                }
            }

            if(else_stmts){
                scope_in_soft(IF_SCOPE, compiler);

                for (size_t i = 0; i < DYNARR_LEN(else_stmts); i++){
                    Stmt *stmt = (Stmt *)dynarr_get_ptr(i, else_stmts);
                    compile_stmt(stmt, compiler);
                }

                scope_out(compiler);
            }

            label(compiler, "IF_END_%d", id);

			break;
		}case WHILE_STMTTYPE:{
            int32_t while_id = generate_id(compiler);

			WhileStmt *while_stmt = (WhileStmt *)stmt->sub_stmt;
            Token *while_token = while_stmt->while_token;
			Expr *condition = while_stmt->condition;
			DynArr *stmts = while_stmt->stmts;

            jmp(compiler, while_token, "WHILE_CONDITION_%d", while_id);
            label(compiler, "WHILE_BODY_%d", while_id);

			Scope *scope = scope_in_soft(WHILE_SCOPE, compiler);
            scope->id = while_id;

			for(size_t i = 0; i < DYNARR_LEN(stmts); i++){
				compile_stmt((Stmt *)dynarr_get_ptr(i, stmts), compiler);
			}

			scope_out(compiler);

            label(compiler, "WHILE_CONDITION_%d", while_id);
			compile_expr(condition, compiler);
            jit(compiler, while_token, "WHILE_BODY_%d", while_id);
            label(compiler, "WHILE_END_%d", while_id);

			break;
		}case FOR_RANGE_STMTTYPE:{
            int32_t for_id = generate_id(compiler);

            ForRangeStmt *for_range_stmt = (ForRangeStmt *)stmt->sub_stmt;
            Token *for_token = for_range_stmt->for_token;
            Token *symbol_token = for_range_stmt->symbol_token;
            Expr *left_expr = for_range_stmt->left_expr;
            Token *for_type_token = for_range_stmt->for_type_token;
            Expr *right_expr = for_range_stmt->right_expr;
            DynArr *stmts = for_range_stmt->stmts;

            scope_in_soft(BLOCK_SCOPE, compiler);

            if(for_type_token->type == UPTO_TOKTYPE){
                compile_expr(left_expr, compiler);
                Symbol *symbol = declare(IMUT_SYMTYPE, symbol_token, compiler);

                compile_expr(right_expr, compiler);
                Symbol *right_symbol = declare_unknown(symbol_token, IMUT_SYMTYPE, compiler);

                label(compiler, "FOR_RANGE_START_%d", for_id);
                write_chunk(LGET_OPCODE, compiler);
                write_location(symbol_token, compiler);
                write_chunk(symbol->local, compiler);

                write_chunk(LGET_OPCODE, compiler);
                write_location(for_token, compiler);
                write_chunk(right_symbol->local, compiler);

                write_chunk(GE_OPCODE, compiler);
                write_location(for_token, compiler);

                jit(compiler, for_token, "FOR_RANGE_END_%d", for_id);

                Scope *scope = scope_in_soft(FOR_SCOPE, compiler);
                scope->id = for_id;

                if(stmts){
                    for (size_t i = 0; i < DYNARR_LEN(stmts); i++){
                        Stmt *stmt = (Stmt *)dynarr_get_ptr(i, stmts);
                        compile_stmt(stmt, compiler);
                    }
                }

                scope_out(compiler);

                label(compiler, "FOR_RANGE_CONDITION_%d", for_id);
                write_chunk(LGET_OPCODE, compiler);
                write_location(symbol_token, compiler);
                write_chunk(symbol->local, compiler);

                write_chunk(CINT_OPCODE, compiler);
                write_location(for_token, compiler);
                write_chunk(1, compiler);

                write_chunk(ADD_OPCODE, compiler);
                write_location(for_token, compiler);

                write_chunk(LSET_OPCODE, compiler);
                write_location(symbol_token, compiler);
                write_chunk(symbol->local, compiler);

                write_chunk(POP_OPCODE, compiler);
                write_location(for_token, compiler);

                jmp(compiler, for_token, "FOR_RANGE_START_%d", for_id);

                label(compiler, "FOR_RANGE_END_%d", for_id);
            }

            if(for_type_token->type == DOWNTO_TOKTYPE){
                compile_expr(right_expr, compiler);
                Symbol *symbol = declare(IMUT_SYMTYPE, symbol_token, compiler);

                compile_expr(left_expr, compiler);
                Symbol *left_symbol = declare_unknown(symbol_token, IMUT_SYMTYPE, compiler);

                jmp(compiler, for_token, "FOR_RANGE_CONDITION_%d", for_id);

                label(compiler, "FOR_RANGE_START_%d", for_id);
                write_chunk(LGET_OPCODE, compiler);
                write_chunk(symbol->local, compiler);

                write_chunk(LGET_OPCODE, compiler);
                write_chunk(left_symbol->local, compiler);

                write_chunk(GE_OPCODE, compiler);

                jif(compiler, for_token, "FOR_RANGE_END_%d", for_id);

                Scope *scope = scope_in_soft(FOR_SCOPE, compiler);
                scope->id = for_id;

                if(stmts){
                    for (size_t i = 0; i < DYNARR_LEN(stmts); i++){
                        Stmt *stmt = (Stmt *)dynarr_get_ptr(i, stmts);
                        compile_stmt(stmt, compiler);
                    }
                }

                scope_out(compiler);

                label(compiler, "FOR_RANGE_CONDITION_%d", for_id);
                write_chunk(LGET_OPCODE, compiler);
                write_location(symbol_token, compiler);
                write_chunk(symbol->local, compiler);

                write_chunk(CINT_OPCODE, compiler);
                write_location(for_token, compiler);
                write_chunk(1, compiler);

                write_chunk(SUB_OPCODE, compiler);
                write_location(for_token, compiler);

                write_chunk(LSET_OPCODE, compiler);
                write_location(symbol_token, compiler);
                write_chunk(symbol->local, compiler);

                write_chunk(POP_OPCODE, compiler);
                write_location(for_token, compiler);

                jmp(compiler, for_token, "FOR_RANGE_START_%d", for_id);

                label(compiler, "FOR_RANGE_END_%d", for_id);
            }

            scope_out(compiler);

            break;
        } case STOP_STMTTYPE:{
			StopStmt *stop_stmt = (StopStmt *)stmt->sub_stmt;
			Token *stop_token = stop_stmt->stop_token;

            Scope *scope = inside_loop(compiler);

            assert(scope->id != -1);

            if(!scope){
                error(compiler, stop_token, "Can't use 'stop' statement outside loops.");
            }

            if(scope->type == WHILE_SCOPE){
                jmp(compiler, stop_token, "WHILE_END_%d", scope->id);
            }

            if(scope->type == FOR_SCOPE){
                jmp(compiler, stop_token, "FOR_RANGE_END_%d", scope->id);
            }

			break;
		}case CONTINUE_STMTTYPE:{
            ContinueStmt *continue_stmt = (ContinueStmt *)stmt->sub_stmt;
            Token *continue_token = continue_stmt->continue_token;

            Scope *scope = inside_loop(compiler);

            assert(scope->id != -1);

            if(!scope){
                error(compiler, continue_token, "Can't use 'continue' statement outside loops.");
            }

            if(scope->type == WHILE_SCOPE){
                jmp(compiler, continue_token, "WHILE_CONDITION_%d", scope->id);
            }

            if(scope->type == FOR_SCOPE){
                jmp(compiler, continue_token, "FOR_RANGE_CONDITION_%d", scope->id);
            }

            break;
        }case RETURN_STMTTYPE:{
            ReturnStmt *return_stmt = (ReturnStmt *)stmt->sub_stmt;
			Token *return_token = return_stmt->return_token;
            Expr *value = return_stmt->value;

			Scope *scope = inside_fn(compiler);

			if(!scope || scope->depth == 1)
				error(compiler, return_token, "Can't use 'return' statement outside functions.");

            if(value) compile_expr(value, compiler);
            else {
                write_chunk(EMPTY_OPCODE, compiler);
                write_location(return_token, compiler);
            }

            write_chunk(RET_OPCODE, compiler);
            write_location(return_token, compiler);

            break;
        }case FUNCTION_STMTTYPE:{
            FunctionStmt *function_stmt = (FunctionStmt *)stmt->sub_stmt;
            Token *identifier_token = function_stmt->identifier_token;
            DynArr *params = function_stmt->params;
            DynArr *stmts = function_stmt->stmts;

            Scope *enclosing = inside_fn(compiler);

            Fn *fn = NULL;
            size_t symbol_index = 0;

            Symbol *fn_symbol = exists_global(identifier_token->lexeme, compiler) ?
                                get(identifier_token, compiler) :
                                declare(FN_SYMTYPE, identifier_token, compiler);

            Scope *fn_scope = scope_in_fn(identifier_token->lexeme, compiler, &fn, &symbol_index);

            fn_symbol->index = ((int)symbol_index);

            if(params){
                for (size_t i = 0; i < DYNARR_LEN(params); i++){
                    Token *param_token = (Token *)dynarr_get_ptr(i, params);
                    char *param_name = factory_clone_raw_str(param_token->lexeme, RTALLOCATOR, NULL);

                    declare(MUT_SYMTYPE, param_token, compiler);
                    dynarr_insert_ptr(param_name, fn->params);
                }
            }

            if(stmts){
                char return_empty = 1;
                size_t stmts_len = DYNARR_LEN(stmts);

                for (size_t i = 0; i < stmts_len; i++){
                    Stmt *stmt = (Stmt *)dynarr_get_ptr(i, stmts);
                    compile_stmt(stmt, compiler);

                    if(i == stmts_len - 1 && stmt->type == RETURN_STMTTYPE){
                        return_empty = 0;
                    }
                }

                if(return_empty){
                    write_chunk(EMPTY_OPCODE, compiler);
                    write_location(identifier_token, compiler);

                    write_chunk(RET_OPCODE, compiler);
                    write_location(identifier_token, compiler);
                }
            }

            scope_out_fn(compiler);

            int is_normal = enclosing == NULL;

            if(fn_scope->captured_symbols_len > 0){
                MetaClosure *closure = MEMORY_ALLOC(RTALLOCATOR, MetaClosure, 1);

                closure->meta_out_values_len = fn_scope->captured_symbols_len;

                for (int i = 0; i < fn_scope->captured_symbols_len; i++){
                    closure->meta_out_values[i].at = fn_scope->captured_symbols[i]->local;
                }

                closure->fn = fn;

                set_closure(symbol_index, closure, compiler);
            }else{
                set_fn(symbol_index, fn, compiler);
            }

            if(!is_normal){to_local(fn_symbol, compiler);}

            write_chunk(SGET_OPCODE, compiler);
            write_location(identifier_token, compiler);
            write_i32(symbol_index, compiler);

            if(is_normal){
                write_chunk(GDEF_OPCODE, compiler);
                write_location(identifier_token, compiler);
                write_str_alloc(identifier_token->lexeme_len, identifier_token->lexeme, compiler);
            }

            break;
        }case IMPORT_STMTTYPE:{
            ImportStmt *import_stmt = (ImportStmt *)stmt->sub_stmt;
            Token *import_token = import_stmt->import_token;
            DynArr *names = import_stmt->names;
            Token *alt_name_token = import_stmt->alt_name;

            LZOHTable *modules = compiler->modules;
            Module *current_module = compiler->current_module;
            Module *previous_module = compiler->previous_module;
            DynArr *current_symbols = MODULE_SYMBOLS(current_module);

            Token *name_token = NULL;
            size_t module_pathname_len;
            char *module_pathname = names_to_pathname(names, compiler, &module_pathname_len, &name_token);

            alt_name_token = alt_name_token ? alt_name_token : name_token;

            char *real_name = name_token->lexeme;
            char *alt_name = alt_name_token->lexeme;

            NativeModule *native_module = resolve_native_module(module_pathname, compiler);

            if(native_module){
                size_t symbol_idx = add_native_module_symbol(native_module, current_symbols);
                Symbol *symbol = declare(NATIVE_MODULE_SYMTYPE, alt_name_token, compiler);

                symbol->index = symbol_idx;

                from_symbols_to_global(symbol_idx, import_token, alt_name, compiler);

                break;
            }

            if(compiler->is_importing){
                char *module_pathname = current_module->pathname;
                size_t module_pathname_len = strlen(module_pathname);

                char local_module_pathname[module_pathname_len + 1];
                memcpy(local_module_pathname, module_pathname, module_pathname_len);
                local_module_pathname[module_pathname_len] = 0;

                char *parent_pathname = utils_files_parent_pathname(local_module_pathname, NULL);
                size_t parent_pathname_len = strlen(parent_pathname);

                if(!lzohtable_lookup(parent_pathname_len, parent_pathname, compiler->import_paths, NULL)){
                    char *cloned_parent_pathname = factory_clone_raw_str(parent_pathname, CTALLOCATOR, NULL);
                    RawStr rstr = {
                        .len = parent_pathname_len,
                        .buff = cloned_parent_pathname
                    };

                    dynarr_insert(&rstr, compiler->search_paths);
                    lzohtable_put(parent_pathname_len, cloned_parent_pathname, NULL, compiler->import_paths, NULL);
                }
            }

            size_t resolved_module_pathname_len;
            char *resolved_module_pathname = resolve_module_location(
                import_token,
                module_pathname,
                previous_module,
                current_module,
                compiler,
                NULL,
                &resolved_module_pathname_len
            );
            Module *module = NULL;

            if(lzohtable_lookup(
                resolved_module_pathname_len,
                resolved_module_pathname,
                modules, (void **)(&module))
            ){
                size_t symbol_idx = add_module_symbol(module, current_symbols);
                Symbol *symbol = declare(MODULE_SYMTYPE, alt_name_token, compiler);

                symbol->index = symbol_idx;

                from_symbols_to_global(symbol_idx, import_token, alt_name, compiler);

                break;
            }

            module = factory_create_module(real_name, resolved_module_pathname, compiler->rtallocator);

            DynArr *search_paths = compiler->search_paths;
            LZOHTable *import_paths = compiler->import_paths;
            LZOHTable *keywords = compiler->keywords;
            LZOHTable *native_fns = compiler->natives_fns;
            RawStr *source = utils_read_source(resolved_module_pathname, CTALLOCATOR);
            DynArr *tokens = FACTORY_DYNARR_PTR(CTALLOCATOR);
            DynArr *fns_prototypes = FACTORY_DYNARR_PTR(CTALLOCATOR);
            DynArr *stmts = FACTORY_DYNARR_PTR(CTALLOCATOR);

            Lexer *lexer = lexer_create(CTALLOCATOR, RTALLOCATOR);
            Parser *parser = parser_create(CTALLOCATOR);
            Compiler *import_compiler = compiler_create(compiler->ctallocator, compiler->rtallocator);

            if(lexer_scan(source, tokens, keywords, resolved_module_pathname, lexer)){
                error(compiler, import_token, "Failed to import module '%s'", resolved_module_pathname);
            }

            if(parser_parse(tokens, fns_prototypes, stmts, parser)){
                error(compiler, import_token, "Failed to import module '%s'", resolved_module_pathname);
            }

            if(compiler_import(
                search_paths,
                import_paths,
                keywords,
                native_fns,
                fns_prototypes,
                stmts,
                current_module,
                module,
                modules,
                import_compiler
            )){
                error(compiler, import_token, "Failed to import module '%s'", resolved_module_pathname);
            }

            lzohtable_put_ck(
                resolved_module_pathname_len,
                resolved_module_pathname,
                module,
                modules,
                NULL
            );

            size_t symbol_idx = add_module_symbol(module, current_symbols);
            Symbol *symbol = declare(MODULE_SYMTYPE, alt_name_token, compiler);

            symbol->index = symbol_idx;

            from_symbols_to_global(symbol_idx, import_token, alt_name, compiler);

            break;
        }case THROW_STMTTYPE:{
            ThrowStmt *throw_stmt = (ThrowStmt *)stmt->sub_stmt;
            Token *throw_token = throw_stmt->throw_token;
			Expr *value = throw_stmt->value;

            Scope *scope = current_scope(compiler);

            if(!inside_fn(compiler)){
                error(compiler, throw_token, "Can not use 'throw' statement in global scope.");
            }
            if(scope->type == CATCH_SCOPE){
                error(compiler, throw_token, "Can not use 'throw' statement inside catch blocks.");
            }

			if(value){
                compile_expr(value, compiler);
            }

            write_chunk(THROW_OPCODE, compiler);
            write_location(throw_token, compiler);
            write_chunk(value ? 1 : 0, compiler);

            break;
        }case TRY_STMTTYPE:{
            uint32_t trycatch_id = generate_trycatch_id(compiler);

            TryStmt *try_stmt = (TryStmt *)stmt->sub_stmt;
            DynArr *try_stmts = try_stmt->try_stmts;
			Token *err_identifier = try_stmt->err_identifier;
			DynArr *catch_stmts = try_stmt->catch_stmts;

            Token *try_token = try_stmt->try_token;

            write_chunk(TRYO_OPCODE, compiler);
            write_location(try_token, compiler);
            size_t catch_ip_idx = write_i16(0, compiler);

            if(try_stmts){
                scope_in_soft(TRY_SCOPE, compiler);
                size_t stmts_len = DYNARR_LEN(try_stmts);

                for (size_t i = 0; i < stmts_len; i++){
                    compile_stmt((Stmt *)dynarr_get_ptr(i, try_stmts), compiler);
                }

                scope_out(compiler);
            }

            write_chunk(TRYC_OPCODE, compiler);
            write_location(try_token, compiler);

			if(catch_stmts){
                size_t stmts_len = DYNARR_LEN(catch_stmts);

                scope_in_soft(CATCH_SCOPE, compiler);
                jmp(compiler, try_token, "CATCH_END_%" PRIu32, trycatch_id);

                size_t current_ip = chunks_len(compiler);
                update_i16(catch_ip_idx, (int16_t)current_ip, compiler);

                if(err_identifier){
                    Symbol *symbol = declare(IMUT_SYMTYPE, err_identifier, compiler);

                    write_chunk(LSET_OPCODE, compiler);
                    write_chunk(symbol->local, compiler);
                    write_location(err_identifier, compiler);
                }else{
                    write_chunk(POP_OPCODE, compiler);
                    write_location(try_token, compiler);
                }

				for (size_t i = 0; i < stmts_len; i++){
                    Stmt *stmt = (Stmt *)dynarr_get_ptr(i, catch_stmts);
                    compile_stmt(stmt, compiler);
                }

                label(compiler, "CATCH_END_%" PRIu32, trycatch_id);
                scope_out(compiler);
			}

            break;
        }case EXPORT_STMTTYPE:{
            ExportStmt *export_stmt = (ExportStmt *)stmt->sub_stmt;
            DynArr *symbols = export_stmt->symbols;

            for (size_t i = 0; i < DYNARR_LEN(symbols); i++){
                Token *symbol_identifier = dynarr_get_ptr(i, symbols);

                write_chunk(GASET_OPCODE, compiler);
                write_location(symbol_identifier, compiler);
                write_str_alloc(symbol_identifier->lexeme_len, symbol_identifier->lexeme, compiler);
                write_chunk(1, compiler);
            }

            break;
        }default:{
            assert("Illegal stmt type");
        }
    }
}

void define_natives(Compiler *compiler){
    assert(compiler->depth > 0);

    LZOHTable *native_fns = compiler->natives_fns;

    for (size_t i = 0; i < native_fns->m; i++){
        LZOHTableSlot *slot = &native_fns->slots[i];

        if(!slot->used){
            continue;
        }

        NativeFn *native_fn = (NativeFn *)slot->value;

        declare_native_fn(native_fn->name, compiler);
    }
}

void define_fns_prototypes(DynArr *fns_prototypes, Compiler *compiler){
    size_t len = DYNARR_LEN(fns_prototypes);

    for (size_t i = 0; i < len; i++){
        Token *token = (Token *)dynarr_get_ptr(i, fns_prototypes);
        declare(FN_SYMTYPE, token, compiler);
    }
}

void add_captured_symbol(Token *identifier, Symbol *symbol, Scope *scope, Compiler *compiler){
    if(scope->captured_symbols_len >= SYMBOLS_LENGTH){
        error(compiler, identifier, "Cannot capture more than %d symbols", SYMBOLS_LENGTH);
    }

    Scope *sfn_scope = inside_fn_from_symbol(symbol, compiler);
    int fns_between = count_fn_scopes(sfn_scope->depth - 1, scope->depth - 1, compiler);

    if(fns_between > 0){
        error(compiler, identifier, "Cannot capture symbols from a distance greater than 1");
    }

    scope->captured_symbols[scope->captured_symbols_len++] = symbol;
}

void set_fn(size_t index, Fn *fn, Compiler *compiler){
    Module *module = compiler->current_module;
	DynArr *symbols = MODULE_SYMBOLS(module);
	SubModuleSymbol symbol = (SubModuleSymbol){
        .type = FUNCTION_SUBMODULE_SYM_TYPE,
        .value = fn
    };

    dynarr_set_at(index, &symbol, symbols);
}

void set_closure(size_t index, MetaClosure *closure, Compiler *compiler){
    Module *module = compiler->current_module;
	DynArr *symbols = MODULE_SYMBOLS(module);
	SubModuleSymbol symbol = (SubModuleSymbol){
        .type = CLOSURE_SUBMODULE_SYM_TYPE,
        .value = closure
    };

    dynarr_set_at(index, &symbol, symbols);
}
//--------------------------------------------------------------------------//
//                          PUBLIC IMPLEMENTATION                           //
//--------------------------------------------------------------------------//
Compiler *compiler_create(Allocator *ctallocator, Allocator *rtallocator){
    Allocator *labels_allocator = memory_create_arena_allocator(ctallocator, NULL);
    Compiler *compiler = MEMORY_ALLOC(ctallocator, Compiler, 1);

    if(!labels_allocator || !compiler){
        memory_destroy_arena_allocator(labels_allocator);
        MEMORY_DEALLOC(ctallocator, Compiler, 1, compiler);

        return NULL;
    }

    memset(compiler, 0, sizeof(Compiler));
    compiler->ctallocator = ctallocator;
    compiler->rtallocator = rtallocator;
    compiler->labels_allocator = labels_allocator;

    return compiler;
}

void compiler_destroy(Compiler *compiler){
    if(!compiler){
        return;
    }

    memory_destroy_arena_allocator(compiler->labels_allocator);
    MEMORY_DEALLOC(compiler->ctallocator, Compiler, 1, compiler);
}

int compiler_compile(
    DynArr *search_paths,
    LZOHTable *import_paths,
    LZOHTable *keywords,
    LZOHTable *native_fns,
    DynArr *fns_prototypes,
    DynArr *stmts,
    Module *current_module,
    LZOHTable *modules,
    Compiler *compiler
){
    if(setjmp(compiler->err_jmp) == 1){
        return 1;
    }else{
        compiler->is_importing = 0;
        compiler->symbols = 1;
        compiler->search_paths = search_paths;
        compiler->import_paths = import_paths;
        compiler->keywords = keywords;
        compiler->natives_fns = native_fns;
        compiler->current_module = current_module;
        compiler->modules = modules;
        compiler->fns_prototypes = fns_prototypes;
        compiler->stmts = stmts;

        Fn *fn = NULL;
        size_t symbol_index = 0;

        scope_in_fn("main", compiler, &fn, &symbol_index);
        define_natives(compiler);
        define_fns_prototypes(fns_prototypes, compiler);

        for (size_t i = 0; i < stmts->used; i++){
            Stmt *stmt = (Stmt *)dynarr_get_ptr(i, stmts);
            compile_stmt(stmt, compiler);
        }

        write_chunk(EMPTY_OPCODE, compiler);
        write_chunk(RET_OPCODE, compiler);

        set_fn(symbol_index, fn, compiler);
        scope_out_fn(compiler);

        return compiler->is_err;
    }
}

int compiler_import(
    DynArr *search_paths,
    LZOHTable *import_paths,
    LZOHTable *keywords,
    LZOHTable *native_fns,
    DynArr *fns_prototypes,
    DynArr *stmts,
    Module *previous_module,
    Module *current_module,
    LZOHTable *modules,
    Compiler *compiler
){
    if(setjmp(compiler->err_jmp) == 1){
        return 1;
    }else{
        compiler->is_importing = 1;
        compiler->search_paths = search_paths;
        compiler->import_paths = import_paths;
        compiler->keywords = keywords;
        compiler->natives_fns = native_fns;
        compiler->fns_prototypes = fns_prototypes;
        compiler->stmts = stmts;
        compiler->previous_module = previous_module;
        compiler->current_module = current_module;
        compiler->modules = modules;

        Fn *fn = NULL;
        size_t symbol_index = 0;

        scope_in_fn("import", compiler, &fn, &symbol_index);
        define_natives(compiler);
        define_fns_prototypes(fns_prototypes, compiler);

        for (size_t i = 0; i < stmts->used; i++){
            Stmt *stmt = (Stmt *)dynarr_get_ptr(i, stmts);
            compile_stmt(stmt, compiler);
        }

        write_chunk(EMPTY_OPCODE, compiler);
        write_chunk(RET_OPCODE, compiler);

        set_fn(symbol_index, fn, compiler);
		scope_out(compiler);

        return compiler->is_err;
    }
}
//<PUBLIC IMPLEMENTATION
