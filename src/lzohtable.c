#include "lzohtable.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

static void *lzalloc(size_t size, LZOHTableAllocator *allocator){
    return allocator ? allocator->alloc(size, allocator->ctx) : malloc(size);
}

static void *lzrealloc(void *ptr, size_t old_size, size_t new_size, LZOHTableAllocator *allocator){
    return allocator ? allocator->realloc(ptr, old_size, new_size, allocator->ctx) : realloc(ptr, new_size);
}

static void lzdealloc(void *ptr, size_t size, LZOHTableAllocator *allocator){
    allocator ? allocator->dealloc(ptr, size, allocator->ctx) : free(ptr);
}

#define MEMORY_ALLOC(_type, _count, _allocator)((_type *)lzalloc(sizeof(_type) * (_count), (_allocator)))
#define MEMORY_REALLOC(_ptr, _type, _old_count, _new_count, allocator)((type *)(lzrealloc((_ptr), sizeof(_type) * (_old_count), sizeof(_type) * (_new_count), (_allocator))))
#define MEMORY_DEALLOC(_ptr, _type, _count, _allocator)(lzdealloc((_ptr), sizeof(_type) * (_count), (_allocator)))

#define SLOT_SIZE sizeof(LZOHTableSlot)

static inline int is_power_of_two(uintptr_t x){
    return (x & (x - 1)) == 0;
}

static uint64_t fnv_1a_hash(const uint8_t *key, size_t key_size){
    const uint64_t prime = 0x00000100000001b3;
    const uint64_t basis = 0xcbf29ce484222325;
    uint64_t hash = basis;

    for (size_t i = 0; i < key_size; i++){
        hash ^= key[i];
        hash *= prime;
    }

    return hash;
}

static LZOHTableSlot* robin_hood_lookup(void *key, size_t size, LZOHTable *table, size_t *out_index){
    uint64_t hash = fnv_1a_hash(key, size);
    size_t index = hash & (table->m - 1);

    size_t i = index;
    size_t probe = 0;

    while (1){
        LZOHTableSlot *slot = &table->slots[i];

        if(!slot->used){
            break;
        }

        if(slot->probe < probe){
            break;
        }

        if(size == slot->key_size && strncmp(key, slot->key, size) == 0){
            if(out_index){
                *out_index = i;
            }

            return slot;
        }

        i = (i + 1) & (table->m - 1);
        probe++;
    }

    return NULL;
}

static int robin_hood_insert(
    uint64_t hash,
    size_t index,
    void *key,
    size_t key_size,
    void *value,
    size_t m,
    LZOHTableSlot *slots,
    LZOHTableSlot **out_slot
){
    char fixed = 0;

    size_t probe = 0;
    char kcpy = 0;
    char vcpy = 0;

    size_t i = index;
    size_t count = 0;

    while(count < m){
        LZOHTableSlot *slot = slots + i;

        if(slot->used){
            if(key_size == slot->key_size && strncmp(key, slot->key, key_size) == 0){
                slot->value = value;

                if(out_slot){
                    *out_slot = slot;
                }

                return 2;
            }

            if(probe > slot->probe){
                if(!fixed){
                    fixed = 1;

                    if(out_slot){
                        *out_slot = slot;
                    }
                }

                size_t rich_probe = slot->probe;
                uint64_t rich_hash = slot->hash;
                char rich_kcpy = slot->kcpy;
                char rich_vcpy = slot->vcpy;
                size_t rich_key_size = slot->key_size;
                void *rich_key = slot->key;
                void *rich_value = slot->value;

                slot->probe = probe;
                slot->hash = hash;
                slot->kcpy = kcpy;
                slot->vcpy = vcpy;
                slot->key_size = key_size;
                slot->key = key;
                slot->value = value;

                probe = rich_probe;
                hash = rich_hash;
                kcpy = rich_kcpy;
                vcpy = rich_vcpy;
                key_size = rich_key_size;
                key = rich_key;
                value = rich_value;
            }
        }else{
            slot->used = 1;

            slot->probe = probe;
            slot->hash = hash;

            slot->kcpy = kcpy;
            slot->vcpy = vcpy;
            slot->key_size = key_size;

            slot->key = key;
            slot->value = value;

            if(!fixed && out_slot){
                *out_slot = slot;
            }

            return 3;
        }

        i = (i + 1) & (m - 1);
        count++;
        probe++;
    }

    return 1;
}

