#include "memory.h"
#include "lzarena.h"
#include "lzdynalloc.h"
#include <stdio.h>
#include <stdlib.h>

static LZArena *arena = NULL;
static LZDynAllocNode *dynalloc = NULL;
static DynArrAllocator dynarr_allocator = {0};
static LZHTableAllocator lzhtable_allocator = {0};

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
	arena = lzarena_create();
	dynalloc = lzdynalloc_node_raw(32768, LZARENA_ALLOC(32768, arena));

	dynarr_allocator.alloc = dyn_alloc;
	dynarr_allocator.realloc = dyn_realloc;
	dynarr_allocator.dealloc = dyn_dealloc;
	dynarr_allocator.ctx = dynalloc;

    lzhtable_allocator.alloc = dyn_alloc;
	lzhtable_allocator.realloc = dyn_realloc;
	lzhtable_allocator.dealloc = dyn_dealloc;
	lzhtable_allocator.ctx = dynalloc;

    return 0;
}

void memory_deinit(){
	lzarena_destroy(arena);
}

void memory_report(){
	printf("Arena: %ld/%ld\n", arena->used, arena->len);
}

void *memory_alloc(size_t size){
    void *ptr = LZARENA_ALLOC(size, arena);

	if(!ptr){
        fprintf(stderr, "out of memory\n");
		fprintf(stderr, "request: %ld\n", size);
		memory_deinit();
		exit(EXIT_FAILURE);
	}

	return ptr;
}

DynArr *memory_dynarr(size_t size){
    return dynarr_create(size, &dynarr_allocator);
}

DynArrPtr *memory_dynarr_ptr(){
	return dynarr_ptr_create(&dynarr_allocator);
}

LZHTable *memory_lzhtable(){
    return lzhtable_create(17, &lzhtable_allocator);
}

char *memory_clone_str(char *str){
    size_t len = strlen(str);
    char *cstr = memory_alloc(len + 1);
    
    memcpy(cstr, str, len);
    cstr[len] = '\0';

    return cstr;
}
