#include "lzarena.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int is_power_of_two(size_t x) {
	return (x & (x-1)) == 0;
}

static size_t align_size(size_t size, size_t alignment){
    assert(is_power_of_two(alignment) && "alignment must be power of two");
    size_t module = size % alignment;
    size_t padding = module == 0 ? 0 : alignment - module;
    return size + padding;
}

static int append_region(size_t size, LZArena *arena){
    size_t real_size = size >= LZREGION_DEFAULT_LENGTH ?
        size * 0.50 + size :
        LZREGION_DEFAULT_LENGTH;
    LZRegion *region = lzregion_create(real_size);

    if(!region) return 1;

    if(arena->region_stack)
        region->next = arena->region_stack;

    arena->region_stack = region;
    arena->len += real_size;

    return 0;
}

static void *alloc_from_region(size_t size, size_t alignment, LZRegion *region, LZArena *arena){
    arena->used += size;
    return lzregion_align_alloc(size, alignment, region);
}

static void *alloc_from_regions(size_t size, size_t alignment, LZArena *arena){
    LZRegion *region = arena->region_stack;
    
    while (region){
        LZRegion *prev = region->next;
        void *ptr = lzregion_align_alloc(size, alignment, region);
        
        if(ptr){
            arena->used += size;
            return ptr;
        }

        region = prev;
    }

    return NULL;
}

LZRegion *lzregion_create_raw(size_t len, char *buff){
    if(!buff) return NULL;

	LZRegion *region = (LZRegion *)buff;
    region->offset = 0;
    region->len = len - LZREGION_SIZE;
	region->buff = LZREGION_SIZE + buff;
    region->next = NULL;

	return region;
}

LZRegion *lzregion_create(size_t len){
    size_t real_len = LZREGION_SIZE + len;
	char *buff = malloc(real_len);
    return lzregion_create_raw(real_len, buff);
}

void lzregion_destroy(LZRegion *region){
	if(!region) return;
	free(region);
}

void *lzregion_align_alloc(size_t size, size_t alignment, LZRegion *region){
    size_t asize = align_size(size, alignment);
    
    if(asize + region->offset > region->len) return NULL;
    
    void *ptr = region->buff + region->offset;
    region->offset += asize;
    
    return ptr;
}

void *lzregion_align_realloc(
    void *ptr,
    size_t new_size,
    size_t old_size,
    size_t alignment,
    LZRegion *region
){
    void *new_ptr = lzregion_align_alloc(new_size, alignment, region);
    
    if(!ptr) return new_ptr;
    assert((char *)ptr >= (char *)region && (char *)ptr <= (char *)region && "ptr out of bounds");
    if(!new_ptr) return NULL;
    
    memcpy(new_ptr, ptr, old_size);
    
    return new_ptr;
}

LZArena *lzarena_create(){
    LZArena *arena = malloc(LZARENA_SIZE);
    if(!arena) return NULL;
    memset(arena, 0, LZARENA_SIZE);
    return arena;
}

void lzarena_destroy(LZArena *arena){
    if(!arena) return;
    
    LZRegion *region = arena->region_stack;
    
    while (region){
        LZRegion *next = region->next;
        lzregion_destroy(region);
        region = next;
    }

    free(arena);
}

#define APPEND_AND_ALLOC(s, al, ar) \
    append_region(s, ar) == 1 ? NULL : alloc_from_region(s, al, ar->region_stack, ar)

void *lzarena_align_alloc(size_t size, size_t alignment, LZArena *arena){
    size_t aligned_size = align_size(size, alignment);

    if(aligned_size > LZARENA_FREE_BYTES(arena))
        return APPEND_AND_ALLOC(aligned_size, alignment, arena);
    
    void *ptr = alloc_from_regions(aligned_size, alignment, arena);
    
    return ptr ? ptr : APPEND_AND_ALLOC(aligned_size, alignment, arena);
}

void *lzarena_align_realloc(void *ptr, size_t new_size, size_t old_size, size_t alignment, LZArena *arena){
	void *new_ptr = lzarena_align_alloc(new_size, alignment, arena);
	if(!new_ptr) return NULL;
	memcpy(new_ptr, ptr, old_size);
	return new_ptr;
}