static LZOHTableSlot *create_slots(size_t m, LZOHTableAllocator *allocator){
    assert(is_power_of_two(m));

    LZOHTableSlot *slots = MEMORY_ALLOC(LZOHTableSlot, m, allocator);

    if(!slots){
        return NULL;
    }

    memset(slots, 0, SLOT_SIZE * m);

    return slots;
}

static LZOHTableSlot *resize_slots(size_t m, LZOHTableAllocator *allocator, size_t *out_m){
    assert(is_power_of_two(m));

    size_t count = m * 2;
    LZOHTableSlot *slots = MEMORY_ALLOC(LZOHTableSlot, count, allocator);

    if(!slots){
        return NULL;
    }

    memset(slots, 0, SLOT_SIZE * count);

    if(out_m){
        *out_m = count;
    }

    return slots;
}

static inline void free_slots(size_t m, LZOHTableSlot *slots, LZOHTableAllocator *allocator){
    MEMORY_DEALLOC(slots, LZOHTableSlot, m, allocator);
}

LZOHTable *lzohtable_create(size_t m, float lfth, LZOHTableAllocator *allocator){
    LZOHTableSlot *slots = create_slots(m, allocator);
    LZOHTable *table = MEMORY_ALLOC(LZOHTable, 1, allocator);

    if(!slots || !table){
        free_slots(m, slots, allocator);
        MEMORY_DEALLOC(table, LZOHTable, 1, allocator);

        return NULL;
    }

    table->n = 0;
    table->m = m;
    table->lfth = lfth;
    table->slots = slots;
    table->allocator = allocator;

    return table;
}

void lzohtable_destroy_help(void *extra, void (*destroy)(void *key, void *value, void *extra), LZOHTable *table){
    if(!table){
        return;
    }

    LZOHTableAllocator *allocator = table->allocator;

    if(destroy){
        for (size_t i = 0; i < table->m; i++){
            LZOHTableSlot *slot = &table->slots[i];

            if(!slot->used){
                continue;
            }

            if(slot->kcpy){
                MEMORY_DEALLOC(slot->key, char, slot->key_size, allocator);
                destroy(NULL, slot->value, extra);
            }else{
                destroy(slot->key, slot->value, extra);
            }
        }
    }

    free_slots(table->m, table->slots, allocator);
    MEMORY_DEALLOC(table, LZOHTable, 1, allocator);
}

void lzohtable_print(void (*print)(size_t index, size_t probe, void *key, size_t key_len, void *value), LZOHTable *table){
    for (size_t i = 0; i < table->m; i++){
        LZOHTableSlot *slot = &table->slots[i];

        if(slot->used){
            print(i, slot->probe, slot->key, slot->key_size, slot->value);
        }
    }
}

int copy_paste_slots(LZOHTable *table){
    size_t new_m = table->m;
    LZOHTableSlot *new_slots = resize_slots(new_m, table->allocator, &new_m);

    if(!new_slots){
        return 1;
    }

    for (size_t i = 0; i < table->m; i++){
        LZOHTableSlot *old_slot = &table->slots[i];

        if(!old_slot->used){
            continue;
        }

        uint64_t hash = old_slot->hash;
        size_t index = hash & (new_m - 1);
        LZOHTableSlot *new_slot = NULL;

        robin_hood_insert(
            hash,
            index,
            old_slot->key,
            old_slot->key_size,
            old_slot->value,
            new_m,
            new_slots,
            &new_slot
        );

        new_slot->kcpy = old_slot->kcpy;
        new_slot->vcpy = old_slot->vcpy;
    }

    free_slots(table->m, table->slots, table->allocator);

    table->m = new_m;
    table->slots = new_slots;

    return 0;
}

int lzohtable_lookup(void *key, size_t size, LZOHTable *table, void **out_value){
    uint64_t hash = fnv_1a_hash(key, size);
    size_t index = hash & (table->m - 1);

    size_t i = index;
    size_t probe = 0;

    while (1){
        LZOHTableSlot *slot = &table->slots[i];

        if(!slot->used){
            break;
        }

        if(slot->probe < probe){
            break;
        }

        if(size == slot->key_size && strncmp(key, slot->key, size) == 0){
            if(out_value){
                *out_value = slot->value;
            }

            return 1;
        }

        i = (i + 1) & (table->m - 1);
        probe++;
    }

    return 0;
}

