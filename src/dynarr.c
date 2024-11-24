#include "dynarr.h"
#include <assert.h>

// private interface
static void *_alloc_(size_t size, struct dynarr_allocator *allocator);
static void *_realloc_(void *ptr, size_t old_size, size_t new_size, struct dynarr_allocator *allocator);
static void _dealloc_(void *ptr, size_t size, struct dynarr_allocator *allocator);

static size_t padding_size(size_t item_size);
static int resize(size_t new_count, struct dynarr *dynarr);
static void move_items(size_t from, size_t to, size_t count, struct dynarr *dynarr);

#define DYNARR_DETERMINATE_GROW(count) (count == 0 ? DYNARR_DEFAULT_GROW_SIZE : count * 2)
#define DYNARR_LEN(dynarr)(dynarr->used)
#define DYNARR_FREE(dynarr)(dynarr->count - DYNARR_LEN(dynarr))
#define DYNARR_ITEM_SIZE(dynarr)(padding_size(dynarr->size) + dynarr->size)
#define DYNARR_CALC_SIZE(count, dynarr) (DYNARR_ITEM_SIZE(dynarr) * count)
#define DYNARR_ITEMS_SIZE(dynarr) (DYNARR_CALC_SIZE(dynarr->count, dynarr))
#define DYNARR_POSITION(position, dynarr) (((char *)dynarr->items) + (position * DYNARR_ITEM_SIZE(dynarr)))

#define DYNARR_PTR_ITEMS_SIZE(dynarr_ptr) (sizeof(void *) * dynarr_ptr->count)

// private implementation
void *_alloc_(size_t size, struct dynarr_allocator *allocator){
    return allocator ? allocator->alloc(size, allocator->ctx) : malloc(size);
}

void *_realloc_(void *ptr, size_t new_size, size_t old_size, struct dynarr_allocator *allocator){
    return allocator ? allocator->realloc(ptr, new_size, old_size, allocator->ctx) : realloc(ptr, new_size);
}

void _dealloc_(void *ptr, size_t size, struct dynarr_allocator *allocator){
    if (!ptr)
        return;

    if (allocator)
        allocator->dealloc(ptr, size, allocator->ctx);
    else
        free(ptr);
}

size_t padding_size(size_t item_size){
    size_t modulo = item_size & (DYNARR_ALIGNMENT - 1);
    return DYNARR_ALIGNMENT - modulo;
}

int resize(size_t new_count, struct dynarr *dynarr){
    size_t old_size = DYNARR_CALC_SIZE(dynarr->count, dynarr);
    size_t new_size = DYNARR_CALC_SIZE(new_count, dynarr);
    void *items = _realloc_(dynarr->items, new_size, old_size, dynarr->allocator);

    if (!items) return 1;

    dynarr->items = items;
    dynarr->count = new_count;

    return 0;
}

void move_items(size_t from, size_t to, size_t count, struct dynarr *dynarr){
    memmove(
        DYNARR_POSITION(to, dynarr),
        DYNARR_POSITION(from, dynarr),
        DYNARR_ITEM_SIZE(dynarr) * count
    );
}

// public implementation
struct dynarr *dynarr_create(size_t item_size, struct dynarr_allocator *allocator){
    size_t bytes = sizeof(struct dynarr);
    struct dynarr *vector = (struct dynarr *)_alloc_(bytes, allocator);

    vector->used = 0;
    vector->count = 0;
    vector->size = item_size;
    vector->items = NULL;
    vector->allocator = allocator;

    return vector;
}

void dynarr_destroy(struct dynarr *dynarr){
    if (!dynarr)
        return;

    struct dynarr_allocator *allocator = dynarr->allocator;
    size_t size = DYNARR_ITEMS_SIZE(dynarr);

    _dealloc_(dynarr->items, size, allocator);

    dynarr->used = 0;
    dynarr->count = 0;
    dynarr->items = NULL;
    dynarr->allocator = NULL;

    _dealloc_(dynarr, sizeof(struct dynarr), allocator);
}

void *dynarr_get(size_t index, struct dynarr *dynarr){
    void *value = DYNARR_POSITION(index, dynarr);
    return value;
}

void dynarr_set(void *item, size_t index, struct dynarr *dynarr){
    memmove(DYNARR_POSITION(index, dynarr), item, dynarr->size);
}

int dynarr_insert(void *item, struct dynarr *dynarr){
    if (dynarr->used >= dynarr->count)
    {
        size_t new_count = DYNARR_DETERMINATE_GROW(dynarr->count);
        if (resize(new_count, dynarr)) return 1;
    }

    void *slot = DYNARR_POSITION(dynarr->used++, dynarr);
    memmove(slot, item, dynarr->size);

    return 0;
}

