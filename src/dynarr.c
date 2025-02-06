#include "dynarr.h"
#include <assert.h>

#define DYNARR_SIZE sizeof(struct dynarr)

// PRIVATE INTERFACE
static void *lzalloc(size_t size, DynArrAllocator *allocator);
static void *lzrealloc(void *ptr, size_t old_size, size_t new_size, DynArrAllocator *allocator);
static void lzdealloc(void *ptr, size_t size, DynArrAllocator *allocator);

static size_t padding_size(size_t item_size);
static int dynarr_resize(size_t new_count, DynArr *dynarr);

#define GET_ITEM(index, size, buff)((char *)buff + ((index) * size))
#define SET_ITEM(index, size, item, buff)(memmove(GET_ITEM(index, size, buff), item, size))

static void quick_sort(void *buff, size_t size, size_t len, int (*comparator)(void *a, void *b), DynArrAllocator *allocator);

#define DYNARR_DETERMINATE_GROW(count) (count == 0 ? DYNARR_DEFAULT_GROW_SIZE : count * 2)
#define DYNARR_FREE(dynarr)(dynarr->count - DYNARR_LEN(dynarr))
#define DYNARR_ITEM_SIZE(dynarr)(dynarr->padding + dynarr->size)
#define DYNARR_CALC_SIZE(count, dynarr) (DYNARR_ITEM_SIZE(dynarr) * count)
#define DYNARR_ITEMS_SIZE(dynarr) (DYNARR_CALC_SIZE(dynarr->count, dynarr))
#define DYNARR_POSITION(position, dynarr) (dynarr->items + (position * DYNARR_ITEM_SIZE(dynarr)))

#define DYNARR_MOVE_ITEMS(from, to, count, dynarr) \
    memmove(                                       \
        DYNARR_POSITION(to, dynarr),               \
        DYNARR_POSITION(from, dynarr),             \
        DYNARR_ITEM_SIZE(dynarr) * count           \
    )

#define DYNARR_PTR_ITEMS_SIZE(dynarr_ptr) (sizeof(void *) * dynarr_ptr->count)
int dynarr_ptr_resize(size_t new_count, DynArrPtr *dynarr);

// PRIVATE IMPLEMENTATION
void *lzalloc(size_t size, DynArrAllocator *allocator){
    return allocator ? allocator->alloc(size, allocator->ctx) : malloc(size);
}

void *lzrealloc(void *ptr, size_t old_size, size_t new_size, DynArrAllocator *allocator){
    return allocator ? allocator->realloc(ptr, old_size, new_size, allocator->ctx) : realloc(ptr, new_size);
}

void lzdealloc(void *ptr, size_t size, DynArrAllocator *allocator){
    if (!ptr) return;

    if (allocator) allocator->dealloc(ptr, size, allocator->ctx);
    else free(ptr);
}

size_t padding_size(size_t item_size){
    size_t modulo = item_size & (DYNARR_ALIGNMENT - 1);
    return modulo == 0 ? 0 : DYNARR_ALIGNMENT - modulo;
}

int dynarr_resize(size_t new_count, DynArr *dynarr){
    size_t old_size = DYNARR_CALC_SIZE(dynarr->count, dynarr);
    size_t new_size = DYNARR_CALC_SIZE(new_count, dynarr);
    void *items = lzrealloc(dynarr->items, old_size, new_size, dynarr->allocator);

    if (!items) return 1;

    dynarr->items = items;
    dynarr->count = new_count;

    return 0;
}

void quick_sort(void *buff, size_t size, size_t len, int (*comparator)(void *a, void *b), DynArrAllocator *allocator){
    if(len < 2) return;
    
    if(len == 2){
        void *a = GET_ITEM(0, size, buff);
        void *b = GET_ITEM(1, size, buff);
        char c[size];

        memcpy(c, b, size);

        int comparation = comparator(a, b);

        if(comparation > 0){
            SET_ITEM(1, size, a, buff);
            SET_ITEM(0, size, c, buff);
        }
        
        return;
    }
    
    char *temp_buff = (char *)lzalloc(size * len, allocator);
    assert(temp_buff);
    
    char pivot[size];
    size_t pivot_index = 0;
    memcpy(pivot, GET_ITEM(pivot_index, size, buff), size); 

    size_t less = 0;
    size_t greater = 0;

    for (size_t i = 0; i < len; i++){
        if(i == pivot_index) continue;

        void *item = GET_ITEM(i, size, buff);
        int comparation = comparator(item, pivot);

        if(comparation < 0){
            size_t lindex = less++;
            SET_ITEM(lindex, size, item, temp_buff);
        }

        if(comparation >= 0){
            size_t rindex = len - 1 - greater++;
            SET_ITEM(rindex, size, item, temp_buff);
        }
    }

    char *less_buff = GET_ITEM(0, size, temp_buff);
    char *greater_buff = GET_ITEM(len - greater, size, temp_buff);

    quick_sort(less_buff, size, less, comparator, allocator);
    quick_sort(greater_buff, size, greater, comparator, allocator);
    
    SET_ITEM(less, size, pivot, temp_buff);
    memcpy(buff, temp_buff, size * len);

    lzdealloc(temp_buff, size * len, allocator);

    return;
}

