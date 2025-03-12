#include "memory.h"
#include "lzflist.h"
#include "lzarena.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static LZArena *compile_arena = NULL;
static BStrAllocator compile_bstr_allocator = {0};
static DynArrAllocator compile_dynarr_allocator = {0};
static LZHTableAllocator lzhtable_allocator = {0};

static LZArena *runtime_arena = NULL;
static DynArrAllocator runtime_dynarr_allocator = {0};
static LZHTableAllocator runtime_lzhtable_allocator = {0};

static LZFList *runtime_allocator = NULL;
static Allocator allocator = {0};

void *lzarena_link_alloc(size_t size, void *ctx){
	void *ptr = LZARENA_ALLOC(size, ctx);

	if(!ptr){
        fprintf(stderr, "out of memory\n");
		memory_deinit();
		exit(EXIT_FAILURE);
	}

	return ptr;
}

void *lzarena_link_realloc(void *ptr, size_t old_size, size_t new_size, void *ctx){
    void *new_ptr = LZARENA_REALLOC(ptr, old_size, new_size, ctx);

	if(!new_ptr){
		fprintf(stderr, "out of memory\n");
		memory_deinit();
		exit(EXIT_FAILURE);
	}

	return new_ptr;
}

void lzarena_link_dealloc(void *ptr, size_t size, void *ctx){
	// do nothing
}

void *lzflist_link_alloc(size_t size, void *ctx){
    LZFList *allocator = (LZFList *)ctx;
    void *ptr = lzflist_alloc(size, allocator);

    if(!ptr){
        fprintf(stderr, "out of memory\n");
		memory_deinit();
		exit(EXIT_FAILURE);
	}

	return ptr;
}

void *lzflist_link_realloc(void *ptr, size_t old_size, size_t new_size, void *ctx){
    LZFList *allocator = (LZFList *)ctx;
    void *new_ptr = lzflist_realloc(ptr, new_size, allocator);

    if(!new_ptr){
        fprintf(stderr, "out of memory\n");
		memory_deinit();
		exit(EXIT_FAILURE);
	}

    return new_ptr;
}

void lzflist_link_dealloc(void *ptr, size_t size, void *ctx){
	LZFList *allocator = (LZFList *)ctx;
    lzflist_dealloc(ptr, allocator);
}

int memory_init(){
    // COMPILE TIME
	compile_arena = lzarena_create();
    // RUNTIME
    runtime_arena = lzarena_create();
    runtime_allocator = lzflist_create();

    compile_bstr_allocator.ctx = compile_arena;
    compile_bstr_allocator.alloc = lzarena_link_alloc;
    compile_bstr_allocator.realloc = lzarena_link_realloc;
    compile_bstr_allocator.dealloc = lzarena_link_dealloc;

    compile_dynarr_allocator.ctx = compile_arena;
	compile_dynarr_allocator.alloc = lzarena_link_alloc;
	compile_dynarr_allocator.realloc = lzarena_link_realloc;
	compile_dynarr_allocator.dealloc = lzarena_link_dealloc;

    lzhtable_allocator.ctx = compile_arena;
    lzhtable_allocator.alloc = lzarena_link_alloc;
	lzhtable_allocator.realloc = lzarena_link_realloc;
	lzhtable_allocator.dealloc = lzarena_link_dealloc;

    runtime_dynarr_allocator.ctx = runtime_arena;
	runtime_dynarr_allocator.alloc = lzarena_link_alloc;
	runtime_dynarr_allocator.realloc = lzarena_link_realloc;
	runtime_dynarr_allocator.dealloc = lzarena_link_dealloc;

    runtime_lzhtable_allocator.ctx = runtime_arena;
    runtime_lzhtable_allocator.alloc = lzarena_link_alloc;
	runtime_lzhtable_allocator.realloc = lzarena_link_realloc;
	runtime_lzhtable_allocator.dealloc = lzarena_link_dealloc;

    allocator.ctx = runtime_allocator;
    allocator.alloc = lzflist_link_alloc;
    allocator.realloc = lzflist_link_realloc;
    allocator.dealloc = lzflist_link_dealloc;

    return 0;
}

void memory_deinit(){
	lzarena_destroy(compile_arena);
    lzarena_destroy(runtime_arena);
    lzflist_destroy(runtime_allocator);
}

void memory_report(){
    size_t compile_used = 0;
    size_t compile_size = 0;

    lzarena_report(&compile_used, &compile_size, compile_arena);
	printf("Compile arena: %ld/%ld\n", compile_used, compile_size);

    size_t runtime_used = 0;
    size_t runtime_size = 0;
    printf("Runtime arena: %ld/%ld\n", runtime_used, runtime_size);
}

void memory_free_compile(){
    lzarena_destroy(compile_arena);
    compile_arena = NULL;
}

void *memory_arena_alloc(size_t size, int type){
    assert(type >= 0 && type <= 1);

    LZArena *arena = type == COMPILE_ARENA ? compile_arena : runtime_arena;
    void *ptr = LZARENA_ALLOC(size, arena);

	if(!ptr){
        fprintf(stderr, "out of memory\n");
		memory_deinit();
		exit(EXIT_FAILURE);
	}

	return ptr;
}

BStr *compile_bstr(){
    return bstr_create_empty(&compile_bstr_allocator);
}

DynArr *compile_dynarr(size_t size){
    return dynarr_create(size, &compile_dynarr_allocator);
}

DynArrPtr *compile_dynarr_ptr(){
	return dynarr_ptr_create(&compile_dynarr_allocator);
}

LZHTable *compile_lzhtable(){
    return lzhtable_create(16, &lzhtable_allocator);
}

