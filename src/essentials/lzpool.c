#include "lzpool.h"
#include <stdlib.h>
#include <assert.h>

//--------------------------------------------------------------------------//
//                            PRIVATE INTERFACE                             //
//--------------------------------------------------------------------------//
#define HEADER_SIZE (sizeof(LZPoolHeader))
#define MAGIC_NUMBER 0xDEADBEEF
//--------------------------------  MEMORY  --------------------------------//
static inline void *lzalloc(size_t size, LZPoolAllocator *allocator);
static inline void *lzrealloc(void *ptr, size_t old_size, size_t new_size, LZPoolAllocator *allocator);
static inline void lzdealloc(void *ptr, size_t size, LZPoolAllocator *allocator);

#define MEMORY_ALLOC(_type, _count, _allocator)((_type *)lzalloc(sizeof(_type) * (_count), (_allocator)))
#define MEMORY_REALLOC(_ptr, _type, _old_count, _new_count, _allocator)((_type *)(lzrealloc((_ptr), sizeof(_type) * (_old_count), sizeof(_type) * (_new_count), (_allocator))))
#define MEMORY_DEALLOC(_ptr, _type, _count, _allocator)(lzdealloc((_ptr), sizeof(_type) * (_count), (_allocator)))

static inline void destroy_subpool(size_t header_size, size_t slot_size, LZSubPool *subpool, LZPoolAllocator *allocator);

static inline size_t round_size(size_t to, size_t size);

static inline void *slot_chunk(size_t header_size, LZPoolHeader *slot);
static inline LZPoolHeader *slot_from_ptr(size_t header_size, void *ptr);
static inline LZPoolHeader *get_slot_at(size_t idx, size_t header_size, size_t slot_size, void *slots);

static void insert_slot(LZPoolHeader *header, LZPoolHeaderList *list);
static void remove_slot(LZPoolHeader *header, LZPoolHeaderList *list);

static void init_subpool_slots(size_t header_size, size_t slot_size, LZSubPool *subpool, LZPoolHeaderList *list, LZPool *pool);
static void insert_subpool(LZSubPool *subpool, LZSubPoolList *list);
static void remove_subpool(LZSubPool *subpool, LZSubPoolList *list);
//--------------------------------------------------------------------------//
//                          PRIVATE IMPLEMENTATION                          //
//--------------------------------------------------------------------------//
static inline void *lzalloc(size_t size, LZPoolAllocator *allocator){
    return allocator ? allocator->alloc(size, allocator->ctx) : malloc(size);
}

static inline void *lzrealloc(void *ptr, size_t old_size, size_t new_size, LZPoolAllocator *allocator){
    return allocator ? allocator->realloc(ptr, old_size, new_size, allocator->ctx) : realloc(ptr, new_size);
}

static inline void lzdealloc(void *ptr, size_t size, LZPoolAllocator *allocator){
    if(allocator){
        allocator->dealloc(ptr, size, allocator->ctx);
    }else{
        free(ptr);
    }
}

static inline void destroy_subpool(size_t header_size, size_t slot_size, LZSubPool *subpool, LZPoolAllocator *allocator){
    MEMORY_DEALLOC(subpool->slots, char, (header_size + slot_size) * subpool->slots_count, allocator);
    MEMORY_DEALLOC(subpool, LZSubPool, 1, allocator);
}

static inline size_t round_size(size_t to, size_t size){
    size_t mod = size % to;
    size_t padding = mod == 0 ? 0 : to - mod;
    return padding + size;
}

static inline void *slot_chunk(size_t header_size, LZPoolHeader *slot){
    return ((char *)slot) + header_size;
}

static inline LZPoolHeader *slot_from_ptr(size_t header_size, void *ptr){
    LZPoolHeader *header = (LZPoolHeader *)(((char *)ptr) - header_size);
    assert(header->magic == MAGIC_NUMBER && "Corrupted memory dectected");
    return header;
}

static inline LZPoolHeader *get_slot_at(size_t idx, size_t header_size, size_t slot_size, void *slots){
    return (LZPoolHeader *)(((char *)slots) + ((header_size + slot_size) * idx));
}