// public implementation
DynArr *dynarr_create(size_t item_size, DynArrAllocator *allocator){
    DynArr *dynarr = (DynArr *)lzalloc(DYNARR_SIZE, allocator);
    
    if(!dynarr) return NULL;

    dynarr->used = 0;
    dynarr->count = 0;
    dynarr->size = item_size;
    dynarr->padding = padding_size(item_size);
    dynarr->items = NULL;
    dynarr->allocator = allocator;

    return dynarr;
}

void dynarr_destroy(DynArr *dynarr){
    if (!dynarr) return;

    DynArrAllocator *allocator = dynarr->allocator;
    size_t size = DYNARR_ITEMS_SIZE(dynarr);

    lzdealloc(dynarr->items, size, allocator);
    memset(dynarr, 0, DYNARR_SIZE);
    lzdealloc(dynarr, DYNARR_SIZE, allocator);
}

void dynarr_reverse(DynArr *dynarr){
    size_t until = DYNARR_LEN(dynarr) / 2;

    for (size_t left_index = 0; left_index < until; left_index++){
        size_t right_index = dynarr->used - 1 - left_index;
        char *left = DYNARR_POSITION(left_index, dynarr);
        char *right = DYNARR_POSITION(right_index, dynarr);
        char foo[DYNARR_ITEM_SIZE(dynarr)];

        memcpy(foo, left, DYNARR_ITEM_SIZE(dynarr));
        
        DYNARR_SET(right, left_index, dynarr);
        DYNARR_SET(foo, right_index, dynarr);
    }
}

void dynarr_sort(int (*comparator)(void *a, void *b), DynArr *dynarr){
    quick_sort(dynarr->items, DYNARR_ITEM_SIZE(dynarr), dynarr->used, comparator, dynarr->allocator);
}

int dynarr_ptr_resize(size_t new_count, DynArrPtr *dynarr){
    size_t old_size = DYNARR_PTR_ITEMS_SIZE(dynarr);
    size_t new_size = sizeof(void *) * new_count;
    void **items = lzrealloc(dynarr->items, old_size, new_size, dynarr->allocator);

    if (!items)
        return 1;

    for (size_t i = dynarr->count; i < new_count; i++){
        items[i] = NULL;
    }

    dynarr->items = items;
    dynarr->count = new_count;

    return 0;
}

int dynarr_find(void *b, int (*comparator)(void *a, void *b), DynArr *dynarr){
    int left = 0;
    int right = DYNARR_LEN(dynarr) == 0 ? 0 : DYNARR_LEN(dynarr) - 1;

    while (right >= 0 && left < (int)DYNARR_LEN(dynarr)){
        int middle_index = (left + right) / 2;
        void *middle = DYNARR_GET((size_t)middle_index, dynarr);
        int comparition = comparator(middle, b);
        
        if(comparition < 0) left = middle_index + 1;
        else if(comparition > 0) right = middle_index - 1;
        else return middle_index;
    }
    
    return -1;
}

void *dynarr_get_ptr(size_t index, DynArr *dynarr){
    assert(dynarr->size == sizeof(uintptr_t));
    return (void *)(DYNARR_GET_AS(uintptr_t, index, dynarr));
}

void dynarr_set_ptr(void *ptr, size_t index, DynArr *dynarr){
    assert(dynarr->size == sizeof(uintptr_t));
    uintptr_t iptr = (uintptr_t)ptr;
    memmove(DYNARR_GET(index, dynarr), (void *)(&iptr), dynarr->size);
}

int dynarr_insert(void *item, DynArr *dynarr){
    if (dynarr->used >= dynarr->count){
        size_t new_count = DYNARR_DETERMINATE_GROW(dynarr->count);
        if (dynarr_resize(new_count, dynarr)) return 1;
    }

    void *slot = DYNARR_POSITION(dynarr->used++, dynarr);
    memmove(slot, item, dynarr->size);

    return 0;
}

