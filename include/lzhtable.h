// Humble implementation of a hash table

#ifndef _LZHTABLE_H_
#define _LZHTABLE_H_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct _lzhtable_allocator_
{
    void *(*alloc)(size_t byes, void *ctx);
    void *(*realloc)(void *ptr, size_t old_size, size_t new_size, void *ctx);
    void (*dealloc)(void *ptr, size_t size, void *ctx);
    void *ctx;
} LZHTableAllocator;

typedef struct _lzhtable_node_
{
    uint8_t *key;
    size_t key_size;
    void *value;

    struct _lzhtable_node_ *previous_table_node;
    struct _lzhtable_node_ *next_table_node;

    struct _lzhtable_node_ *previous_bucket_node;
    struct _lzhtable_node_ *next_bucket_node;
} LZHTableNode;

typedef struct _lzhtable_bucket_
{
    size_t size;
    struct _lzhtable_node_ *head;
    struct _lzhtable_node_ *tail;
} LZHTableBucket;

typedef struct _lzhtable_
{
    size_t m; // count of buckets available
    size_t n; // count of distinct elements in the table
    struct _lzhtable_bucket_ *buckets;
    struct _lzhtable_node_ *nodes;
    struct _lzhtable_allocator_ *allocator;
} LZHTable;

// interface
struct _lzhtable_ *lzhtable_create(size_t length, struct _lzhtable_allocator_ *allocator);
void lzhtable_destroy(void (*destroy_value)(void *value), struct _lzhtable_ *table);
struct _lzhtable_bucket_ *lzhtable_contains(uint8_t *key, size_t key_size, struct _lzhtable_ *table, struct _lzhtable_node_ **node_out);
void *lzhtable_get(uint8_t *key, size_t key_size, struct _lzhtable_ *table);
int lzhtable_put(uint8_t *key, size_t key_size, void *value, struct _lzhtable_ *table, uint32_t **hash_out);
int lzhtable_remove(uint8_t *key, size_t key_size, struct _lzhtable_ *table, void **value);
void lzhtable_clear(void (*clear_fn)(void *value), struct _lzhtable_ *table);

#define LZHTABLE_SIZE(table) (table->n)

#endif