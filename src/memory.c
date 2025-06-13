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
    void *ptr = lzflist_calloc(size, ctx);

    if(!ptr){
        fprintf(stderr, "Out of memory\n");
        memory_deinit();
        exit(EXIT_FAILURE);
    }

    return ptr;
}

void *lzflist_link_realloc(void *ptr, size_t old_size, size_t new_size, void *ctx){
    void *new_ptr = lzflist_realloc(ptr, new_size, ctx);

    if(!new_ptr){
        fprintf(stderr, "Out of memory\n");
        memory_deinit();
        exit(EXIT_FAILURE);
    }

    return new_ptr;
}

void lzflist_link_dealloc(void *ptr, size_t size, void *ctx){
	lzflist_dealloc(ptr, ctx);
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

Allocator *memory_create_arena_allocator(Allocator *allocator, LZArena **out_arena){
    LZArena *arena = lzarena_create((LZArenaAllocator *)allocator);
    Allocator *arena_allocator = MEMORY_ALLOC(Allocator, 1, allocator);

    MEMORY_INIT_ALLOCATOR(
        arena,
        lzarena_link_alloc,
        lzarena_link_realloc,
        lzarena_link_dealloc,
        arena_allocator
    )

    arena_allocator->extra = allocator;

    if(out_arena){
        *out_arena = arena;
    }

    return arena_allocator;
}

void memory_destroy_arena_allocator(Allocator *arena_allocator){
    if(!arena_allocator){
        return;
    }

    Allocator *origin_allocator = (Allocator *)arena_allocator->extra;
    LZArena *arena = (LZArena *)arena_allocator->ctx;

    lzarena_destroy(arena);
    MEMORY_DEALLOC(Allocator, 1, arena_allocator, origin_allocator);
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