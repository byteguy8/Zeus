#include "compiler.h"
#include "memory.h"
#include "factory.h"
#include "utils.h"
#include "token.h"
#include "expr.h"
#include "stmt.h"
#include "opcode.h"
#include "lexer.h"
#include "parser.h"
#include "native_math.h"
#include "native_random.h"
#include "native_time.h"
#include "native_io.h"
#include "native_os.h"
#include <stdint.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

#define COMPILE_ALLOCATOR (compiler->ctallocator)
#define LABELS_ALLOCATOR (compiler->labels_allocator)

//>PRIVATE INTERFACE
static void error(Compiler *compiler, Token *token, char *msg, ...);
void descompose_i16(int16_t value, uint8_t *bytes);
void descompose_i32(int32_t value, uint8_t *bytes);
#define WHILE_IN(compiler) (++compiler->while_counter)
#define WHILE_OUT(compiler) (--compiler->while_counter)
static void mark_stop(size_t len, size_t index, Compiler *compiler);
static void unmark_stop(size_t which, Compiler *compiler);
static void mark_continue(size_t len, size_t index, Compiler *compiler);
static void unmark_continue(size_t which, Compiler *compiler);
Scope *scope_in(ScopeType type, Compiler *compiler);
Scope *scope_in_soft(ScopeType type, Compiler *compiler);
Scope *scope_in_fn(char *name, Compiler *compiler, Fn **out_function, size_t *out_fn_index);
void scope_out(Compiler *compiler);
void scope_out_fn(Compiler *compiler);
Scope *inside_while(Compiler *compiler);
Scope *inside_function(Compiler *compiler);
Scope *symbol_fn_scope(Symbol *symbol, Compiler *compiler);
int count_fn_scopes(int from, int to, Compiler *compiler);
Scope *current_scope(Compiler *compiler);
Scope *previous_scope(Scope *scope, Compiler *compiler);
void insert_label(Scope *scope, Label *label);
Label *label(Compiler *compiler, char *fmt, ...);
void insert_jmp(Scope *scope, JMP *jmp);
void remove_jmp(JMP *jmp);
JMP *jif(Compiler *compiler, Token *ref_token, char *fmt, ...);
JMP *jmp(Compiler *compiler, Token *ref_token, char *fmt, ...);
void resolve_jmps(Scope *current, Compiler *compiler);
int resolve_symbol(Token *identifier, Symbol *symbol, Compiler *compiler);
Symbol *exists_scope(char *name, Scope *scope, Compiler *compiler);
Symbol *exists_local(char *name, Compiler *compiler);
Symbol *get(Token *identifier_token, Compiler *compiler);
void to_local(Symbol *symbol, Compiler *compiler);
void from_symbols_to_global(size_t symbol_index, Token *location_token, char *name, Compiler *compiler);
Symbol *declare_native(char *identifier, Compiler *compiler);
Symbol *declare(SymbolType type, Token *identifier_token, Compiler *compiler);
DynArr *current_chunks(Compiler *compiler);
DynArr *current_locations(Compiler *compiler);
DynArr *current_constants(Compiler *compiler);
DynArr *current_float_values(Compiler *compiler);
size_t chunks_len(Compiler *compiler);
size_t write_chunk(uint8_t chunk, Compiler *compiler);
void write_location(Token *token, Compiler *compiler);
void update_chunk(size_t index, uint8_t chunk, Compiler *compiler);
size_t write_i16(int16_t i16, Compiler *compiler);
size_t write_i32(int32_t i32, Compiler *compiler);
void update_i16(size_t index, int16_t i16, Compiler *compiler);
void update_i32(size_t index, int32_t i32, Compiler *compiler);
size_t write_i64_const(int64_t i64, Compiler *compiler);
size_t write_double_const(double value, Compiler *compiler);
void write_str(char *rstr, Compiler *compiler);
char *names_to_name(DynArr *names, Token **out_name, Compiler *compiler);
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
//<PRIVATE INTERFACE
void add_captured_symbol(Token *identifier, Symbol *symbol, Scope *scope, Compiler *compiler){
    if(scope->captured_symbols_len >= SYMBOLS_LENGTH){
        error(compiler, identifier, "Cannot capture more than %d symbols", SYMBOLS_LENGTH);
    }

    Scope *sfn_scope = symbol_fn_scope(symbol, compiler);
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
        .type = FUNCTION_MSYMTYPE,
        .value = fn
    };

    DYNARR_SET(&symbol, index, symbols);
}

void set_closure(size_t index, MetaClosure *closure, Compiler *compiler){
    Module *module = compiler->current_module;
	DynArr *symbols = MODULE_SYMBOLS(module);
	SubModuleSymbol symbol = (SubModuleSymbol){
        .type = CLOSURE_MSYMTYPE,
        .value = closure
    };

    DYNARR_SET(&symbol, index, symbols);
}

