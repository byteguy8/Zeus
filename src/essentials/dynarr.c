#include "dynarr.h"

struct dynarr{
    size_t used;
    size_t capacity;
    size_t item_size;
    char *items;
    const DynArrAllocator *allocator;
};

// PRIVATE INTERFACE
static void *lzalloc(size_t size, const DynArrAllocator *allocator);
static void *lzrealloc(
    void *ptr,
    size_t old_size,
    size_t new_size,
    const DynArrAllocator *allocator
);
static void lzdealloc(void *ptr, size_t size, const DynArrAllocator *allocator);

#define MEMORY_ALLOC(_type, _count, _allocator) \
    ((_type *)lzalloc(sizeof(_type) * (_count), (_allocator)))

#define MEMORY_REALLOC(_ptr, _type, _old_count, _new_count, _allocator) \
    ((_type *)(lzrealloc((_ptr), sizeof(_type) * (_old_count), sizeof(_type) * (_new_count), (_allocator))))

#define MEMORY_DEALLOC(_ptr, _type, _count, _allocator) \
    (lzdealloc((_ptr), sizeof(_type) * (_count), (_allocator)))

static int grow(DynArr *dynarr);
static int grow_by(DynArr *dynarr, size_t new_count);
static int shrink(DynArr *dynarr);
static inline void *get_slot(const DynArr *dynarr, size_t idx);
#define CALC_ITMS_MOV_COUNT(_len, _from) ((_len) - (_from))
static inline void move_items(DynArr *dynarr, size_t from, size_t to);

// PRIVATE IMPLEMENTATION
void *lzalloc(size_t size, const DynArrAllocator *allocator){
    return allocator ? allocator->alloc(size, allocator->ctx) : malloc(size);
}

void *lzrealloc(
    void *ptr,
    size_t old_size,
    size_t new_size,
    const DynArrAllocator *allocator
){
    return allocator ?
           allocator->realloc(ptr, old_size, new_size, allocator->ctx) :
           realloc(ptr, new_size);
}

void lzdealloc(void *ptr, size_t size, const DynArrAllocator *allocator){
    if (allocator){
        allocator->dealloc(ptr, size, allocator->ctx);
    }else{
        free(ptr);
    }
}

static int grow(DynArr *dynarr){
    size_t item_size = dynarr->item_size;
    size_t old_count = dynarr->capacity;
    size_t new_count = old_count == 0 ? DYNARR_DEFAULT_GROW_SIZE : old_count * 2;
    size_t old_size = old_count * item_size;
    size_t new_size = new_count * item_size;

    void *new_items = MEMORY_REALLOC(
        dynarr->items,
        char,
        old_size,
        new_size,
        dynarr->allocator
    );

    if (!new_items){
        return 1;
    }

    dynarr->capacity = new_count;
    dynarr->items = new_items;

    return 0;
}

static int grow_by(DynArr *dynarr, size_t new_count){
    size_t item_size = dynarr->item_size;
    size_t old_count = dynarr->capacity;
    size_t old_size = old_count * item_size;
    size_t new_size = new_count * item_size;

    void *new_items = MEMORY_REALLOC(
        dynarr->items,
        char,
        old_size,
        new_size,
        dynarr->allocator
    );

    if (!new_items){
        return 1;
    }

    dynarr->capacity = new_count;
    dynarr->items = new_items;

    return 0;
}

static int shrink(DynArr *dynarr){
    size_t item_size = dynarr->item_size;
    size_t old_count = dynarr->capacity;
    size_t new_count = old_count / 2;
    size_t old_size = old_count * item_size;
    size_t new_size = new_count * item_size;

    void *new_items = MEMORY_REALLOC(
        dynarr->items,
        char,
        old_size,
        new_size,
        dynarr->allocator
    );

    if (!new_items){
        return 1;
    }

    dynarr->capacity = new_count;
    dynarr->items = new_items;

    return 0;
}

static inline void *get_slot(const DynArr *dynarr, size_t idx){
    return ((char *)(dynarr->items)) + (idx * dynarr->item_size);
}

static inline void move_items(DynArr *dynarr, size_t from, size_t to){
    size_t itms_mov_count = CALC_ITMS_MOV_COUNT(dynarr_len(dynarr), from);

    if(from == to || itms_mov_count == 0){
        return;
    }

    memmove(
        get_slot(dynarr, to),
        get_slot(dynarr, from),
        itms_mov_count * dynarr->item_size
    );
}

// public implementation
DynArr *dynarr_init(void *raw_dynarr, size_t item_size, const DynArrAllocator *allocator){
    DynArr *dynarr = raw_dynarr;

    dynarr->used = 0;
    dynarr->capacity = 0;
    dynarr->item_size = item_size;
    dynarr->items = NULL;
    dynarr->allocator = allocator;

    return dynarr;
}