char *compile_clone_str(char *str){
    size_t len = strlen(str);
    char *cstr = A_COMPILE_ALLOC(len + 1);
    
    memcpy(cstr, str, len);
    cstr[len] = '\0';

    return cstr;
}

char *compile_clone_str_range(size_t start, size_t len, char *str){
    char *cstr = A_COMPILE_ALLOC(len + 1);
    memcpy(cstr, str + start, len);
    cstr[len] = '\0';
    return cstr;
}

DynArr *runtime_dynarr(size_t size){
    return dynarr_create(size, &runtime_dynarr_allocator);
}

DynArrPtr *runtime_dynarr_ptr(){
    return dynarr_ptr_create(&runtime_dynarr_allocator);
}

LZHTable *runtime_lzhtable(){
    return lzhtable_create(16, &runtime_lzhtable_allocator);
}

char *runtime_clone_str(char *str){
    size_t len = strlen(str);
    char *cstr = A_RUNTIME_ALLOC(len + 1);
    
    memcpy(cstr, str, len);
    cstr[len] = '\0';

    return cstr;
}

char *runtime_clone_str_range(size_t start, size_t len, char *str){
    char *cstr = A_RUNTIME_ALLOC(len + 1);
    memcpy(cstr, str + start, len);
    cstr[len] = '\0';
    return cstr;
}

NativeModule *runtime_native_module(char *name){
	char *module_name = runtime_clone_str(name);
	LZHTable *symbols = runtime_lzhtable();
	NativeModule *module = A_RUNTIME_ALLOC(sizeof(NativeModule));
	
	module->name = module_name;
	module->symbols = symbols;
	
	return module;
}

void runtime_add_native_fn_info(char *name, uint8_t arity, RawNativeFn raw_native, LZHTable *natives){
    size_t name_len = strlen(name);
    NativeFnInfo *info = (NativeFnInfo *)A_RUNTIME_ALLOC(sizeof(NativeFnInfo));

    info->arity = arity;
    info->raw_native = raw_native;

    lzhtable_put((uint8_t *)name, name_len, info, natives, NULL);
}

void runtime_add_native_fn(char *name, uint8_t arity, RawNativeFn raw_native, NativeModule *module){
	size_t name_len = strlen(name);
	NativeFn *native = (NativeFn *)A_RUNTIME_ALLOC(sizeof(NativeFn));
	NativeModuleSymbol *symbol = (NativeModuleSymbol *)A_RUNTIME_ALLOC(sizeof(NativeModuleSymbol));

    native->unique = 1;
    native->arity = arity;
    memcpy(native->name, name, name_len);
    native->name[name_len] = '\0';
    native->name_len = name_len;
    native->target = NULL;
    native->raw_fn = raw_native;

	symbol->type = NATIVE_FUNCTION_NMSYMTYPE;
	symbol->value.fn = native;

    lzhtable_put((uint8_t *)name, name_len, symbol, module->symbols, NULL);
}

Fn *runtime_fn(char *name, Module *module){
    char *fn_name = runtime_clone_str(name);
    DynArrPtr *params = runtime_dynarr_ptr();
    DynArr *chunks = runtime_dynarr(sizeof(uint8_t));
	DynArr *locations = runtime_dynarr(sizeof(OPCodeLocation));
    DynArr *constants = runtime_dynarr(sizeof(int64_t));
    DynArr *float_values = runtime_dynarr(sizeof(double));
    Fn *fn = (Fn *)A_RUNTIME_ALLOC(sizeof(Fn));
    
    fn->name = fn_name;
    fn->params = params;
    fn->chunks = chunks;
	fn->locations = locations;
    fn->integers = constants;
    fn->floats = float_values;
    fn->module = module;

    return fn;
}

Module *runtime_module(char *name, char *filepath){
    char *module_name = runtime_clone_str(name);
    char *module_pathname = runtime_clone_str(filepath);
    LZHTable *strings = runtime_lzhtable();
	DynArr *symbols = runtime_dynarr(sizeof(SubModuleSymbol));
    LZHTable *tries = runtime_lzhtable();
    LZHTable *globals = runtime_lzhtable();
    SubModule *submodule = A_RUNTIME_ALLOC(sizeof(SubModule));
    Module *module = (Module *)A_RUNTIME_ALLOC(sizeof(Module));
    
    submodule->resolved = 0;
    submodule->strings = strings;
    submodule->symbols = symbols;
    submodule->tries = tries;
    submodule->globals = globals;

    module->shadow = 0;
    module->name = module_name;
    module->pathname = module_pathname;
    module->submodule = submodule;

    return module;
}

Module *runtime_clone_module(char *new_name, char *filepath, Module *module){
    char *module_name = runtime_clone_str(new_name);
    char *module_pathname = runtime_clone_str(filepath);
    Module *new_module = (Module *)A_RUNTIME_ALLOC(sizeof(Module));

    new_module->shadow = 1;
    new_module->name = module_name;
    new_module->pathname = module_pathname;
    new_module->submodule = module->submodule;

    return new_module;
}

Allocator *memory_allocator(){
    return &allocator;
}

void *memory_alloc(size_t size){
    return lzflist_link_alloc(size, runtime_allocator);
}

void *memory_realloc(void *ptr, size_t size){
    return lzflist_link_realloc(ptr, 0, size, runtime_allocator);
}

void memory_dealloc(void *ptr){
    return lzflist_link_dealloc(ptr, 0, runtime_allocator);
}

DynArr *memory_dynarr(size_t size){
    return dynarr_create(size, (DynArrAllocator *)&allocator);
}