static void insert_slot(LZPoolHeader *header, LZPoolHeaderList *list){
    if(list->tail){
        list->tail->next = header;
        header->prev = list->tail;
    }else{
        list->head = header;
    }

    list->len++;
    list->tail = header;
}

static void remove_slot(LZPoolHeader *header, LZPoolHeaderList *list){
    if(header == list->head){
        list->head = header->next;
    }

    if(header == list->tail){
        list->tail = header->prev;
    }

    list->len--;

    if(header->prev){
        header->prev->next = header->next;
    }

    if(header->next){
        header->next->prev = header->prev;
    }
}

static void init_subpool_slots(size_t header_size, size_t slot_size, LZSubPool *subpool, LZPoolHeaderList *list, LZPool *pool){
    size_t slots_count = subpool->slots_count;
    char *offset = subpool->slots;
    LZPoolHeader *prev = NULL;

    for (size_t i = 0; i < slots_count; i++){
        char *next_offset = i + 1 < slots_count ? offset + (header_size + slot_size) : NULL;
        LZPoolHeader *current = (LZPoolHeader *)offset;
        LZPoolHeader *next = (LZPoolHeader *)next_offset;

        current->magic = MAGIC_NUMBER;
        current->used = 0;
        current->pool = pool;
        current->subpool = subpool;
        current->prev = prev;
        current->next = NULL;

        if(next){
            current->next = next;

            next->magic = MAGIC_NUMBER;
            next->used = 0;
            next->pool = pool;
            next->subpool = subpool;
            next->prev = current;
            next->next = NULL;
        }

        offset = next_offset;
        prev = current;
    }

    offset = (char *)subpool->slots;
    LZPoolHeader *first = (LZPoolHeader *)offset;
    LZPoolHeader *last = get_slot_at(slots_count - 1, header_size, slot_size, offset);

    if(list->tail){
        list->tail->next = first;
        first->prev = list->tail;
    }else{
        list->head = first;
        list->tail = last;
    }

    list->len += slots_count;
}

static void insert_subpool(LZSubPool *subpool, LZSubPoolList *list){
    if(list->tail){
        list->tail->next = subpool;
        subpool->prev = list->tail;
    }else{
        list->head = subpool;
    }

    list->len++;
    list->tail = subpool;
}

static void remove_subpool(LZSubPool *subpool, LZSubPoolList *list){
    if(subpool == list->head){
        list->head = subpool->next;
    }

    if(subpool == list->tail){
        list->tail = subpool->prev;
    }

    list->len--;

    if(subpool->prev){
        subpool->prev->next = subpool->next;
    }

    if(subpool->next){
        subpool->next = subpool->prev;
    }
}
//--------------------------------------------------------------------------//
//                          PUBLIC IMPLEMENTATION                           //
//--------------------------------------------------------------------------//
inline void lzpool_init(size_t slot_size, LZPoolAllocator *allocator, LZPool *pool){
    pool->header_size = round_size(LZPOOL_DEFAULT_ALIGNMENT, HEADER_SIZE);
    pool->slot_size = round_size(LZPOOL_DEFAULT_ALIGNMENT, slot_size);
    pool->slots = (LZPoolHeaderList){0};
    pool->subpools = (LZSubPoolList){0};
    pool->allocator = allocator;
}

void lzpool_destroy_deinit(LZPool *pool){
    if(!pool){
        return;
    }

    size_t header_size = pool->header_size;
    size_t slot_size = pool->slot_size;
    LZPoolAllocator *allocator = pool->allocator;
    LZSubPool *current = pool->subpools.head;
    LZSubPool *next = NULL;

    while (current){
        next = current->next;

        destroy_subpool(header_size, slot_size, current, allocator);

        current = next;
    }
}

