#include "memory.h"
#include "lzflist.h"
#include "lzarena.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

void *lzarena_link_alloc(size_t size, void *ctx){
    return LZARENA_ALLOC(size, ctx);
}

void *lzarena_link_realloc(void *ptr, size_t old_size, size_t new_size, void *ctx){
    return LZARENA_REALLOC(ptr, old_size, new_size, ctx);
}

void lzarena_link_dealloc(void *ptr, size_t size, void *ctx){
	// nothing to be done
}

Allocator *memory_arena_allocator(Allocator *allocator, LZArena **out_arena){
    LZArena *arena = lzarena_create((LZArenaAllocator *)allocator);
    Allocator *arena_allocator = MEMORY_ALLOC(allocator, Allocator, 1);

    if(!arena || !arena_allocator){
        lzarena_destroy(arena);
        MEMORY_DEALLOC(allocator, Allocator, 1, arena_allocator);

        return NULL;
    }

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

void memory_destroy_arena_allocator(Allocator *allocator){
    if(!allocator){
        return;
    }

    Allocator *source_allocator = (Allocator *)allocator->extra;
    LZArena *arena = (LZArena *)allocator->ctx;

    lzarena_destroy(arena);
    MEMORY_DEALLOC(source_allocator, Allocator, 1, allocator);
}