DynArr *dynarr_create(const DynArrAllocator *allocator, size_t item_size){
    DynArr *dynarr = MEMORY_ALLOC(DynArr, 1, allocator);

    if(!dynarr){
        return NULL;
    }

    dynarr->used = 0;
    dynarr->capacity = 0;
    dynarr->item_size = item_size;
    dynarr->items = NULL;
    dynarr->allocator = allocator;

    return dynarr;
}

DynArr *dynarr_create_by(
    const DynArrAllocator *allocator,
    size_t item_size,
    size_t item_count
){
    size_t by = item_count / DYNARR_DEFAULT_GROW_SIZE + 1;
    size_t new_capacity = DYNARR_DEFAULT_GROW_SIZE * by;
    void *items = MEMORY_ALLOC(char, item_size * new_capacity, allocator);
    DynArr *dynarr = MEMORY_ALLOC(DynArr, 1, allocator);

    if(!items || !dynarr){
        MEMORY_DEALLOC(items, char, item_size * item_count, allocator);
        MEMORY_DEALLOC(dynarr, DynArr, 1, allocator);

        return NULL;
    }

    dynarr->used = 0;
    dynarr->capacity = new_capacity;
    dynarr->item_size = item_size;
    dynarr->items = items;
    dynarr->allocator = allocator;

    return dynarr;
}

void dynarr_deinit(DynArr *dynarr){
    if (!dynarr){
        return;
    }

    MEMORY_DEALLOC(
        dynarr->items,
        char,
        dynarr->item_size * dynarr->capacity,
        dynarr->allocator
    );
}

void dynarr_destroy(DynArr *dynarr){
    if (!dynarr){
        return;
    }

    const DynArrAllocator *allocator = dynarr->allocator;

    MEMORY_DEALLOC(
        dynarr->items,
        char,
        dynarr->item_size * dynarr->capacity,
        allocator
    );
    MEMORY_DEALLOC(
        dynarr,
        DynArr,
        1,
        allocator
    );
}

inline size_t dynarr_len(const DynArr *dynarr){
    return dynarr->used;
}

inline size_t dynarr_capacity(const DynArr *dynarr){
    return dynarr->capacity;
}

inline size_t dynarr_item_size(const DynArr *dynarr){
    return dynarr->item_size;
}

inline size_t dynarr_available(const DynArr *dynarr){
    return dynarr->capacity - dynarr->used;
}

inline int dynarr_make_room(DynArr *dynarr, size_t count){
    size_t by = count / DYNARR_DEFAULT_GROW_SIZE + 1;
    size_t new_capacity = DYNARR_DEFAULT_GROW_SIZE * by + dynarr->capacity;

    return grow_by(dynarr, new_capacity);
}

inline int dynarr_reduce(DynArr *dynarr){
    if(dynarr_len(dynarr) < dynarr->capacity / 2){
        return !shrink(dynarr);
    }

    return 0;
}

void dynarr_reverse(DynArr *dynarr){
    size_t item_size = dynarr->item_size;
    size_t len = dynarr_len(dynarr);
    size_t until = dynarr_len(dynarr) / 2;

    for (size_t left_index = 0; left_index < until; left_index++){
        size_t right_index = len - 1 - left_index;
        char *left = get_slot(dynarr, left_index);
        char *right = get_slot(dynarr, right_index);
        char temp_item[item_size];

        memcpy(temp_item, left, item_size);
        dynarr_set_at(dynarr, left_index, right);
        dynarr_set_at(dynarr, right_index, temp_item);
    }
}

inline void dynarr_sort(DynArr *dynarr, DynArrComparator comparator){
    qsort(dynarr->items, dynarr->used, dynarr->item_size, comparator);
}

int dynarr_find(const DynArr *dynarr, const void *item, DynArrComparator comparator){
    size_t len = dynarr_len(dynarr);
    int low = 0;
    int high = len == 0 ? 0 : len - 1;

    while (low <= high){
        int middle_index = low + (high - low) / 2;
        void *middle = get_slot(dynarr, (size_t)middle_index);
        int comparison = comparator(middle, item);

        if(comparison < 0){
            low = middle_index + 1;
        }else if(comparison > 0){
            high = middle_index - 1;
        }else{
            return middle_index;
        }
    }

    return -1;
}

inline void *dynarr_get_raw(const DynArr *dynarr, size_t idx){
    if(idx >= dynarr_len(dynarr)){
        return NULL;
    }

    return get_slot(dynarr, idx);
}

inline void *dynarr_get_ptr(const DynArr *dynarr, size_t idx){
    if(idx >= dynarr_len(dynarr)){
        return NULL;
    }

    return (void *)(*(uintptr_t *)get_slot(dynarr, idx));
}

inline int dynarr_set_at(DynArr *dynarr, size_t idx, const void *item){
    if(idx >= dynarr_len(dynarr)){
        return IDX_OUT_OF_BOUNDS_ERR_DYNARR_CODE;
    }

    memmove(get_slot(dynarr, idx), item, dynarr->item_size);

    return OK_DYNARR_CODE;
}

