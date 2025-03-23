#include "memory.h"
#include "lzflist.h"
#include "lzarena.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static LZFList *mallocator = NULL;
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
    mallocator = lzflist_create();

    allocator.ctx = mallocator;
    allocator.alloc = lzflist_link_alloc;
    allocator.realloc = lzflist_link_realloc;
    allocator.dealloc = lzflist_link_dealloc;

    return 0;
}

void memory_deinit(){
    lzflist_destroy(mallocator);
}

Allocator *memory_allocator(){
    return &allocator;
}

Allocator *memory_arena_allocator(LZArena **out_arena){
    Allocator *arena_allocator = (Allocator *)MEMORY_ALLOC(Allocator, 1, &allocator);
    LZArena *arena = lzarena_create((LZArenaAllocator *)&allocator);

    arena_allocator->ctx = arena;
    arena_allocator->alloc = lzarena_link_alloc;
    arena_allocator->realloc = lzarena_link_realloc;
    arena_allocator->dealloc = lzarena_link_dealloc;

    if(out_arena){*out_arena = arena;}

    return arena_allocator;
}

void *memory_alloc(size_t size){
    return lzflist_link_alloc(size, mallocator);
}

void *memory_realloc(void *ptr, size_t size){
    return lzflist_link_realloc(ptr, 0, size, mallocator);
}

void memory_dealloc(void *ptr){
    return lzflist_link_dealloc(ptr, 0, mallocator);
}