int dynarr_insert_at(size_t index, void *item, struct dynarr *dynarr){
    if(DYNARR_LEN(dynarr) == 0)
        return dynarr_insert(item, dynarr);

    if (dynarr->used >= dynarr->count)
    {
        size_t new_count = DYNARR_DETERMINATE_GROW(dynarr->count);
        if (resize(new_count, dynarr)) return 1;
    }

    move_items(index, index + 1, DYNARR_LEN(dynarr) - index, dynarr);
    dynarr_set(item, index, dynarr);

    dynarr->used++;

    return 0;
}

int dynarr_append(struct dynarr *from, struct dynarr *to){
    size_t free = DYNARR_FREE(to);
    size_t len = DYNARR_LEN(from);

    if(len > free){
        size_t diff = len - free;
        size_t by = diff / DYNARR_DEFAULT_GROW_SIZE + 1;
        size_t new_count = to->count + DYNARR_DEFAULT_GROW_SIZE * by;
        
        if(resize(new_count, to)) return 1;
    }

    memmove(
        DYNARR_POSITION(to->used, to),
        DYNARR_POSITION(0, from),
        DYNARR_CALC_SIZE(len, from)
    );

    to->used += len;

    return 0;
}

void dynarr_remove_index(size_t index, struct dynarr *dynarr){   
    size_t last_index = dynarr->used == 0 ? 0 : dynarr->used - 1;

    if(index < last_index){
        size_t from = index + 1;
        size_t to = index;
        size_t count = dynarr->used - 1;
        move_items(from, to, count, dynarr);
    }

    dynarr->used--;
}

void dynarr_remove_all(struct dynarr *dynarr){
    resize(DYNARR_DEFAULT_GROW_SIZE, dynarr);
    dynarr->used = 0;
}

struct dynarr_ptr *dynarr_ptr_create(struct dynarr_allocator *allocator){
    struct dynarr_ptr *dynarr = (struct dynarr_ptr *)_alloc_(sizeof(struct dynarr_ptr), allocator);

    if (!dynarr)
        return NULL;

    dynarr->used = 0;
    dynarr->count = 0;
    dynarr->items = NULL;
    dynarr->allocator = allocator;

    return dynarr;
}

void dynarr_ptr_destroy(struct dynarr_ptr *dynarr){
    if (!dynarr)
        return;

    size_t size = DYNARR_PTR_ITEMS_SIZE(dynarr);
    _dealloc_(dynarr->items, size, dynarr->allocator);

    dynarr->used = 0;
    dynarr->count = 0;
    dynarr->items = NULL;

    _dealloc_(dynarr, sizeof(struct dynarr_ptr), dynarr->allocator);
}

int dynarr_ptr_resize(size_t new_count, struct dynarr_ptr *dynarr){
    size_t old_size = DYNARR_PTR_ITEMS_SIZE(dynarr);
    size_t new_size = sizeof(void *) * new_count;
    void **items = _realloc_(dynarr->items, new_size, old_size, dynarr->allocator);

    if (!items)
        return 1;

    for (size_t i = dynarr->count; i < new_count; i++)
    {
        items[i] = NULL;
    }

    dynarr->items = items;
    dynarr->count = new_count;

    return 0;
}

void dynarr_ptr_set(size_t index, void *value, struct dynarr_ptr *dynarr){
    *DYNARR_PTR_POSITION(index, dynarr) = value;
}

int dynarr_ptr_insert(void *ptr, struct dynarr_ptr *dynarr){
    if (dynarr->used >= dynarr->count && dynarr_ptr_resize(DYNARR_DETERMINATE_GROW(dynarr->count), dynarr))
        return 1;

    *(dynarr->items + (dynarr->used++)) = ptr;

    return 0;
}

void dynarr_ptr_move(size_t index, size_t from, size_t to, struct dynarr_ptr *dynarr){
    for (size_t o = index, i = from; i < to; o++, i++)
        dynarr->items[o] = dynarr->items[i];
}

void dynarr_ptr_remove_index(size_t index, struct dynarr_ptr *dynarr){
    if (index < dynarr->used - 1)
        dynarr_ptr_move(index, index + 1, dynarr->used, dynarr);

    if (dynarr->used - 1 > DYNARR_DEFAULT_GROW_SIZE && dynarr->used - 1 < dynarr->count / 2)
        dynarr_ptr_resize(dynarr->count / 2, dynarr);

    dynarr->used--;
}

void dynarr_ptr_remove_ptr(void *ptr, struct dynarr_ptr *dynarr){
    for (size_t i = 0; i < dynarr->used; i++)
    {
        if (*(dynarr->items + i) == ptr)
        {
            dynarr_ptr_remove_index(i, dynarr);
            return;
        }
    }
}