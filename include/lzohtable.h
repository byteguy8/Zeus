#ifndef LZOHTABLE_H
#define LZOHTABLE_H

#include <stdint.h>
#include <stddef.h>

typedef struct lzohtable_allocator{
    void *ctx;
    void *(*alloc)(size_t size, void *ctx);
    void *(*realloc)(void *ptr, size_t old_size, size_t new_size, void *ctx);
    void (*dealloc)(void *ptr, size_t size, void *ctx);
}LZOHTableAllocator;

typedef struct lzohtable_slot{
    unsigned char used;

    size_t probe;
    uint64_t hash;

    char kcpy;
    char vcpy;
    size_t key_size;
    size_t value_size;

    void *key;
    void *value;
}LZOHTableSlot;

typedef struct lzohtable{
    size_t n;                      // count of distinct elements
    size_t m;                      // count of slots
    float lfth;                    // load factor threshold
    LZOHTableSlot *slots;
    LZOHTableAllocator *allocator;
}LZOHTable;

#define LZOHTABLE_LOAD_FACTOR(_table)(((float)(_table)->n) / ((float)(_table)->m))

LZOHTable *lzohtable_create(size_t m, float lfth, LZOHTableAllocator *allocator);

void lzohtable_destroy_help(void *extra, void (*destroy)(void *key, void *value, void *extra), LZOHTable *table);

#define LZOHTABLE_DESTROY(_table)(lzohtable_destroy_help(NULL, NULL, (_table)))

void lzohtable_print(void (*print)(size_t index, size_t probe, void *key, size_t key_len, void *value), LZOHTable *table);

int lzohtable_lookup(void *key, size_t size, LZOHTable *table, void **out_value);

void lzohtable_clear_help(void *extra, void (*destroy)(void *key, void *value, void *extra), LZOHTable *table);

#define LZOHTABLE_CLEAR(_table)(lzohtable_clear_help(NULL, NULL, (_table)))

int lzohtable_put(void *key, size_t size, void *value, LZOHTable *table, uint64_t *out_hash);

int lzohtable_put_ck(void *key, size_t size, void *value, LZOHTable *table, uint64_t *out_hash);

int lzohtable_put_ckv(void *key, size_t key_size, void *value, size_t value_size, LZOHTable *table, uint64_t *out_hash);

void lzohtable_remove_help(void *key, size_t size, void *extra, void (*destroy)(void *key, void *value, void *extra), LZOHTable *table);

#define LZOHTABLE_REMOVE(_key, _size, _table)(lzohtable_remove_help((_key), (_size), NULL, NULL, _table))

#endif