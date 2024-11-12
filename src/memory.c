#include "memory.h"
#include "lzarena.h"
#include "lzdynalloc.h"
#include <stdio.h>
#include <stdlib.h>

static LZRegion *region = NULL;
static LZDynAllocNode *dynalloc = NULL;
static DynArrAllocator dynarr_allocator = {0};
static LZHTableAllocator lzhtable_allocator = {0};

void *region_alloc(size_t size, void *ctx){
	LZRegion *region = (LZRegion *)ctx;
	void *ptr = LZREGION_ALLOC(size, region);

	if(ptr == NULL){
        fprintf(stderr, "out of memory: %ld/%ld\n", region->used, region->buff_size);
		memory_deinit();
		exit(EXIT_FAILURE);
	}

	return ptr;
}

void *region_realloc(void *ptr, size_t new_size, size_t old_size, void *ctx){
    void *new_ptr = region_alloc(new_size, ctx);

	if(!new_ptr){
		fprintf(stderr, "out of memory: %ld/%ld\n", region->used, region->buff_size);
		memory_deinit();
		exit(EXIT_FAILURE);
	}

    memcpy(new_ptr, ptr, old_size);

	return new_ptr;
}

void region_dealloc(void *ptr, size_t size, void *ctx){
	// do nothing
}

void *dyn_alloc(size_t size, void *ctx){
	LZDynAllocNode *node = (LZDynAllocNode *)ctx;
	void *ptr = lzdynalloc_node_alloc(size, node);

	if(!ptr){
		fprintf(stderr, "out of memory: %ld/%ld\n", region->used, region->buff_size);
        fprintf(stderr, "request: %ld\n", size);
		memory_deinit();
		exit(EXIT_FAILURE);
	}

	return ptr;
}

void *dyn_realloc(void *ptr, size_t new_size, size_t old_size, void *ctx){
	LZDynAllocNode *node = (LZDynAllocNode *)ctx;
	void *new_ptr = lzdynalloc_node_realloc(new_size, ptr, node);

	if(!new_ptr){
		fprintf(stderr, "out of memory: %ld/%ld\n", region->used, region->buff_size);
        fprintf(stderr, "request: %ld\n", new_size);
		memory_deinit();
		exit(EXIT_FAILURE);
	}

	return new_ptr;

}

void dyn_dealloc(void *ptr, size_t size, void *ctx){
	if(!ptr) return;
	LZDynAllocNode *node = (LZDynAllocNode *)ctx;
	lzdynalloc_node_dealloc(ptr, node);
}

int memory_init(){
	region = lzregion_create(33554432);
	dynalloc = lzdynalloc_node_raw(16777216, LZREGION_ALLOC(16777216, region));

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
	lzregion_destroy(region);
}

void memory_report(){
	printf("Region: %ld(%.2f)/%ld\n", region->used, region->used * 1.0 / region->buff_size * 100.0, region->buff_size);
}

void *memory_alloc(size_t size){
    void *ptr = LZREGION_ALLOC(size, region);

	if(ptr == NULL){
        fprintf(stderr, "out of memory: %ld/%ld\n", region->used, region->buff_size);
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
