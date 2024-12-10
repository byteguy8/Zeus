#include "lzhtable.h"

#define NODE_SIZE (sizeof(struct lzhtable_node))
#define TABLE_SIZE (sizeof(struct lzhtable))

// private interface
static void *lzalloc(size_t size, struct lzhtable_allocator *allocator);
static void *lzrealloc(
    void *ptr,
    size_t old_size,
    size_t new_size,
    struct lzhtable_allocator *allocator
);
static void lzdealloc(
    void *ptr,
    size_t size,
    struct lzhtable_allocator *allocator
);

static uint32_t jenkins_hash(const uint8_t *key, size_t length);
void lzhtable_node_destroy(struct lzhtable_node *node, struct lzhtable *table);
int lzhtable_compare(uint32_t hash, struct lzhtable_bucket *bucket, struct lzhtable_node **out_node);
int lzhtable_bucket_hash_insert(
    void *key,
    void *value,
    uint32_t hash,
    struct lzhtable_bucket *bucket,
    struct lzhtable_node **out_node,
    struct lzhtable_allocator *allocator
);
int lzhtable_bucket_insert(
    uint8_t *key, 
    size_t key_size, 
    void *value, 
    struct lzhtable_bucket *bucket, 
    struct lzhtable_allocator *allocator, 
    struct lzhtable_node **out_node
);

// private implementation
void *lzalloc(size_t size, struct lzhtable_allocator *allocator){
    return allocator ? allocator->alloc(size, allocator->ctx) : malloc(size);
}

void *lzrealloc(void *ptr, size_t old_size, size_t new_size, struct lzhtable_allocator *allocator){
    return allocator ? allocator->realloc(ptr, old_size, new_size, allocator->ctx) : realloc(ptr, new_size);
}

void lzdealloc(void *ptr, size_t size, struct lzhtable_allocator *allocator){
    if (!ptr) return;

    if (allocator){
        allocator->dealloc(ptr, size, allocator->ctx);
        return;
    }

    free(ptr);
}

