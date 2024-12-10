// Humble implementation of a hash table

#ifndef _LZHTABLE_H_
#define _LZHTABLE_H_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct lzhtable_allocator{
    void *(*alloc)(size_t byes, void *ctx);
    void *(*realloc)(void *ptr, size_t old_size, size_t new_size, void *ctx);
    void (*dealloc)(void *ptr, size_t size, void *ctx);
    void *ctx;
} LZHTableAllocator;

typedef struct lzhtable_node{
    void *key;
    void *value;
    uint32_t hash;

    struct lzhtable_node *previous_table_node;
    struct lzhtable_node *next_table_node;

    struct lzhtable_node *previous_bucket_node;
    struct lzhtable_node *next_bucket_node;
} LZHTableNode;

typedef struct lzhtable_bucket{
    size_t size;
    struct lzhtable_node *head;
    struct lzhtable_node *tail;
} LZHTableBucket;

typedef struct lzhtable{
    size_t m; // count of buckets available
    size_t n; // count of distinct elements in the table
    struct lzhtable_allocator *allocator;
    struct lzhtable_bucket *buckets;
    struct lzhtable_node *head;
    struct lzhtable_node *tail;
} LZHTable;

// interface
struct lzhtable *lzhtable_create(size_t length, struct lzhtable_allocator *allocator);
void lzhtable_destroy(void (*destroy_value)(void *key, void *value), struct lzhtable *table);

uint32_t lzhtable_hash(uint8_t *key, size_t key_size);

struct lzhtable_bucket *lzhtable_hash_contains(
    uint32_t hash,
    struct lzhtable *table,
    struct lzhtable_node **node_out
);
struct lzhtable_bucket *lzhtable_contains(
    uint8_t *key,
    size_t key_size,
    struct lzhtable *table,
    struct lzhtable_node **node_out
);

void *lzhtable_hash_get(uint32_t hash, struct lzhtable *table);
void *lzhtable_get(uint8_t *key, size_t key_size, struct lzhtable *table);

int lzhtable_hash_put_key(void *key, uint32_t hash, void *value, struct lzhtable *table);
int lzhtable_hash_put(uint32_t hash, void *value, struct lzhtable *table);
int lzhtable_put(
    uint8_t *key,
    size_t key_size,
    void *value,
    struct lzhtable *table,
    uint32_t **hash_out
);

int lzhtable_hash_remove(uint32_t hash, struct lzhtable *table, void **value);
int lzhtable_remove(uint8_t *key, size_t key_size, struct lzhtable *table, void **value);

void lzhtable_clear(void (*clear_fn)(void *value), struct lzhtable *table);

#define LZHTABLE_SIZE(table) (table->n)

#endif