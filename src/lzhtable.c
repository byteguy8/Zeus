#include "lzhtable.h"

// private interface
static void *_alloc_(size_t size, struct _lzhtable_allocator_ *allocator);
static void *_realloc_(void *ptr, size_t old_size, size_t new_size, struct _lzhtable_allocator_ *allocator);
static void _dealloc_(void *ptr, size_t size, struct _lzhtable_allocator_ *allocator);

static uint32_t jenkins_hash(const uint8_t *key, size_t length);
void lzhtable_node_destroy(struct _lzhtable_node_ *node, struct _lzhtable_ *table);
int lzhtable_compare(uint8_t *key, size_t key_size, struct _lzhtable_bucket_ *bucket, struct _lzhtable_node_ **out_node);
int lzhtable_bucket_insert(uint8_t *key, size_t key_size, void *value, struct _lzhtable_bucket_ *bucket, struct _lzhtable_allocator_ *allocator, struct _lzhtable_node_ **out_node);

// private implementation
void *_alloc_(size_t size, struct _lzhtable_allocator_ *allocator)
{
    return allocator ? allocator->alloc(size, allocator->ctx) : malloc(size);
}

void *_realloc_(void *ptr, size_t old_size, size_t new_size, struct _lzhtable_allocator_ *allocator)
{
    return allocator ? allocator->realloc(ptr, old_size, new_size, allocator->ctx) : realloc(ptr, new_size);
}

void _dealloc_(void *ptr, size_t size, struct _lzhtable_allocator_ *allocator)
{
    if (!ptr)
        return;

    if (allocator)
    {
        allocator->dealloc(ptr, size, allocator->ctx);
        return;
    }

    free(ptr);
}

uint32_t jenkins_hash(const uint8_t *key, size_t length)
{
    size_t i = 0;
    uint32_t hash = 0;

    while (i != length)
    {
        hash += key[i++];
        hash += hash << 10;
        hash ^= hash >> 6;
    }

    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;

    return hash;
}

void lzhtable_node_destroy(struct _lzhtable_node_ *node, struct _lzhtable_ *table)
{
    _dealloc_(node->key, node->key_size, table->allocator);

    node->key = NULL;
    node->key_size = 0;
    node->value = NULL;

    node->next_table_node = NULL;
    node->previous_table_node = NULL;

    node->next_bucket_node = NULL;
    node->previous_bucket_node = NULL;

    _dealloc_(node, sizeof(struct _lzhtable_node_), table->allocator);
}

int lzhtable_compare(uint8_t *key, size_t key_size, struct _lzhtable_bucket_ *bucket, struct _lzhtable_node_ **out_node)
{
    struct _lzhtable_node_ *node = bucket->head;

    while (node)
    {
        struct _lzhtable_node_ *next = node->next_bucket_node;

        if (node->key_size == key_size)
        {
            if (memcmp(key, node->key, key_size) == 0)
            {
                if (out_node)
                    *out_node = node;

                return 1;
            }
        }

        node = next;
    }

    return 0;
}

int lzhtable_bucket_insert(uint8_t *key, size_t key_size, void *value, struct _lzhtable_bucket_ *bucket, struct _lzhtable_allocator_ *allocator, struct _lzhtable_node_ **out_node)
{
    uint8_t *key_cpy = _alloc_(key_size, allocator);
    struct _lzhtable_node_ *node = _alloc_(sizeof(struct _lzhtable_node_), allocator);

    if (!key_cpy || !node)
    {
        _dealloc_(key_cpy, key_size, allocator);
        _dealloc_(node, sizeof(struct _lzhtable_node_), allocator);

        return 1;
    }

    memcpy(key_cpy, key, key_size);

    node->key = key_cpy;
    node->key_size = key_size;
    node->value = value;

    node->previous_table_node = NULL;
    node->next_table_node = NULL;

    node->previous_bucket_node = NULL;
    node->next_bucket_node = NULL;

    if (bucket->head)
    {
        node->previous_bucket_node = bucket->tail;
        bucket->tail->next_bucket_node = node;
    }
    else
        bucket->head = node;

    bucket->size++;
    bucket->tail = node;

    if (out_node)
        *out_node = node;

    return 0;
}

// public implementation
struct _lzhtable_ *lzhtable_create(size_t length, struct _lzhtable_allocator_ *allocator)
{
    size_t buckets_length = sizeof(struct _lzhtable_bucket_) * length;

    struct _lzhtable_bucket_ *buckets = (struct _lzhtable_bucket_ *)_alloc_(buckets_length, allocator);
    struct _lzhtable_ *table = (struct _lzhtable_ *)_alloc_(sizeof(struct _lzhtable_), allocator);