inline int dynarr_set_ptr(DynArr *dynarr, size_t idx, const void *ptr){
    if(idx >= dynarr_len(dynarr)){
        return IDX_OUT_OF_BOUNDS_ERR_DYNARR_CODE;
    }

    size_t rsize = dynarr->item_size;
    uintptr_t iptr = (uintptr_t)ptr;

    memmove(get_slot(dynarr, idx), &iptr, rsize);

    return OK_DYNARR_CODE;
}

inline int dynarr_insert(DynArr *dynarr, const void *item){
    if (dynarr->used >= dynarr->capacity && grow(dynarr)){
        return ALLOC_ERR_DYNARR_CODE;
    }

    memmove(get_slot(dynarr, dynarr->used++), item, dynarr->item_size);

    return OK_DYNARR_CODE;
}

int dynarr_insert_at(DynArr *dynarr, size_t idx, const void *item){
    size_t len = dynarr_len(dynarr);

    if(idx > len){
        return IDX_OUT_OF_BOUNDS_ERR_DYNARR_CODE;
    }

    if(dynarr_available(dynarr) == 0 && grow(dynarr)){
        return ALLOC_ERR_DYNARR_CODE;
    }

    move_items(dynarr, idx, idx + 1);
    memmove(get_slot(dynarr, idx), item, dynarr->item_size);

    dynarr->used++;

    return OK_DYNARR_CODE;
}

inline int dynarr_insert_ptr(DynArr *dynarr, const void *ptr){
    if(dynarr->item_size != sizeof(uintptr_t)){
        return INCORRECT_SIZE_ERR_DYNARR_CODE;
    }

    uintptr_t iptr = (uintptr_t)ptr;

    return dynarr_insert(dynarr, &iptr);
}

inline int dynarr_insert_ptr_at(DynArr *dynarr, size_t idx, const void *ptr){
    if(dynarr->item_size != sizeof(uintptr_t)){
        return INCORRECT_SIZE_ERR_DYNARR_CODE;
    }

    uintptr_t iptr = (uintptr_t)ptr;

    return dynarr_insert_at(dynarr, idx, &iptr);
}

int dynarr_append(DynArr *to, const DynArr *from){
    size_t from_len = dynarr_len(from);

    if(from_len == 0){
        return DYNARR_EMPTY_ERR_DYNARR_CODE;
    }

    size_t to_len = dynarr_len(to);
    size_t to_available = dynarr_available(to);
    size_t to_start_idx = to_len;

    if(from_len <= to_available){
        memmove(
            get_slot(to, to_start_idx),
            get_slot(from, 0),
            from->item_size * from_len
        );

        to->used += from_len;

        return OK_DYNARR_CODE;
    }

    if(dynarr_make_room(to, from_len - to_available)){
        return ALLOC_ERR_DYNARR_CODE;
    }

    memmove(
        get_slot(to, to_start_idx),
        get_slot(from, 0),
        from->item_size * from_len
    );

    to->used += from_len;

    return OK_DYNARR_CODE;
}

int dynarr_join(
    const DynArrAllocator *allocator,
    const DynArr *a_dynarr,
    const DynArr *b_dynarr,
    DynArr **out_new_dynarr
){
    size_t a_item_size = a_dynarr->item_size;
    size_t b_item_size = b_dynarr->item_size;

    if(a_item_size != b_item_size){
        return SIZE_MISMATCH_ERR_DYNARR_CODE;
    }

    #define ITEM_SIZE a_item_size

    size_t a_len = dynarr_len(a_dynarr);
    size_t b_len = dynarr_len(b_dynarr);
    size_t c_len = a_len + b_len;
    DynArr *c_dynarr = dynarr_create_by(allocator, ITEM_SIZE, c_len);

    if(!c_dynarr){
        return ALLOC_ERR_DYNARR_CODE;
    }

    memcpy(c_dynarr->items, a_dynarr->items, ITEM_SIZE * a_len);
    memcpy(get_slot(c_dynarr, a_len), b_dynarr->items, ITEM_SIZE * b_len);

    c_dynarr->used = c_len;
    *out_new_dynarr = c_dynarr;

    return OK_DYNARR_CODE;
}

inline int dynarr_remove_index(DynArr *dynarr, size_t idx){
    size_t len = dynarr_len(dynarr);

    if(idx >= len){
        return IDX_OUT_OF_BOUNDS_ERR_DYNARR_CODE;
    }

    if(idx < len - 1){
        move_items(dynarr, idx + 1, idx);
    }

    dynarr->used--;

    return OK_DYNARR_CODE;
}

int dynarr_remove_if(DynArr *dynarr, DynArrPredicate predicate, void *ctx){
    size_t i = 0;
    size_t remove_count  = 0;

    while (i < dynarr_len(dynarr)){
        void *item = dynarr_get_raw(dynarr, i);

        if(predicate(item, ctx)){
            dynarr_remove_index(dynarr, i);

            remove_count++;
        }else{
            i++;
        }
    }

    return remove_count;
}

inline void dynarr_remove_all(DynArr *dynarr){
    dynarr->used = 0;
}