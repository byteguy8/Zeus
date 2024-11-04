// Humble implementation of a dynamic array

#ifndef _DYNARR_H_
#define _DYNARR_H_

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#ifndef DYNARR_ALIGNMENT
#define DYNARR_ALIGNMENT 8
#endif

#ifndef DYNARR_DEFAULT_GROW_SIZE
#define DYNARR_DEFAULT_GROW_SIZE 8
#endif

typedef struct dynarr_allocator
{
    void *(*alloc)(size_t size, void *ctx);
    void *(*realloc)(void *ptr, size_t new_size, size_t old_size, void *ctx);
    void (*dealloc)(void *ptr, size_t size, void *ctx);
    void *ctx;
} DynArrAllocator;

typedef struct dynarr
{
    size_t count;
    size_t used;
    size_t size;
    void *items;
    struct dynarr_allocator *allocator;
} DynArr;

typedef struct dynarr_ptr
{
    size_t used;
    size_t count;
    void **items;
    struct dynarr_allocator *allocator;
} DynArrPtr;

// interface

// DynArr
struct dynarr *dynarr_create(size_t item_size, struct dynarr_allocator *allocator);
void dynarr_destroy(struct dynarr *dynarr);

#define DYNARR_LEN(dynarr) (dynarr->used)
#define DYNARR_AVAILABLE(dynarr) (dynarr->count - dynarr->used)

void *dynarr_get(size_t index, struct dynarr *dynarr);
void dynarr_set(void *item, size_t index, struct dynarr *dynarr);
int dynarr_insert(void *item, struct dynarr *dynarr);
void dynarr_remove_index(size_t index, struct dynarr *dynarr);
void dynarr_remove_all(struct dynarr *dynarr);

// DynArrPtr
struct dynarr_ptr *dynarr_ptr_create(struct dynarr_allocator *allocator);
void dynarr_ptr_destroy(struct dynarr_ptr *dynarr);

#define DYNARR_PTR_SIZE(count, dynarr) (sizeof(void *) * (count))
#define DYNARR_PTR_POSITION(position, dynarr) (dynarr->items + position)
#define DYNARR_PTR_GET(index, dynarr) *DYNARR_PTR_POSITION(index, dynarr)

int dynarr_ptr_resize(size_t new_count, struct dynarr_ptr *dynarr);

void dynarr_ptr_set(size_t index, void *value, struct dynarr_ptr *dynarr);
int dynarr_ptr_insert(void *ptr, struct dynarr_ptr *dynarr);
void dynarr_ptr_move(size_t index, size_t from, size_t to, struct dynarr_ptr *dynarr);
void dynarr_ptr_remove_index(size_t index, struct dynarr_ptr *dynarr);
void dynarr_ptr_remove_ptr(void *ptr, struct dynarr_ptr *dynarr);

#endif