void lzohtable_clear_help(void *extra, void (*destroy)(void *key, void *value, void *extra), LZOHTable *table){
    for (size_t i = 0; i < table->m; i++){
        LZOHTableSlot *slot = &table->slots[i];

        if(slot->used){
            table->n--;
            slot->used = 0;

            if(destroy){
                destroy(slot->key, slot->value, extra);
            }
        }
    }
}

int lzohtable_put(void *key, size_t size, void *value, LZOHTable *table, uint64_t *out_hash){
    if(LZOHTABLE_LOAD_FACTOR(table) >= table->lfth && copy_paste_slots(table)){
        return 1;
    }

    uint64_t hash = fnv_1a_hash(key, size);
    size_t index = hash & (table->m - 1);

    if(robin_hood_insert(hash, index, key, size, value, table->m, table->slots, NULL) == 3){
        table->n++;
    }

    if(out_hash){
        *out_hash = hash;
    }

    return 0;
}

int lzohtable_put_ck(void *key, size_t size, void *value, LZOHTable *table, uint64_t *out_hash){
    if(LZOHTABLE_LOAD_FACTOR(table) >= table->lfth && copy_paste_slots(table)){
        return 1;
    }

    uint64_t hash = fnv_1a_hash(key, size);
    size_t index = hash & (table->m - 1);
    LZOHTableSlot *slot = NULL;

    if(robin_hood_insert(hash, index, key, size, value, table->m, table->slots, &slot) == 3){
        char *ckey = MEMORY_ALLOC(char, size, table->allocator);

        if(!ckey){
            return 1;
        }

        memcpy(ckey, key, size);

        slot->kcpy = 1;
        slot->key = ckey;

        table->n++;
    }

    if(out_hash){
        *out_hash = hash;
    }

    return 0;
}

int lzohtable_put_ckv(void *key, size_t key_size, void *value, size_t value_size, LZOHTable *table, uint64_t *out_hash){
    if(LZOHTABLE_LOAD_FACTOR(table) >= table->lfth && copy_paste_slots(table)){
        return 1;
    }

    uint64_t hash = fnv_1a_hash(key, key_size);
    size_t index = hash & (table->m - 1);
    LZOHTableSlot *slot = NULL;

    if(robin_hood_insert(hash, index, key, key_size, value, table->m, table->slots, &slot) == 3){
        char *ckey = MEMORY_ALLOC(char, key_size, table->allocator);
        char *cvalue = MEMORY_ALLOC(char, value_size, table->allocator);

        if(!ckey || !cvalue){
            MEMORY_DEALLOC(ckey, char, key_size, table->allocator);
            MEMORY_DEALLOC(cvalue, char, value_size, table->allocator);

            return 1;
        }

        memcpy(ckey, key, key_size);
        memcpy(cvalue, value, value_size);

        slot->kcpy = 1;
        slot->vcpy = 1;
        slot->key = ckey;
        slot->value = cvalue;

        table->n++;
    }

    if(out_hash){
        *out_hash = hash;
    }

    return 0;
}

void lzohtable_remove_help(void *key, size_t size, void *extra, void (*destroy)(void *key, void *value, void *extra), LZOHTable *table){
    size_t index;
    LZOHTableSlot *slot = robin_hood_lookup(key, size, table, &index);

    if(!slot){
        return;
    }

    void *k = slot->kcpy ? NULL : slot->key;
    void *v = slot->vcpy ? NULL : slot->value;

    if(destroy){
        destroy(k, v, extra);
    }
    if(slot->kcpy){
        MEMORY_DEALLOC(slot->key, char, slot->key_size, table->allocator);
    }
    if(slot->vcpy){
        MEMORY_DEALLOC(slot->value, char, slot->value_size, table->allocator);
    }

    memset(slot, 0, SLOT_SIZE);

    size_t i = (index + 1) & (table->m - 1);

    while(1){
        LZOHTableSlot *current = &table->slots[i];

        if(current->used){
            if(current->probe == 0){
                break;
            }

            LZOHTableSlot *previous = &table->slots[(i - 1) & (table->m - 1)];

            previous->used = 1;
            previous->probe = current->probe - 1;
            previous->hash = current->hash;
            previous->kcpy = current->kcpy;
            previous->vcpy = current->vcpy;
            previous->key_size = current->key_size;
            previous->value_size = current->value_size;
            previous->key = current->key;
            previous->value = current->value;

            memset(current, 0, SLOT_SIZE);
        }else{
            break;
        }

        i = (i + 1) & (table->m - 1);
    }
}