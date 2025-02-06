// Humble implementation of a dynamic array

#ifndef DYNARR_H
#define DYNARR_H

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifndef DYNARR_ALIGNMENT
#define DYNARR_ALIGNMENT 8
#endif

#ifndef DYNARR_DEFAULT_GROW_SIZE
#define DYNARR_DEFAULT_GROW_SIZE 8
#endif

typedef struct dynarr_allocator{
    void *ctx;
    void *(*alloc)(size_t size, void *ctx);
    void *(*realloc)(void *ptr, size_t old_size, size_t new_size, void *ctx);
    void (*dealloc)(void *ptr, size_t size, void *ctx);
}DynArrAllocator;

typedef struct dynarr{
    size_t count;
    size_t used;
    size_t size;
    size_t padding;
    char *items;
    struct dynarr_allocator *allocator;
}DynArr;

typedef struct dynarr_ptr{
    size_t used;
    size_t count;
    void **items;
    struct dynarr_allocator *allocator;
}DynArrPtr;

// PUBLIC INTERFACE DYNARR
DynArr *dynarr_create(size_t item_size, DynArrAllocator *allocator);
void dynarr_destroy(DynArr *dynarr);

#define DYNARR_LEN(dynarr)((dynarr)->used)
#define DYNARR_AVAILABLE(dynarr)((dynarr)->count - (dynarr)->used)
void dynarr_reverse(DynArr *dyarr);
void dynarr_sort(int (*comparator)(void *a, void *b), DynArr *dynarr);
int dynarr_find(void *b, int (*comparator)(void *a, void *b), DynArr *dynarr);

#define DYNARR_GET(index, dynarr)((dynarr)->items + (((dynarr)->padding + (dynarr)->size) * (index)))
#define DYNARR_GET_AS(as, index, arr)(*(as *)(DYNARR_GET(index, arr)))
#define DYNARR_SET(item, index, dynarr)(memmove(DYNARR_GET(index, dynarr), (item), (dynarr)->size))
void *dynarr_get_ptr(size_t index, DynArr *dynarr);
void dynarr_set_ptr(void *ptr, size_t index, DynArr *dynarr);
int dynarr_insert(void *item, DynArr *dynarr);
int dynarr_insert_at(size_t index, void *item, DynArr *dynarr);
int dynarr_insert_ptr(void *ptr, DynArr *dynarr);
int dynarr_insert_ptr_at(size_t index, void *ptr, DynArr *dynarr);
int dynarr_append(DynArr *from, DynArr *to);
void dynarr_remove_index(size_t index, DynArr *dynarr);
int dynarr_remove_all(DynArr *dynarr);

// PUBLIC INTERFACE DYNARRPTR
DynArrPtr *dynarr_ptr_create(DynArrAllocator *allocator);
void dynarr_ptr_destroy(DynArrPtr *dynarr);

#define DYNARR_PTR_LEN(dynarr_ptr)((dynarr_ptr)->used)
#define DYNARR_PTR_POSITION(position, dynarr) ((dynarr)->items + (position))
#define DYNARR_PTR_GET(index, dynarr) (*DYNARR_PTR_POSITION(index, dynarr))

void dynarr_ptr_set(size_t index, void *value, DynArrPtr *dynarr);
int dynarr_ptr_insert(void *ptr, DynArrPtr *dynarr);
void dynarr_ptr_move(size_t index, size_t from, size_t to, DynArrPtr *dynarr);
void dynarr_ptr_remove_index(size_t index, DynArrPtr *dynarr);
void dynarr_ptr_remove_ptr(void *ptr, DynArrPtr *dynarr);

#endif