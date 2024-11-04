#include "memory.h"
#include "lzarena.h"
#include <stdio.h>
#include <stdlib.h>

static LZRegion *region = NULL;
static DynArrAllocator dynarr_allocator = {0};
static LZHTableAllocator lzhtable_allocator = {0};

void *region_alloc(size_t size, void *ctx){
	LZRegion *region = (LZRegion *)ctx;
	void *ptr = LZREGION_ALLOC(size, region);

	if(ptr == NULL){
        printf("out of memory\n");
		memory_deinit();
		exit(EXIT_FAILURE);
	}

	return ptr;
}

void *region_realloc(void *ptr, size_t new_size, size_t old_size, void *ctx){
    void *new_ptr = region_alloc(new_size, ctx);
    memcpy(new_ptr, ptr, old_size);
	return new_ptr;
}

void region_dealloc(void *ptr, size_t size, void *ctx){
	// do nothing
}

int memory_init(){
	region = lzregion_create(2097152);

	dynarr_allocator.alloc = region_alloc;
	dynarr_allocator.realloc = region_realloc;
	dynarr_allocator.dealloc = region_dealloc;
	dynarr_allocator.ctx = region;

    lzhtable_allocator.alloc = region_alloc;
	lzhtable_allocator.realloc = region_realloc;
	lzhtable_allocator.dealloc = region_dealloc;
	lzhtable_allocator.ctx = region;

    return 0;
}

void memory_deinit(){
	lzregion_destroy(region);
}

void *memory_alloc(size_t size){
    void *ptr = LZREGION_ALLOC(size, region);

	if(ptr == NULL){
        printf("out of memory\n");
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