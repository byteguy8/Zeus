#include "memory.h"
#include "lzarena.h"
#include "lzdynalloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static LZArena *compile_arena = NULL;
static LZDynAllocNode *compile_dynalloc = NULL;
static DynArrAllocator compile_dynarr_allocator = {0};
static LZHTableAllocator lzhtable_allocator = {0};

static LZArena *runtime_arena = NULL;
static LZDynAllocNode *runtime_dynalloc = NULL;
static DynArrAllocator runtime_dynarr_allocator = {0};
static LZHTableAllocator runtime_lzhtable_allocator = {0};

void *arena_alloc(size_t size, void *ctx){
	void *ptr = LZARENA_ALLOC(size, ctx);

	if(!ptr){
        fprintf(stderr, "out of memory\n");
		memory_deinit();
		exit(EXIT_FAILURE);
	}

	return ptr;
}

void *arena_realloc(void *ptr, size_t new_size, size_t old_size, void *ctx){
    void *new_ptr = LZARENA_REALLOC(ptr, new_size, old_size, ctx);

	if(!new_ptr){
		fprintf(stderr, "out of memory\n");
		memory_deinit();
		exit(EXIT_FAILURE);
	}

	return new_ptr;
}

void arena_dealloc(void *ptr, size_t size, void *ctx){
	// do nothing
}

void *dyn_alloc(size_t size, void *ctx){
	void *ptr = lzdynalloc_node_alloc(size, ctx);

	if(!ptr){
		fprintf(stderr, "out of memory\n");
		memory_deinit();
		exit(EXIT_FAILURE);
	}

	return ptr;
}

void *dyn_realloc(void *ptr, size_t new_size, size_t old_size, void *ctx){
	void *new_ptr = lzdynalloc_node_realloc(new_size, ptr, ctx);

	if(!new_ptr){
		fprintf(stderr, "out of memory\n");
		memory_deinit();
		exit(EXIT_FAILURE);
	}

	return new_ptr;

}

void dyn_dealloc(void *ptr, size_t size, void *ctx){
	if(!ptr) return;
	lzdynalloc_node_dealloc(ptr, ctx);
}

int memory_init(){
	compile_arena = lzarena_create();
	compile_dynalloc = lzdynalloc_node_raw(32768, LZARENA_ALLOC(32768, compile_arena));

	compile_dynarr_allocator.alloc = dyn_alloc;
	compile_dynarr_allocator.realloc = dyn_realloc;
	compile_dynarr_allocator.dealloc = dyn_dealloc;
	compile_dynarr_allocator.ctx = compile_dynalloc;

    lzhtable_allocator.alloc = dyn_alloc;
	lzhtable_allocator.realloc = dyn_realloc;
	lzhtable_allocator.dealloc = dyn_dealloc;
	lzhtable_allocator.ctx = compile_dynalloc;

    runtime_arena = lzarena_create();
    runtime_dynalloc = lzdynalloc_node_raw(32768, LZARENA_ALLOC(32768, runtime_arena));

	runtime_dynarr_allocator.alloc = dyn_alloc;
	runtime_dynarr_allocator.realloc = dyn_realloc;
	runtime_dynarr_allocator.dealloc = dyn_dealloc;
	runtime_dynarr_allocator.ctx = runtime_dynalloc;

    runtime_lzhtable_allocator.alloc = dyn_alloc;
	runtime_lzhtable_allocator.realloc = dyn_realloc;
	runtime_lzhtable_allocator.dealloc = dyn_dealloc;
	runtime_lzhtable_allocator.ctx = runtime_dynalloc;

    return 0;
}

void memory_deinit(){
	lzarena_destroy(compile_arena);
    lzarena_destroy(runtime_arena);
}

void memory_report(){
	printf("Compile arena: %ld/%ld\n", compile_arena->used, compile_arena->len);
    printf("Runtime arena: %ld/%ld\n", runtime_arena->used, runtime_arena->len);
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

DynArr *compile_dynarr(size_t size){
    return dynarr_create(size, &compile_dynarr_allocator);
}

DynArrPtr *compile_dynarr_ptr(){
	return dynarr_ptr_create(&compile_dynarr_allocator);
}

LZHTable *compile_lzhtable(){
    return lzhtable_create(17, &lzhtable_allocator);
}

char *compile_clone_str(char *str){
    size_t len = strlen(str);
    char *cstr = A_COMPILE_ALLOC(len + 1);
    
    memcpy(cstr, str, len);
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
    return lzhtable_create(17, &runtime_lzhtable_allocator);
}

char *runtime_clone_str(char *str){
    size_t len = strlen(str);
    char *cstr = A_RUNTIME_ALLOC(len + 1);
    
    memcpy(cstr, str, len);
    cstr[len] = '\0';

    return cstr;
}

Fn *runtime_fn(char *name, Module *module){
    char *fn_name = runtime_clone_str(name);
    DynArrPtr *params = runtime_dynarr_ptr();
    DynArr *chunks = runtime_dynarr(sizeof(uint8_t));
	DynArr *locations = runtime_dynarr(sizeof(OPCodeLocation));
    DynArr *constants = runtime_dynarr(sizeof(int64_t));
    Fn *fn = (Fn *)A_RUNTIME_ALLOC(sizeof(Fn));
    
    fn->name = fn_name;
    fn->params = params;
    fn->chunks = chunks;
	fn->locations = locations;
    fn->constants = constants;
    fn->module = module;

    return fn;
}

Module *runtime_module(char *name, char *filepath){
    char *module_name = runtime_clone_str(name);
    char *module_filepath = runtime_clone_str(filepath);
    LZHTable *strings = runtime_lzhtable();
	LZHTable *symbols = runtime_lzhtable();
    LZHTable *tries = runtime_lzhtable();
    LZHTable *globals = runtime_lzhtable();
    SubModule *submodule = A_RUNTIME_ALLOC(sizeof(SubModule));
    Module *module = (Module *)A_RUNTIME_ALLOC(sizeof(Module));
    
    submodule->resolve = 0;
    submodule->strings = strings;
    submodule->symbols = symbols;
    submodule->tries = tries;
    submodule->globals = globals;

    module->shadow = 0;
    module->name = module_name;
    module->filepath = module_filepath;
    module->submodule = submodule;

    return module;
}

Module *runtime_clone_module(char *new_name, char *filepath, Module *module){
    char *module_name = runtime_clone_str(new_name);
    char *module_filepath = runtime_clone_str(filepath);
    Module *new_module = (Module *)A_RUNTIME_ALLOC(sizeof(Module));

    new_module->shadow = 1;
    new_module->name = module_name;
    new_module->filepath = module_filepath;
    new_module->submodule = module->submodule;

    return new_module;
}