//>PRIVATE IMPLEMENTATION
static void error(Compiler *compiler, Token *token, char *msg, ...){
    va_list args;
    va_start(args, msg);

    fprintf(stderr, "Compiler error at line %d in file '%s':\n\t", token->line, token->pathname);
    vfprintf(stderr, msg, args);
    fprintf(stderr, "\n");

    va_end(args);
    longjmp(compiler->err_jmp, 1);
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

static void mark_stop(size_t len, size_t index, Compiler *compiler){
	assert(compiler->stop_ptr < LOOP_MARK_LENGTH);
    LoopMark *loop_mark = &compiler->stops[compiler->stop_ptr++];

    loop_mark->id = compiler->while_counter;
	loop_mark->len = len;
    loop_mark->index = index;
}

static void unmark_stop(size_t which, Compiler *compiler){
	assert(which < compiler->stop_ptr);

	for(size_t i = which; i + 1 < compiler->stop_ptr; i++){
        LoopMark *a = &compiler->stops[i];
        LoopMark *b = &compiler->stops[i + 1];

		memcpy(a, b, sizeof(LoopMark));
	}

	compiler->stop_ptr--;
}

static void mark_continue(size_t len, size_t index, Compiler *compiler){
	assert(compiler->stop_ptr < LOOP_MARK_LENGTH);
    LoopMark *loop_mark = &compiler->continues[compiler->continue_ptr++];

    loop_mark->id = compiler->while_counter;
	loop_mark->len = len;
    loop_mark->index = index;
}

static void unmark_continue(size_t which, Compiler *compiler){
	assert(which < compiler->continue_ptr);

	for(size_t i = which; i + 1 < compiler->continue_ptr; i++){
        LoopMark *a = &compiler->stops[i];
        LoopMark *b = &compiler->stops[i + 1];

		memcpy(a, b, sizeof(LoopMark));
	}

	compiler->continue_ptr--;
}

Scope *scope_in(ScopeType type, Compiler *compiler){
    Scope *scope = &compiler->scopes[compiler->depth++];

    scope->depth = compiler->depth;
    scope->locals = 0;
    scope->scope_locals = 0;
    scope->try = NULL;
	scope->type = type;
    scope->symbols_len = 0;
    scope->captured_symbols_len = 0;

    return scope;
}

Scope *scope_in_soft(ScopeType type, Compiler *compiler){
    Scope *enclosing = &compiler->scopes[compiler->depth - 1];
    Scope *scope = &compiler->scopes[compiler->depth++];

    scope->depth = compiler->depth;
    scope->locals = enclosing->locals;
    scope->scope_locals = 0;
	scope->type = type;
    scope->symbols_len = 0;

    return scope;
}

Scope *scope_in_fn(char *name, Compiler *compiler, Fn **out_function, size_t *out_symbol_index){
    assert(compiler->fn_ptr < FUNCTIONS_LENGTH);

    Fn *fn = factory_create_fn(name, compiler->current_module, compiler->rtallocator);
    compiler->fn_stack[compiler->fn_ptr++] = fn;

    DynArr *symbols = MODULE_SYMBOLS(compiler->current_module);

    SubModuleSymbol module_symbol = {0};
    dynarr_insert(&module_symbol, symbols);

    if(out_function) *out_function = fn;
    if(out_symbol_index) *out_symbol_index = DYNARR_LEN(symbols) - 1;

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

Scope *inside_while(Compiler *compiler){
	assert(compiler->depth > 0);

	for(int i = (int)compiler->depth - 1; i >= 0; i--){
		Scope *scope = compiler->scopes + i;
		if(scope->type == WHILE_SCOPE) return scope;
		if(scope->type == FUNCTION_SCOPE) return NULL;
	}

	return NULL;
}

Scope *inside_function(Compiler *compiler){
	assert(compiler->depth > 0);

	for(int i = (int)compiler->depth - 1; i >= 0; i--){
		Scope *scope = compiler->scopes + i;
		if(scope->type == FUNCTION_SCOPE){
            if(i == 0) return NULL;
            return scope;
        }
	}

	return NULL;
}

Scope *symbol_fn_scope(Symbol *symbol, Compiler *compiler){
    assert(compiler->depth > 0);

	for(int i = (int)symbol->depth - 1; i >= 0; i--){
		Scope *scope = compiler->scopes + i;
		if(scope->type == FUNCTION_SCOPE){
            if(i == 0) return NULL;
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
    Allocator *allocator = scope->depth == 1 ? COMPILE_ALLOCATOR : LABELS_ALLOCATOR;

    char *name = MEMORY_ALLOC(char, LABEL_NAME_LENGTH, allocator);
    int out_count = vsnprintf(name, LABEL_NAME_LENGTH, fmt, args);

    va_end(args);
    assert(out_count < LABEL_NAME_LENGTH);

    Label *label = MEMORY_ALLOC(Label, 1, allocator);

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
    Allocator *allocator = scope->depth == 1 ? COMPILE_ALLOCATOR : LABELS_ALLOCATOR;

    char *label = MEMORY_ALLOC(char, LABEL_NAME_LENGTH, allocator);
    int out_count = vsnprintf(label, LABEL_NAME_LENGTH, fmt, args);

    va_end(args);
    assert(out_count < LABEL_NAME_LENGTH);

    JMP *jmp = (JMP *)MEMORY_ALLOC(JMP, 1, allocator);

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
    Allocator *allocator = scope->depth == 1 ? COMPILE_ALLOCATOR : LABELS_ALLOCATOR;

    char *label = MEMORY_ALLOC(char, LABEL_NAME_LENGTH, allocator);
    int out_count = vsnprintf(label, LABEL_NAME_LENGTH, fmt, args);

    va_end(args);
    assert(out_count < LABEL_NAME_LENGTH);

    JMP *jmp = (JMP *)MEMORY_ALLOC(JMP, 1, allocator);

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
            if(selected->location < jmp->bef){
                size_t len = jmp->bef - selected->location + 1;
                update_i16(jmp->idx, -len, compiler);
            }else{
                size_t len = selected->location - jmp->af + 1;
                update_i16(jmp->idx, len, compiler);
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
        Scope *fn_scope = inside_function(compiler);

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
    write_str(name, compiler);
}

Symbol *declare_native(char *identifier, Compiler *compiler){
    size_t identifier_len = strlen(identifier);
    Scope *scope = current_scope(compiler);
    Symbol *symbol = &scope->symbols[scope->symbols_len++];

    symbol->depth = scope->depth;
    symbol->local = -1;
    symbol->type = NATIVE_FN_SYMTYPE;
    symbol->name_len = identifier_len;
    memcpy(symbol->name, identifier, identifier_len);
    symbol->name[identifier_len] = '\0';
    symbol->identifier_token = NULL;

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
    symbol->name_len = identifier_len;
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
    return fn->integers;
}

DynArr *current_float_values(Compiler *compiler){
    assert(compiler->fn_ptr > 0 && compiler->fn_ptr < FUNCTIONS_LENGTH);
    Fn *fn = compiler->fn_stack[compiler->fn_ptr - 1];
    return fn->floats;
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
    location.filepath = factory_clone_raw_str(token->pathname, compiler->rtallocator);

	dynarr_insert(&location, locations);
}

void update_chunk(size_t index, uint8_t chunk, Compiler *compiler){
	DynArr *chunks = current_chunks(compiler);
    DYNARR_SET(&chunk, index, chunks);
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

void update_i16(size_t index, int16_t i16, Compiler *compiler){
    uint8_t bytes[2];
	DynArr *chunks = current_chunks(compiler);

	descompose_i16(i16, bytes);

	for(size_t i = 0; i < 2; i++)
        DYNARR_SET(bytes + i, index + i, chunks);
}

void update_i32(size_t index, int32_t i32, Compiler *compiler){
	uint8_t bytes[4];
	DynArr *chunks = current_chunks(compiler);

	descompose_i32(i32, bytes);

	for(size_t i = 0; i < 4; i++)
        DYNARR_SET(bytes + i, index + i, chunks);
}

size_t write_i64_const(int64_t i64, Compiler *compiler){
    DynArr *constants = current_constants(compiler);
    int32_t used = (int32_t) constants->used;

    assert(used < 32767 && "Too many constants");

    dynarr_insert(&i64, constants);

	return write_i16(used, compiler);
}

size_t write_double_const(double value, Compiler *compiler){
	DynArr *float_values = current_float_values(compiler);
	int32_t length = (int32_t) float_values->used;

	assert(length < 32767 && "Too many constants");

	dynarr_insert(&value, float_values);

	return write_i16(length, compiler);
}

void write_str(char *rstr, Compiler *compiler){
    uint8_t *key = (uint8_t *)rstr;
    size_t key_size = strlen(rstr);
    uint32_t hash = lzhtable_hash(key, key_size);

    Module *module = compiler->current_module;
    SubModule *submodule = module->submodule;
    LZHTable *strings = submodule->strings;

    if(!lzhtable_hash_contains(hash, strings, NULL)){
        char *str = factory_clone_raw_str(rstr, compiler->rtallocator);
        lzhtable_hash_put(hash, str, strings);
    }

    write_i32((int32_t)hash, compiler);
}

char *names_to_name(DynArr *names, Token **out_name, Compiler *compiler){
    if(DYNARR_LEN(names) == 1){
        Token *name = (Token *)DYNARR_GET(0, names);
        *out_name = name;
        return name->lexeme;
    }

    Token *name = NULL;
    BStr *bstr = FACTORY_BSTR(compiler->ctallocator);

    for (size_t i = 0; i < DYNARR_LEN(names); i++){
        name = (Token *)DYNARR_GET(i, names);
        bstr_append(name->lexeme, bstr);
        if(i + 1 < DYNARR_LEN(names)) bstr_append("/", bstr);
    }

    *out_name = name;

    return (char *)bstr_raw_substr(0, bstr->len - 1, bstr);
}

NativeModule *resolve_native_module(char *module_name, Compiler *compiler){
	if(strcmp("math", module_name) == 0){
		if(!math_module){
			math_module_init(compiler->rtallocator);
		}

		return math_module;
	}

    if(strcmp("random", module_name) == 0){
        if(!random_module){
            random_module_init(compiler->rtallocator);
        }

        return random_module;
    }

    if(strcmp("time", module_name) == 0){
        if(!time_module){
            time_module_init(compiler->rtallocator);
        }

        return time_module;
    }

    if(strcmp("io", module_name) == 0){
        if(!io_module){
            io_module_init(compiler->rtallocator);
        }

        return io_module;
    }

    if(strcmp("os", module_name) == 0){
        if(!os_module){
            os_module_init(compiler->rtallocator);
        }

        return os_module;
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

    for (int i = 0; i < compiler->paths_len; i++){
        char* base_pathname = compiler->paths[i];
        size_t base_pathname_len = strlen(base_pathname);

        size_t module_pathname_len = base_pathname_len + module_name_len + 4;
        char module_pathname[module_pathname_len + 1];

        memcpy(module_pathname, base_pathname, base_pathname_len);
        memcpy(module_pathname + base_pathname_len, "/", 1);
        memcpy(module_pathname + base_pathname_len + 1, module_name, module_name_len);
        memcpy(module_pathname + base_pathname_len + 1 + module_name_len, ".ze", 3);
        module_pathname[module_pathname_len] = '\0';

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

        if(!UTILS_FILES_EXISTS(module_pathname)){continue;}

		if(!UTILS_FILE_CAN_READ(module_pathname)){
			error(compiler, import_token, "File at '%s' do not exists or cannot be read", module_pathname);
		}

        if(!utils_files_is_regular(module_pathname)){
            error(compiler, import_token, "File at '%s' is not a regular file", module_pathname);
        }

        *out_module_name_len = module_name_len;
        *out_module_pathname_len = module_pathname_len;

        return factory_clone_raw_str(module_pathname, compiler->ctallocator);
    }

    error(compiler, import_token, "module %s not found", module_name);

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

            if((value >= -128 && value <= 127) ||
            (value >= 0 && value <= 255)){
                write_chunk(CINT_OPCODE, compiler);
                write_location(token, compiler);

                write_chunk((uint8_t)value, compiler);

                break;
            }

            write_chunk(INT_OPCODE, compiler);
			write_location(token, compiler);

            write_i64_const(value, compiler);

            break;
        }case FLOAT_EXPRTYPE:{
			FloatExpr *float_expr = (FloatExpr *)expr->sub_expr;
			Token *float_token = float_expr->token;
			double value = *(double *)float_token->literal;

			write_chunk(FLOAT_OPCODE, compiler);
			write_location(float_token, compiler);

			write_double_const(value, compiler);

			break;
		}case STRING_EXPRTYPE:{
			StringExpr *string_expr = (StringExpr *)expr->sub_expr;
			uint32_t hash = string_expr->hash;

			write_chunk(STRING_OPCODE, compiler);
			write_location(string_expr->string_token, compiler);

			write_i32((int32_t) hash, compiler);

			break;
		}case TEMPLATE_EXPRTYPE:{
            TemplateExpr *template_expr = (TemplateExpr *)expr->sub_expr;
            Token *template_token = template_expr->template_token;
            DynArr *exprs = template_expr->exprs;

            for (int16_t i = DYNARR_LEN(exprs) - 1; i >= 0; i--){
                Expr *expr = (Expr *)dynarr_get_ptr(i, exprs);
                compile_expr(expr, compiler);
            }

            write_chunk(TEMPLATE_OPCODE, compiler);
            write_location(template_token, compiler);

            write_i16((int16_t)DYNARR_LEN(exprs), compiler);

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

                    char *name = factory_clone_raw_str(param_identifier->lexeme, compiler->rtallocator);
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
                MetaClosure *closure = MEMORY_ALLOC(MetaClosure, 1, compiler->rtallocator);

                closure->values_len = fn_scope->captured_symbols_len;

                for (int i = 0; i < fn_scope->captured_symbols_len; i++){
                    closure->values[i].at = fn_scope->captured_symbols[i]->local;
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
                write_str(identifier->lexeme, compiler);

                break;
            }

            if(symbol->depth == 1){
                write_chunk(GGET_OPCODE, compiler);
                write_location(identifier, compiler);
                write_str(identifier->lexeme, compiler);
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
                for (size_t i = 0; i < DYNARR_LEN(args); i++){
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

            write_str(symbol_token->lexeme, compiler);

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

    	        if(symbol->type == IMUT_SYMTYPE)
        	        error(compiler, identifier, "'%s' declared as constant. Can't change its value.", identifier->lexeme);

                int is_out = resolve_symbol(identifier, symbol, compiler);

            	compile_expr(value_expr, compiler);

	            if(symbol->depth == 1){
                	write_chunk(GSET_OPCODE, compiler);
   					write_location(equals_token, compiler);

                	write_str(identifier->lexeme, compiler);
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

				write_chunk(PUT_OPCODE, compiler);
				write_location(equals_token, compiler);

				write_str(symbol_token->lexeme, compiler);

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
			Token *identifier_token = compound_expr->identifier_token;
			Token *operator = compound_expr->operator;
			Expr *right = compound_expr->right;

			Symbol *symbol = get(identifier_token, compiler);

			if(symbol->type == IMUT_SYMTYPE)
				error(compiler, identifier_token, "'%s' declared as constant. Can't change its value.", identifier_token->lexeme);

			if(symbol->depth == 1){
                write_chunk(GGET_OPCODE, compiler);
			    write_str(identifier_token->lexeme, compiler);
            }else{
                write_chunk(LGET_OPCODE, compiler);
			    write_chunk(symbol->local, compiler);
            }

			compile_expr(right, compiler);

			switch(operator->type){
				case COMPOUND_ADD_TOKTYPE:{
					write_chunk(ADD_OPCODE, compiler);
					break;
				}
				case COMPOUND_SUB_TOKTYPE:{
					write_chunk(SUB_OPCODE, compiler);
					break;
				}
				case COMPOUND_MUL_TOKTYPE:{
					write_chunk(MUL_OPCODE, compiler);
					break;
				}
				case COMPOUND_DIV_TOKTYPE:{
					write_chunk(DIV_OPCODE, compiler);
					break;
				}
				default:{
					assert("Illegal compound type");
				}
			}

			write_location(operator, compiler);

			if(symbol->depth == 1){
                write_chunk(GSET_OPCODE, compiler);
				write_location(identifier_token, compiler);

			    write_str(identifier_token->lexeme, compiler);
            }else{
                write_chunk(LSET_OPCODE, compiler);
				write_location(identifier_token, compiler);

			    write_chunk(symbol->local, compiler);
            }

			break;
		}case ARRAY_EXPRTYPE:{
            ArrayExpr *array_expr = (ArrayExpr *)expr->sub_expr;
            Token *array_token = array_expr->array_token;
            Expr *len_expr = array_expr->len_expr;
            DynArr *values = array_expr->values;

            if(len_expr){
                compile_expr(len_expr, compiler);
            }else{
                write_chunk(INT_OPCODE, compiler);
                write_location(array_token, compiler);

                if(values){
                    write_i64_const((int64_t)DYNARR_LEN(values), compiler);
                }else{
                    write_i64_const((int64_t)0, compiler);
                }
            }

            write_chunk(ARRAY_OPCODE, compiler);
            write_location(array_token, compiler);

            write_chunk(1, compiler);
            write_i32(0, compiler);

            if(values){
                for (int32_t i = (int32_t)DYNARR_LEN(values) - 1; i >= 0; i--){
                    compile_expr((Expr *)dynarr_get_ptr(i, values), compiler);

                    write_chunk(ARRAY_OPCODE, compiler);
                    write_location(array_token, compiler);

                    write_chunk(2, compiler);
                    write_i32(i, compiler);
                }
            }

            break;
        }case LIST_EXPRTYPE:{
			ListExpr *list_expr = (ListExpr *)expr->sub_expr;
			Token *list_token = list_expr->list_token;
			DynArr *exprs = list_expr->exprs;

            if(exprs){
				for(int i = DYNARR_LEN(exprs) - 1; i >= 0; i--){
					compile_expr((Expr *)dynarr_get_ptr(i, exprs), compiler);
				}
			}

            int16_t len = exprs ? (int16_t)DYNARR_LEN(exprs) : 0;

            write_chunk(LIST_OPCODE, compiler);
			write_location(list_token, compiler);

            write_i16(len, compiler);

			break;
		}case DICT_EXPRTYPE:{
            DictExpr *dict_expr = (DictExpr *)expr->sub_expr;
            DynArr *key_values = dict_expr->key_values;

            if(key_values){
                for (int i = DYNARR_LEN(key_values) - 1; i >= 0; i--){
                    DictKeyValue *key_value = (DictKeyValue *)dynarr_get_ptr(i, key_values);
                    Expr *key = key_value->key;
                    Expr *value = key_value->value;

                    compile_expr(key, compiler);
                    compile_expr(value, compiler);
                }
            }

            int16_t len = (int16_t)(key_values ? key_values->used : 0);

            write_chunk(DICT_OPCODE, compiler);
			write_location(dict_expr->dict_token, compiler);

            write_i16(len, compiler);

            break;
        }case RECORD_EXPRTYPE:{
			RecordExpr *record_expr = (RecordExpr *)expr->sub_expr;
			DynArr *key_values = record_expr->key_values;

			if(key_values){
				for(int i = (int)(DYNARR_LEN(key_values) - 1); i >= 0; i--){
					RecordExprValue *key_value = (RecordExprValue *)dynarr_get_ptr(i, key_values);
					compile_expr(key_value->value, compiler);
				}
			}

			write_chunk(RECORD_OPCODE, compiler);
			write_location(record_expr->record_token, compiler);

			write_chunk((uint8_t)(key_values ? DYNARR_LEN(key_values) : 0), compiler);

			if(key_values){
				for(size_t i = 0; i < DYNARR_LEN(key_values); i++){
					RecordExprValue *key_value = (RecordExprValue *)dynarr_get_ptr(i, key_values);
					write_str(key_value->key->lexeme, compiler);
				}
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
                }case FN_OTYPE:
                 case CLOSURE_OTYPE:
                 case NATIVE_FN_OTYPE:{
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
        .type = NATIVE_MODULE_MSYMTYPE,
        .value = module
    };

    dynarr_insert(&module_symbol, symbols);

    return DYNARR_LEN(symbols) - 1;
}

size_t add_module_symbol(Module *module, DynArr *symbols){
	SubModuleSymbol module_symbol = (SubModuleSymbol ){
        .type = MODULE_MSYMTYPE,
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

            Symbol *symbol = declare(is_const ? IMUT_SYMTYPE : MUT_SYMTYPE, identifier_token, compiler);

            if(initializer_expr == NULL){
                write_chunk(EMPTY_OPCODE, compiler);
            }else{
                compile_expr(initializer_expr, compiler);
            }

            if(symbol->depth == 1){
                write_chunk(GDEF_OPCODE, compiler);
				write_location(identifier_token, compiler);
                write_str(identifier_token->lexeme, compiler);
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
			IfStmt *if_stmt = (IfStmt *)stmt->sub_stmt;
            IfStmtBranch *if_branch = if_stmt->if_branch;
            DynArr *elif_branches = if_stmt->elif_branches;
            DynArr *else_stmts = if_stmt->else_stmts;

            Token *if_branch_token = if_branch->branch_token;
            Expr *if_condition = if_branch->condition_expr;
            DynArr *if_stmts = if_branch->stmts;

            compile_expr(if_condition, compiler);
            jif(compiler, if_branch_token, "IFB_END");

            scope_in_soft(IF_SCOPE, compiler);

            if(if_stmts){
                for (size_t i = 0; i < DYNARR_LEN(if_stmts); i++){
                    Stmt *stmt = (Stmt *)dynarr_get_ptr(i, if_stmts);
                    compile_stmt(stmt, compiler);
                }
            }

            scope_out(compiler);

            jmp(compiler, if_branch_token, "IF_END");
            label(compiler, "IFB_END");

            if(elif_branches){
                for (size_t branch_idx = 0; branch_idx < DYNARR_LEN(elif_branches); branch_idx++){
                    IfStmtBranch *elif_branch = (IfStmtBranch *)dynarr_get_ptr(branch_idx, elif_branches);

                    Token *elif_branch_token = elif_branch->branch_token;
                    Expr *elif_condition = elif_branch->condition_expr;
                    DynArr *elif_stmts = elif_branch->stmts;

                    compile_expr(elif_condition, compiler);
                    jif(compiler, elif_branch_token, "ELIF_END_%zu", branch_idx);

                    scope_in_soft(IF_SCOPE, compiler);

                    for (size_t i = 0; i < DYNARR_LEN(elif_stmts); i++){
                        Stmt *stmt = (Stmt *)dynarr_get_ptr(i, elif_stmts);
                        compile_stmt(stmt, compiler);
                    }

                    scope_out(compiler);

                    jmp(compiler, elif_branch_token, "IF_END");
                    label(compiler, "ELIF_END_%zu", branch_idx);
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

            label(compiler, "IF_END");

			break;
		}case WHILE_STMTTYPE:{
			WhileStmt *while_stmt = (WhileStmt *)stmt->sub_stmt;
            Token *while_token = while_stmt->while_token;
			Expr *condition = while_stmt->condition;
			DynArr *stmts = while_stmt->stmts;

			write_chunk(JMP_OPCODE, compiler);
            write_location(while_stmt->while_token, compiler);
			size_t jmp_index = write_i16(0, compiler);

			size_t len_bef_body = chunks_len(compiler);

			scope_in_soft(WHILE_SCOPE, compiler);
			char while_id = WHILE_IN(compiler);

			for(size_t i = 0; i < DYNARR_LEN(stmts); i++){
				compile_stmt((Stmt *)dynarr_get_ptr(i, stmts), compiler);
			}

			WHILE_OUT(compiler);
			scope_out(compiler);

            size_t len_af_body = chunks_len(compiler);
			size_t body_len = len_af_body - len_bef_body;

            update_i16(jmp_index, (int16_t)body_len + 1, compiler);

			compile_expr(condition, compiler);
            size_t len_af_while = chunks_len(compiler);
			size_t while_len = len_af_while - len_bef_body;

			write_chunk(JIT_OPCODE, compiler);
            write_location(while_token, compiler);
			write_i16(-((int16_t)while_len), compiler);

			size_t af_header = chunks_len(compiler);
			size_t ptr = 0;

			while (compiler->stop_ptr > 0 && ptr < compiler->stop_ptr){
				LoopMark *loop_mark = &compiler->stops[ptr];

				if(loop_mark->id != while_id){
					ptr++;
					continue;
				}

				update_i16(loop_mark->index, af_header - loop_mark->len + 1, compiler);
				unmark_stop(ptr, compiler);
			}

            ptr = 0;

            while (compiler->continue_ptr > 0 && ptr < compiler->continue_ptr){
				LoopMark *loop_mark = &compiler->continues[ptr];

				if(loop_mark->id != while_id){
					ptr++;
					continue;
				}

				update_i16(loop_mark->index, len_af_body - loop_mark->len + 1, compiler);
                unmark_continue(ptr, compiler);
			}

			break;
		}case STOP_STMTTYPE:{
			StopStmt *stop_stmt = (StopStmt *)stmt->sub_stmt;
			Token *stop_token = stop_stmt->stop_token;

			if(!inside_while(compiler))
				error(compiler, stop_token, "Can't use 'stop' statement outside loops.");

			write_chunk(JMP_OPCODE, compiler);
            write_location(stop_token, compiler);
			size_t index = write_i16(0, compiler);

            size_t len = chunks_len(compiler);
			mark_stop(len, index, compiler);

			break;
		}case CONTINUE_STMTTYPE:{
            ContinueStmt *continue_stmt = (ContinueStmt *)stmt->sub_stmt;
            Token *continue_token = continue_stmt->continue_token;

            if(!inside_while(compiler))
                error(compiler, continue_token, "Can't use 'continue' statement outside loops.");

            write_chunk(JMP_OPCODE, compiler);
            write_location(continue_token, compiler);
            size_t index = write_i16(0, compiler);

            size_t len = chunks_len(compiler);
            mark_continue(len, index, compiler);

            break;
        }case RETURN_STMTTYPE:{
            ReturnStmt *return_stmt = (ReturnStmt *)stmt->sub_stmt;
			Token *return_token = return_stmt->return_token;
            Expr *value = return_stmt->value;

			Scope *scope = inside_function(compiler);

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

            Scope *enclosing = inside_function(compiler);

            Fn *fn = NULL;
            size_t symbol_index = 0;

            Symbol *fn_symbol = declare(FN_SYMTYPE, identifier_token, compiler);
            Scope *fn_scope = scope_in_fn(identifier_token->lexeme, compiler, &fn, &symbol_index);

            fn_symbol->index = ((int)symbol_index);

            if(params){
                for (size_t i = 0; i < DYNARR_LEN(params); i++){
                    Token *param_token = (Token *)dynarr_get_ptr(i, params);
                    declare(MUT_SYMTYPE, param_token, compiler);

                    char *name = factory_clone_raw_str(param_token->lexeme, compiler->rtallocator);
                    dynarr_insert_ptr(name, fn->params);
                }
            }

            char returned = 0;

            for (size_t i = 0; i < DYNARR_LEN(stmts); i++){
                compile_stmt((Stmt *)dynarr_get_ptr(i, stmts), compiler);

                if(i + 1 >= DYNARR_LEN(stmts) && stmt->type == RETURN_STMTTYPE){
                    returned = 1;
                }
            }

            if(!returned){
                write_chunk(EMPTY_OPCODE, compiler);
                write_location(identifier_token, compiler);

                write_chunk(RET_OPCODE, compiler);
                write_location(identifier_token, compiler);
            }

            scope_out_fn(compiler);

            int is_normal = enclosing == NULL;

            if(fn_scope->captured_symbols_len > 0){
                MetaClosure *closure = MEMORY_ALLOC(MetaClosure, 1, compiler->rtallocator);

                closure->values_len = fn_scope->captured_symbols_len;

                for (int i = 0; i < fn_scope->captured_symbols_len; i++){
                    closure->values[i].at = fn_scope->captured_symbols[i]->local;
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
                write_str(identifier_token->lexeme, compiler);
            }

            break;
        }case IMPORT_STMTTYPE:{
            assert(compiler->paths_len > 0);

            ImportStmt *import_stmt = (ImportStmt *)stmt->sub_stmt;
            Token *import_token = import_stmt->import_token;
            DynArr *names = import_stmt->names;
            Token *alt_name = import_stmt->alt_name;

            LZHTable *modules = compiler->modules;
            Module *current_module = compiler->current_module;
            Module *previous_module = compiler->previous_module;
            DynArr *current_symbols = MODULE_SYMBOLS(current_module);

            Token *name = NULL;

            char *module_name = NULL;

            char *module_absolute_name = names_to_name(names, &name, compiler);
            size_t module_absolute_name_len;

            module_name = name->lexeme;

            char *module_alt_name = alt_name ? alt_name->lexeme : NULL;
            char *module_real_name = module_alt_name ? module_alt_name : module_name;

			NativeModule *native_module = resolve_native_module(module_name, compiler);

			if(native_module){
				size_t symbol_index = add_native_module_symbol(native_module, current_symbols);
				Symbol *symbol = declare(MODULE_SYMTYPE, alt_name ? alt_name : name, compiler);

                symbol->index = symbol_index;

                from_symbols_to_global(symbol_index, import_token, module_real_name, compiler);

				break;
			}

            size_t module_pathname_len;
            char *module_pathname = resolve_module_location(
                import_token,
                module_absolute_name,
                previous_module,
                current_module,
                compiler,
                &module_absolute_name_len,
                &module_pathname_len
            );

            LZHTableNode *module_node = NULL;

            if(lzhtable_contains((uint8_t *)module_pathname, module_pathname_len, modules, &module_node)){
                Module *imported_module = (Module *)module_node->value;
                Module *cloned_module = factory_clone_module(module_name, module_pathname, imported_module, compiler->rtallocator);

                size_t symbol_index = add_module_symbol(cloned_module, current_symbols);
                Symbol *symbol = declare(MODULE_SYMTYPE, alt_name ? alt_name : name, compiler);

                symbol->index = symbol_index;

                from_symbols_to_global(symbol_index, import_token, module_real_name, compiler);

                break;
            }

            RawStr *source = utils_read_source(module_pathname, compiler->ctallocator);

            LZHTable *keywords = compiler->keywords;
            LZHTable *natives = compiler->natives;

            DynArr *tokens = FACTORY_DYNARR_PTR(compiler->ctallocator);
            DynArr *stmts = FACTORY_DYNARR_PTR(compiler->ctallocator);

            Module *module = factory_module(module_name, module_pathname, compiler->rtallocator);
            SubModule *submodule = module->submodule;

            Lexer *lexer = lexer_create(compiler->ctallocator);
	        Parser *parser = parser_create(compiler->ctallocator);
            Compiler *import_compiler = compiler_create(compiler->ctallocator, compiler->rtallocator);

            char *cloned_module_pathname = factory_clone_raw_str(module_pathname, compiler->ctallocator);
            import_compiler->paths[import_compiler->paths_len++] = utils_files_parent_pathname(cloned_module_pathname);

            for (int i = 0; i < compiler->paths_len; i++){
				import_compiler->paths[import_compiler->paths_len++] = compiler->paths[i];
            }

            if(lexer_scan(
                source,
                tokens,
                submodule->strings,
                keywords,
                module_pathname,
                lexer
            )){
				compiler->is_err = 1;
				break;
			}

            if(parser_parse(tokens, stmts, parser)){
				compiler->is_err = 1;
				break;
			}

            if(compiler_import(
                keywords,
                natives,
                stmts,
                current_module,
                module,
                modules,
                import_compiler
            )){
                compiler->is_err = 1;
				break;
            }

            uint32_t module_pathname_hash = lzhtable_hash((uint8_t *)module_pathname, module_pathname_len);
            lzhtable_hash_put(module_pathname_hash, module, modules);

            size_t symbol_index = add_module_symbol(module, current_symbols);
            declare(MODULE_SYMTYPE, alt_name ? alt_name : name, compiler);

            from_symbols_to_global(symbol_index, import_token, module_real_name, compiler);

            break;
        }case THROW_STMTTYPE:{
            ThrowStmt *throw_stmt = (ThrowStmt *)stmt->sub_stmt;
            Token *throw_token = throw_stmt->throw_token;
			Expr *value = throw_stmt->value;

            Scope *scope = current_scope(compiler);

            if(!inside_function(compiler)){
                error(compiler, throw_token, "Can not use 'throw' statement in global scope.");
            }
            if(scope->type == CATCH_SCOPE){
                error(compiler, throw_token, "Can not use 'throw' statement inside catch blocks.");
            }

			if(value){
                compile_expr(value, compiler);
            }else{
                write_chunk(EMPTY_OPCODE, compiler);
                write_location(throw_token, compiler);
            }

            write_chunk(THROW_OPCODE, compiler);
            write_location(throw_token, compiler);

            break;
        }case TRY_STMTTYPE:{
            TryStmt *try_stmt = (TryStmt *)stmt->sub_stmt;
            DynArr *try_stmts = try_stmt->try_stmts;
			Token *err_identifier = try_stmt->err_identifier;
			DynArr *catch_stmts = try_stmt->catch_stmts;

            size_t try_jmp_index = 0;
            TryBlock *try_block = MEMORY_ALLOC(TryBlock, 1, compiler->rtallocator);

			memset(try_block, 0, sizeof(TryBlock));

            if(try_stmts){
                size_t start = chunks_len(compiler);
                try_block->try = start;

                Scope *scope = scope_in(TRY_SCOPE, compiler);
                scope->try = try_block;

                Scope *previous = previous_scope(scope, compiler);

                if(previous && previous->try)
                    try_block->outer = previous->try;

                for (size_t i = 0; i < DYNARR_LEN(try_stmts); i++){
                    compile_stmt((Stmt *)dynarr_get_ptr(i, try_stmts), compiler);
                }

                scope_out(compiler);

				write_chunk(JMP_OPCODE, compiler);
                write_location(try_stmt->try_token, compiler);
				try_jmp_index = write_i16(0, compiler);
            }

			if(catch_stmts){
				size_t start = chunks_len(compiler);
                try_block->catch = start;

                scope_in(CATCH_SCOPE, compiler);
				Symbol *symbol = declare(IMUT_SYMTYPE, err_identifier, compiler);

				try_block->local = symbol->local;

				for (size_t i = 0; i < DYNARR_LEN(catch_stmts); i++){
                    compile_stmt((Stmt *)dynarr_get_ptr(i, catch_stmts), compiler);
                }

                scope_out(compiler);

				size_t end = chunks_len(compiler);
				update_i16(try_jmp_index, end - start + 1, compiler);
			}

            Module *module = compiler->current_module;
            SubModule *submodule = module->submodule;
            LZHTable *fn_tries = submodule->tries;
            Fn *fn = compiler->fn_stack[compiler->fn_ptr - 1];

			uint8_t *key = (uint8_t *)fn;
			size_t key_size = sizeof(Fn);
			LZHTableNode *node = NULL;
			DynArr *tries = NULL;

			if(lzhtable_contains(key, key_size, fn_tries, &node)){
				tries = (DynArr *)node->value;
			}else{
				tries = (DynArr *)FACTORY_DYNARR_PTR(compiler->rtallocator);
				lzhtable_put(key, key_size, tries, fn_tries, NULL);
			}

            dynarr_insert_ptr(try_block, tries);

            break;
        }case LOAD_STMTTYPE:{
            LoadStmt *load_stmt = (LoadStmt *)stmt->sub_stmt;
            Token *load_token = load_stmt->load_token;
            Token *path_token = load_stmt->pathname;
            Token *name_token = load_stmt->name;

            declare(NATIVE_MODULE_SYMTYPE, name_token, compiler);

            write_chunk(LOAD_OPCODE, compiler);
            write_location(load_token, compiler);
            write_i32(*(int32_t*)path_token->literal, compiler);

            write_chunk(GSET_OPCODE, compiler);
            write_location(load_token, compiler);
            write_str(name_token->lexeme, compiler);

            write_chunk(POP_OPCODE, compiler);
            write_location(load_token, compiler);

            break;
        }case EXPORT_STMTTYPE:{
            ExportStmt *export_stmt = (ExportStmt *)stmt->sub_stmt;
            DynArr *symbols = export_stmt->symbols;

            for (size_t i = 0; i < DYNARR_LEN(symbols); i++){
                Token *symbol_identifier = dynarr_get_ptr(i, symbols);

                write_chunk(GASET_OPCODE, compiler);
                write_location(symbol_identifier, compiler);
                write_str(symbol_identifier->lexeme, compiler);
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
    LZHTableNode *current = compiler->natives->head;

    while (current){
        LZHTableNode *next = current->next_table_node;

        NativeFn *native = (NativeFn *)current->value;
        declare_native(native->name, compiler);

        current = next;
    }
}
//<PRIVATE IMPLEMENTATION
//>PUBLIC IMPLEMENTATION
Compiler *compiler_create(Allocator *ctallocator, Allocator *rtallocator){
    Compiler *compiler = MEMORY_ALLOC(Compiler, 1, ctallocator);

    if(!compiler){
        return NULL;
    }

    compiler->ctallocator = ctallocator;
    compiler->rtallocator = rtallocator;
    compiler->labels_allocator = memory_create_arena_allocator(ctallocator, NULL);

    return compiler;
}

int compiler_compile(
    LZHTable *keywords,
    LZHTable *natives,
    DynArr *stmts,
    Module *current_module,
    LZHTable *modules,
    Compiler *compiler
){
    if(setjmp(compiler->err_jmp) == 1){
        return 1;
    }else{
        compiler->symbols = 1;
        compiler->keywords = keywords;
        compiler->natives = natives;
        compiler->current_module = current_module;
        compiler->modules = modules;
        compiler->stmts = stmts;

        Fn *fn = NULL;
        size_t symbol_index = 0;

        scope_in_fn("main", compiler, &fn, &symbol_index);
        define_natives(compiler);

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
    LZHTable *keywords,
    LZHTable *natives,
    DynArr *stmts,
    Module *previous_module,
    Module *current_module,
    LZHTable *modules,
    Compiler *compiler
){
    if(setjmp(compiler->err_jmp) == 1){
        return 1;
    }else{
        compiler->keywords = keywords;
        compiler->natives = natives;
        compiler->stmts = stmts;
        compiler->previous_module = previous_module;
        compiler->current_module = current_module;
        compiler->modules = modules;

        Fn *fn = NULL;
        size_t symbol_index = 0;

        scope_in_fn("import", compiler, &fn, &symbol_index);
        define_natives(compiler);

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