uint32_t jenkins_hash(const uint8_t *key, size_t length){
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

void lzhtable_node_destroy(struct lzhtable_node *node, struct lzhtable *table){
    memset(node, 0, NODE_SIZE);
    lzdealloc(node, NODE_SIZE, table->allocator);
}

int lzhtable_compare(uint32_t hash, struct lzhtable_bucket *bucket, struct lzhtable_node **out_node){
    struct lzhtable_node *node = bucket->head;

    while (node){
        struct lzhtable_node *next = node->next_bucket_node;

        if (node->hash == hash){
            if (out_node) *out_node = node;
            return 1;
        }

        node = next;
    }

    return 0;
}

int lzhtable_bucket_hash_insert(
    void *key,
    void *value,
    uint32_t hash,
    struct lzhtable_bucket *bucket,
    struct lzhtable_node **out_node,
    struct lzhtable_allocator *allocator
){
    struct lzhtable_node *node = lzalloc(NODE_SIZE, allocator);

    if (!node) return 1;

    node->key = key;
    node->value = value;
    node->hash = hash;
    node->previous_table_node = NULL;
    node->next_table_node = NULL;
    node->previous_bucket_node = NULL;
    node->next_bucket_node = NULL;

    if (bucket->head){
        node->previous_bucket_node = bucket->tail;
        bucket->tail->next_bucket_node = node;
    }
    else{
        bucket->head = node;
    }

    bucket->size++;
    bucket->tail = node;

    if (out_node)
        *out_node = node;

    return 0;
}

int lzhtable_bucket_insert(
    uint8_t *key, 
    size_t key_size, 
    void *value, 
    struct lzhtable_bucket *bucket, 
    struct lzhtable_allocator *allocator, 
    struct lzhtable_node **out_node
){
    uint32_t hash = jenkins_hash(key, key_size);
    return lzhtable_bucket_hash_insert(key, value, hash, bucket, out_node, allocator);
}

// public implementation
struct lzhtable *lzhtable_create(size_t length, struct lzhtable_allocator *allocator){
    size_t buckets_length = sizeof(struct lzhtable_bucket) * length;

    struct lzhtable_bucket *buckets = (struct lzhtable_bucket *)lzalloc(buckets_length, allocator);
    struct lzhtable *table = (struct lzhtable *)lzalloc(sizeof(struct lzhtable), allocator);

    if (!buckets || !table){
        lzdealloc(buckets, buckets_length, allocator);
        lzdealloc(table, sizeof(struct lzhtable), allocator);

        return NULL;
    }

    memset(buckets, 0, buckets_length);

    table->m = length;
    table->n = 0;
    table->allocator = allocator;
    table->buckets = buckets;
    table->head = NULL;
    table->tail = NULL;

    return table;
}

void lzhtable_destroy(void (*destroy_value)(void *key, void *value), struct lzhtable *table){
    if (!table) return;

    struct lzhtable_allocator *allocator = table->allocator;
    struct lzhtable_node *node = table->head;

    while (node){
        void *key = node->key;
        void *value = node->value;
        struct lzhtable_node *next = node->next_table_node;

        if(destroy_value)
            destroy_value(key, value);
        
        lzhtable_node_destroy(node, table);

        node = next;
    }

    lzdealloc(table->buckets, sizeof(struct lzhtable_bucket) * table->m, allocator);
    memset(table, 0, TABLE_SIZE);
    lzdealloc(table, sizeof(struct lzhtable), allocator);
}

uint32_t lzhtable_hash(uint8_t *key, size_t key_size){
    return jenkins_hash((const uint8_t *)key, key_size);
}

struct lzhtable_bucket *lzhtable_hash_contains(uint32_t hash, struct lzhtable *table, struct lzhtable_node **node_out){
    size_t index = hash % table->m;
    struct lzhtable_bucket *bucket = &table->buckets[index];

    if (lzhtable_compare(hash, bucket, node_out))
        return bucket;

    return NULL;
}

struct lzhtable_bucket *lzhtable_contains(uint8_t *key, size_t key_size, struct lzhtable *table, struct lzhtable_node **node_out){
    uint32_t k = jenkins_hash(key, key_size);
    return lzhtable_hash_contains(k, table, node_out);
}

void *lzhtable_hash_get(uint32_t hash, struct lzhtable *table){
    size_t index = hash % table->m;

    struct lzhtable_bucket *bucket = &table->buckets[index];
    struct lzhtable_node *node = NULL;

    if (lzhtable_compare(hash, bucket, &node))
        return node->value;

    return NULL;
}

void *lzhtable_get(uint8_t *key, size_t key_size, struct lzhtable *table){
    uint32_t k = jenkins_hash(key, key_size);
    return lzhtable_hash_get(k, table);
}

int lzhtable_hash_put_key(void *key, uint32_t hash, void *value, struct lzhtable *table){
    size_t index = hash % table->m;

    struct lzhtable_bucket *bucket = &table->buckets[index];
    struct lzhtable_node *node = NULL;

    if (bucket->size > 0 && lzhtable_compare(hash, bucket, &node)){
        node->value = value;
        return 0;
    }

    if (lzhtable_bucket_hash_insert(key, value, hash, bucket, &node, table->allocator))
        return 1;

    if(table->tail){
        table->tail->next_table_node = node;
        node->previous_table_node = table->tail;
    }else{
        table->head = node;
    }

    table->n++;
    table->tail = node;

    return 0;
}

int lzhtable_hash_put(uint32_t hash, void *value, struct lzhtable *table){
    return lzhtable_hash_put_key(NULL, hash, value, table);
}

int lzhtable_put(uint8_t *key, size_t key_size, void *value, struct lzhtable *table, uint32_t **hash_out){
    uint32_t k = jenkins_hash(key, key_size);
    if(hash_out) **hash_out = k;
    return lzhtable_hash_put(k, value, table);
}

int lzhtable_hash_remove(uint32_t hash, struct lzhtable *table, void **value){
    size_t index = hash % table->m;
    struct lzhtable_bucket *bucket = &table->buckets[index];

    if (bucket->size == 0)
        return 1;

    struct lzhtable_node *node = NULL;

    if (lzhtable_compare(hash, bucket, &node)){
        if (value)
            *value = node->value;

        if(node == table->head)
            table->head = node->next_table_node;
        if(node == table->tail)
            table->tail = node->previous_table_node;
        
        if(node == bucket->head)
            bucket->head = node->next_bucket_node;
        if(node == bucket->tail)
            bucket->tail = node->previous_bucket_node;

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

int lzhtable_remove(uint8_t *key, size_t key_size, struct lzhtable *table, void **value){
    uint32_t k = jenkins_hash(key, key_size);
    return lzhtable_hash_remove(k, table, value);
}

void lzhtable_clear(void (*clear_fn)(void *value), struct lzhtable *table){
    table->n = 0;
    table->head = NULL;
    table->tail = NULL;

    memset(table->buckets, 0, sizeof(struct lzhtable_bucket) * table->m);

    struct lzhtable_node *node = table->head;

    while (node){
        struct lzhtable_node *prev = node->previous_table_node;

        if (clear_fn)
            clear_fn(node->value);

        lzhtable_node_destroy(node, table);

        node = prev;
    }
}