LZPool *lzpool_create(size_t slot_size, LZPoolAllocator *allocator){
    LZPool *pool = MEMORY_ALLOC(LZPool, 1, allocator);

    if(!pool){
        return NULL;
    }

    pool->header_size = round_size(LZPOOL_DEFAULT_ALIGNMENT, HEADER_SIZE);
    pool->slot_size = round_size(LZPOOL_DEFAULT_ALIGNMENT, slot_size);
    pool->slots = (LZPoolHeaderList){0};
    pool->subpools = (LZSubPoolList){0};
    pool->allocator = allocator;

    return pool;
}

void lzpool_destroy(LZPool *pool){
    if(!pool){
        return;
    }

    size_t header_size = pool->header_size;
    size_t slot_size = pool->slot_size;
    LZPoolAllocator *allocator = pool->allocator;
    LZSubPool *current = pool->subpools.head;
    LZSubPool *next = NULL;

    while (current){
        next = current->next;

        destroy_subpool(header_size, slot_size, current, allocator);

        current = next;
    }

    MEMORY_DEALLOC(pool, LZPool, 1, allocator);
}

int lzpool_prealloc(size_t slots_count, LZPool *pool){
    LZPoolAllocator *allocator = pool->allocator;

    size_t header_size = pool->header_size;
    size_t slot_size = pool->slot_size;
    size_t slots_size = (header_size + slot_size) * slots_count;
    void *slots = MEMORY_ALLOC(char, slots_size, allocator);
    LZSubPool *subpool = MEMORY_ALLOC(LZSubPool, 1, allocator);

    if(!slots || !subpool){
        MEMORY_DEALLOC(slots, char, slot_size, allocator);
        MEMORY_DEALLOC(subpool, LZSubPool, 1, allocator);
        return 1;
    }

    subpool->slots_used = 0;
    subpool->slots_count = slots_count;
    subpool->slots = slots;
    subpool->prev = NULL;
    subpool->next = NULL;

    init_subpool_slots(header_size, slot_size, subpool, &pool->slots, pool);
    insert_subpool(subpool, &pool->subpools);

    return 0;
}

void *lzpool_alloc(LZPool *pool){
    LZPoolHeaderList *slots = &pool->slots;
    LZPoolHeader *slot = slots->head;

    if(slot){
        slot->used = 1;
        slot->subpool->slots_used++;

        remove_slot(slot, slots);

        return slot_chunk(pool->header_size, slot);
    }

    return NULL;
}

void *lzpool_alloc_x(size_t slots_count, LZPool *pool){
    LZPoolHeaderList *slots = &pool->slots;

    if(slots->len == 0 && lzpool_prealloc(slots_count, pool)){
        return NULL;
    }

    return lzpool_alloc(pool);
}

void lzpool_dealloc(void *ptr){
    if(!ptr){
        return;
    }

    size_t header_size = round_size(LZPOOL_DEFAULT_ALIGNMENT, HEADER_SIZE);
    LZPoolHeader *slot = slot_from_ptr(header_size, ptr);
    LZPool *pool = (LZPool *)slot->pool;
    LZSubPool *subpool = slot->subpool;

    assert(slot->used && "Trying to free unused memory");

    insert_slot(slot, &pool->slots);

    slot->used = 0;
    subpool->slots_used++;
}

void lzpool_dealloc_release(void *ptr){
    if(!ptr){
        return;
    }

    size_t header_size = round_size(LZPOOL_DEFAULT_ALIGNMENT, HEADER_SIZE);
    LZPoolHeader *slot = slot_from_ptr(header_size, ptr);
    LZPool *pool = (LZPool *)slot->pool;
    LZSubPool *subpool = slot->subpool;

    assert(slot->used && "Trying to free unused memory");

    insert_slot(slot, &pool->slots);

    slot->used = 0;
    subpool->slots_used--;

    if(subpool->slots_used == 0){
        size_t slot_size = pool->slot_size;
        size_t slots_count = subpool->slots_count;
        void *slots = subpool->slots;

        for (size_t i = 0; i < slots_count; i++){
            LZPoolHeader *current = get_slot_at(i, header_size, slot_size, slots);
            remove_slot(current, &pool->slots);
        }

        remove_subpool(subpool, &pool->subpools);
        destroy_subpool(header_size, slot_size, subpool, pool->allocator);
    }
}