int dynarr_insert_at(size_t index, void *item, DynArr *dynarr){
    if(DYNARR_LEN(dynarr) == 0)
        return dynarr_insert(item, dynarr);

    if (dynarr->used >= dynarr->count){
        size_t new_count = DYNARR_DETERMINATE_GROW(dynarr->count);
        if (dynarr_resize(new_count, dynarr)) return 1;
    }

    DYNARR_MOVE_ITEMS(index, index + 1, DYNARR_LEN(dynarr) - index, dynarr);
    DYNARR_SET(item, index, dynarr);

    dynarr->used++;

    return 0;
}

int dynarr_insert_ptr(void *ptr, DynArr *dynarr){
    assert(dynarr->size == sizeof(uintptr_t));
    uintptr_t iptr = (uintptr_t)ptr;
    return dynarr_insert(&iptr, dynarr);
}

int dynarr_insert_ptr_at(size_t index, void *ptr, DynArr *dynarr){
    assert(dynarr->size == sizeof(uintptr_t));
    uintptr_t iptr = (uintptr_t)ptr;
    return dynarr_insert_at(index, &iptr, dynarr);
}

int dynarr_append(DynArr *from, DynArr *to){
    size_t free = DYNARR_FREE(to);
    size_t len = DYNARR_LEN(from);

    if(len > free){
        size_t diff = len - free;
        size_t by = diff / DYNARR_DEFAULT_GROW_SIZE + 1;
        size_t new_count = to->count + DYNARR_DEFAULT_GROW_SIZE * by;
        
        if(dynarr_resize(new_count, to)) return 1;
    }

    memmove(
        DYNARR_POSITION(to->used, to),
        DYNARR_POSITION(0, from),
        DYNARR_CALC_SIZE(len, from)
    );

    to->used += len;

    return 0;
}

void dynarr_remove_index(size_t index, DynArr *dynarr){   
    size_t last_index = dynarr->used == 0 ? 0 : dynarr->used - 1;

    if(index < last_index){
        size_t from = index + 1;
        size_t to = index;
        size_t count = dynarr->used - 1;
        
        DYNARR_MOVE_ITEMS(from, to, count, dynarr);
    }

    dynarr->used--;
}

int dynarr_remove_all(DynArr *dynarr){
    if(!dynarr_resize(DYNARR_DEFAULT_GROW_SIZE, dynarr))
        dynarr->used = 0;
    return 1;
}

DynArrPtr *dynarr_ptr_create(DynArrAllocator *allocator){
    DynArrPtr *dynarr = (DynArrPtr *)lzalloc(sizeof(DynArrPtr), allocator);

    if (!dynarr) return NULL;

    dynarr->used = 0;
    dynarr->count = 0;
    dynarr->items = NULL;
    dynarr->allocator = allocator;

    return dynarr;
}

void dynarr_ptr_destroy(DynArrPtr *dynarr){
    if (!dynarr) return;

    size_t size = DYNARR_PTR_ITEMS_SIZE(dynarr);
    lzdealloc(dynarr->items, size, dynarr->allocator);

    dynarr->used = 0;
    dynarr->count = 0;
    dynarr->items = NULL;

    lzdealloc(dynarr, sizeof(DynArrPtr), dynarr->allocator);
}

void dynarr_ptr_set(size_t index, void *value, DynArrPtr *dynarr){
    *DYNARR_PTR_POSITION(index, dynarr) = value;
}

int dynarr_ptr_insert(void *ptr, DynArrPtr *dynarr){
    if (dynarr->used >= dynarr->count && dynarr_ptr_resize(DYNARR_DETERMINATE_GROW(dynarr->count), dynarr))
        return 1;

    *(dynarr->items + (dynarr->used++)) = ptr;

    return 0;
}

void dynarr_ptr_move(size_t index, size_t from, size_t to, DynArrPtr *dynarr){
    for (size_t o = index, i = from; i < to; o++, i++)
        dynarr->items[o] = dynarr->items[i];
}

void dynarr_ptr_remove_index(size_t index, DynArrPtr *dynarr){
    if (index < dynarr->used - 1)
        dynarr_ptr_move(index, index + 1, dynarr->used, dynarr);

    if (dynarr->used - 1 > DYNARR_DEFAULT_GROW_SIZE && dynarr->used - 1 < dynarr->count / 2)
        dynarr_ptr_resize(dynarr->count / 2, dynarr);

    dynarr->used--;
}

void dynarr_ptr_remove_ptr(void *ptr, DynArrPtr *dynarr){
    for (size_t i = 0; i < dynarr->used; i++){
        if (*(dynarr->items + i) == ptr){
            dynarr_ptr_remove_index(i, dynarr);
            return;
        }
    }
}