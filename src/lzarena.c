#include "lzarena.h"
#include <stdlib.h>

static size_t align_size(size_t size, size_t alignment){
    size_t module = size % alignment;
    size_t value = module == 0 ? 0 : alignment - module;
    return size + value;
}

void lzregion_create_raw(size_t buff_size, char *buff, LZRegion *region){
    region->buff = buff;
    region->used = 0;
    region->buff_size = buff_size;
}

LZRegion *lzregion_create(size_t buff_size){
	char *buff = (char *)malloc(sizeof(LZRegion) + buff_size);
	LZRegion *region = (LZRegion *)buff;

	region->buff = buff + sizeof(LZRegion);
	region->used = 0;
	region->buff_size = buff_size;

	return region;
}

void lzregion_destroy(LZRegion *region){
	if(region == NULL) return;
	free(region);
}

void *lzregion_alloc_align(size_t size, size_t alignment, LZRegion *region){
    size_t asize = align_size(size, alignment);
    size_t used = region->used + asize;

    if(used > region->buff_size) return NULL;

    void *ptr = region->buff + region->used;
    region->used += asize;

    return ptr;
}