    if (!buckets || !table)
    {
        _dealloc_(buckets, buckets_length, allocator);
        _dealloc_(table, sizeof(struct _lzhtable_), allocator);

        return NULL;
    }

    memset(buckets, 0, buckets_length);

    table->m = length;
    table->n = 0;
    table->buckets = buckets;
    table->nodes = NULL;
    table->allocator = allocator;

    return table;
}

void lzhtable_destroy(void (*destroy_value)(void *value), struct _lzhtable_ *table)
{
    if (!table)
        return;

    struct _lzhtable_allocator_ *allocator = table->allocator;
    struct _lzhtable_node_ *node = table->nodes;

    while (node)
    {
        void *value = node->value;
        struct _lzhtable_node_ *previous = node->previous_table_node;

        if(destroy_value)
            destroy_value(value);
        
        lzhtable_node_destroy(node, table);

        node = previous;
    }

    _dealloc_(table->buckets, sizeof(struct _lzhtable_bucket_) * table->m, allocator);

    table->m = 0;
    table->n = 0;
    table->buckets = NULL;
    table->nodes = NULL;

    _dealloc_(table, sizeof(struct _lzhtable_), allocator);
}

struct _lzhtable_bucket_ *lzhtable_contains(uint8_t *key, size_t key_size, struct _lzhtable_ *table, struct _lzhtable_node_ **node_out)
{
    uint32_t k = jenkins_hash(key, key_size);
    size_t index = k % table->m;

    struct _lzhtable_bucket_ *bucket = &table->buckets[index];

    if (lzhtable_compare(key, key_size, bucket, node_out))
        return bucket;

    return NULL;
}

void *lzhtable_get(uint8_t *key, size_t key_size, struct _lzhtable_ *table)
{
    uint32_t k = jenkins_hash(key, key_size);
    size_t index = k % table->m;

    struct _lzhtable_bucket_ *bucket = &table->buckets[index];
    struct _lzhtable_node_ *node = NULL;

    if (lzhtable_compare(key, key_size, bucket, &node))
        return node->value;

    return NULL;
}

int lzhtable_put(uint8_t *key, size_t key_size, void *value, struct _lzhtable_ *table, uint32_t **hash_out)
{
    uint32_t k = jenkins_hash(key, key_size);
    size_t index = k % table->m;

    struct _lzhtable_bucket_ *bucket = &table->buckets[index];
    struct _lzhtable_node_ *node = NULL;

    if (bucket->size > 0 && lzhtable_compare(key, key_size, bucket, &node))
    {
        node->value = value;
        return 0;
    }

    if (lzhtable_bucket_insert(key, key_size, value, bucket, table->allocator, &node))
        return 1;

    if (hash_out)
        **hash_out = k;

    if (table->nodes)
    {
        table->nodes->next_table_node = node;
        node->previous_table_node = table->nodes;
    }

    table->n++;
    table->nodes = node;

    return 0;
}

int lzhtable_remove(uint8_t *key, size_t key_size, struct _lzhtable_ *table, void **value)
{
    uint32_t k = jenkins_hash(key, key_size);
    size_t index = k % table->m;

    struct _lzhtable_bucket_ *bucket = &table->buckets[index];

    if (bucket->size == 0)
        return 1;

    struct _lzhtable_node_ *node = NULL;

    if (lzhtable_compare(key, key_size, bucket, &node))
    {
        if (value)
            *value = node->value;

        if (node == table->nodes)
            table->nodes = node->previous_table_node;

        if (node->previous_table_node)
            node->previous_table_node->next_table_node = node->next_table_node;

        if (node->next_table_node)
            node->next_table_node->previous_table_node = node->previous_table_node;

        if (node->previous_bucket_node)
            node->previous_bucket_node->next_bucket_node = node->next_bucket_node;

        if (node->next_bucket_node)
            node->next_bucket_node->previous_bucket_node = node->previous_bucket_node;

        lzhtable_node_destroy(node, table);

        table->n--;

        return 0;
    }

    return 1;
}

void lzhtable_clear(void (*clear_fn)(void *value), struct _lzhtable_ *table)
{
    table->n = 0;
    table->nodes = NULL;

    memset(table->buckets, 0, sizeof(struct _lzhtable_bucket_) * table->m);

    struct _lzhtable_node_ *node = table->nodes;

    while (node)
    {
        struct _lzhtable_node_ *prev = node->previous_table_node;

        if (clear_fn)
            clear_fn(node->value);

        lzhtable_node_destroy(node, table);

        node = prev;
    }
}