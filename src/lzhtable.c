#include "lzhtable.h"
#include <stdio.h>

#define NODE_SIZE (sizeof(LZHTableNode))
#define BUCKET_SIZE (sizeof(LZHTableBucket))
#define TABLE_SIZE (sizeof(LZHTable))

// PRIVATE INTERFACE
static void *lzalloc(size_t size, LZHTableAllocator *allocator);
static void *lzrealloc(
    void *ptr,
    size_t old_size,
    size_t new_size,
    LZHTableAllocator *allocator
);
static void lzdealloc(
    void *ptr,
    size_t size,
    LZHTableAllocator *allocator
);
static uint32_t one_at_a_time_hash(const uint8_t *key, size_t length);
static uint32_t fnv_1a_hash(const uint8_t *key, size_t key_size);
static void destroy_node(LZHTableNode *node, LZHTable *table);
static int compare(uint32_t hash, LZHTableBucket *bucket, LZHTableNode **out_node);
static int bucket_insert(LZHTableBucket *bucket, LZHTableNode *node);
static int bucket_hash_insert(
    void *key,
    void *value,
    uint32_t hash,
    LZHTableBucket *bucket,
    LZHTableNode **out_node,
    LZHTableAllocator *allocator
);
static int bucket_key_insert(
    uint8_t *key, 
    size_t key_size, 
    void *value, 
    LZHTableBucket *bucket, 
    LZHTableAllocator *allocator, 
    LZHTableNode **out_node
);
static void relocate(LZHTable *table);
static int resize(LZHTable *table);
static void print_bucket(LZHTableBucket *bucket);

// PRIVATE IMPLEMENTATION
void *lzalloc(size_t size, LZHTableAllocator *allocator){
    return allocator ? allocator->alloc(size, allocator->ctx) : malloc(size);
}

void *lzrealloc(void *ptr, size_t old_size, size_t new_size, LZHTableAllocator *allocator){
    return allocator ? allocator->realloc(ptr, old_size, new_size, allocator->ctx) : realloc(ptr, new_size);
}

void lzdealloc(void *ptr, size_t size, LZHTableAllocator *allocator){
    if (!ptr){
        return;
    }

    if (allocator){
        allocator->dealloc(ptr, size, allocator->ctx);
        return;
    }

    free(ptr);
}

uint32_t one_at_a_time_hash(const uint8_t *key, size_t length){
    size_t i = 0;
    uint32_t hash = 0;

    while (i != length){
        hash += key[i++];
        hash += hash << 10;
        hash ^= hash >> 6;
    }

    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;

    return hash;
}

static uint32_t fnv_1a_hash(const uint8_t *key, size_t key_size){
    uint32_t prime = 0x01000193;
    uint32_t basis = 0x811c9dc5 ;
    uint32_t hash = basis;

    for (size_t i = 0; i < key_size; i++){
        hash ^= key[i];
        hash *= prime;   
    }
    
    return hash;
}

void destroy_node(LZHTableNode *node, LZHTable *table){
    memset(node, 0, NODE_SIZE);
    lzdealloc(node, NODE_SIZE, table->allocator);
}

int compare(uint32_t hash, LZHTableBucket *bucket, LZHTableNode **out_node){
    LZHTableNode *node = bucket->head;

    while (node){
        LZHTableNode *next = node->next_bucket_node;

        if (node->hash == hash){
            if (out_node){
                *out_node = node;
            }
            return 1;
        }

        node = next;
    }

    return 0;
}

int bucket_insert(LZHTableBucket *bucket, LZHTableNode *node){
    if (bucket->head){
        node->prev_bucket_node = bucket->tail;
        bucket->tail->next_bucket_node = node;
    }else{
        bucket->head = node;
    }

    bucket->size++;
    bucket->tail = node;

    return 0;
}

int bucket_hash_insert(
    void *key,
    void *value,
    uint32_t hash,
    LZHTableBucket *bucket,
    LZHTableNode **out_node,
    LZHTableAllocator *allocator
){
    LZHTableNode *node = lzalloc(NODE_SIZE, allocator);

    if (!node){
        return 1;
    }

    node->key = key;
    node->value = value;
    node->hash = hash;
    node->prev_table_node = NULL;
    node->next_table_node = NULL;
    node->prev_bucket_node = NULL;
    node->next_bucket_node = NULL;

    bucket_insert(bucket, node);

    if (out_node){
        *out_node = node;
    }

    return 0;
}

int bucket_key_insert(
    uint8_t *key, 
    size_t key_size, 
    void *value, 
    LZHTableBucket *bucket, 
    LZHTableAllocator *allocator, 
    LZHTableNode **out_node
){
    uint32_t hash = fnv_1a_hash(key, key_size);
    return bucket_hash_insert(key, value, hash, bucket, out_node, allocator);
}

void relocate(LZHTable *table){
    memset(table->buckets, 0, BUCKET_SIZE * table->m);
    
    LZHTableNode *current = table->head;
    
    for (;current;){
        size_t index = current->hash % table->m;
        LZHTableNode *next = current->next_table_node;
        LZHTableBucket *bucket = &table->buckets[index];
        
        current->next_bucket_node = NULL;
        current->prev_bucket_node = NULL;

        bucket_insert(bucket, current);
        
        current = next;
    }
}

int resize(LZHTable *table){
    size_t len = table->m;
    size_t new_len = len * 2;
    size_t old_size = BUCKET_SIZE * len;
    size_t new_size = BUCKET_SIZE * new_len;

    void *new_buckets = lzrealloc(
        table->buckets,
        old_size,
        new_size,
        table->allocator
    );

    if(!new_buckets){
        return 1;
    }

    table->m = new_len;
    table->buckets = new_buckets;

    relocate(table);

    return 0;
}

void print_bucket(LZHTableBucket *bucket){
    LZHTableNode *current = bucket->head;
    
    for (;current;){
        LZHTableNode *next = current->next_bucket_node;
        
        printf("%u", current->hash);
        
        if(next){
            printf(", ");
        }

        current = next;
    }
    
}

// PUBLIC IMPLEMENTATION
LZHTable *lzhtable_create(size_t length, LZHTableAllocator *allocator){
    size_t buckets_length = BUCKET_SIZE * length;

    LZHTableBucket *buckets = (LZHTableBucket *)lzalloc(buckets_length, allocator);
    LZHTable *table = (LZHTable *)lzalloc(TABLE_SIZE, allocator);

    if (!buckets || !table){
        lzdealloc(buckets, buckets_length, allocator);
        lzdealloc(table, TABLE_SIZE, allocator);

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

void lzhtable_destroy(void (*destroy_value)(void *key, void *value), LZHTable *table){
    if (!table) return;

    LZHTableAllocator *allocator = table->allocator;
    LZHTableNode *node = table->head;

    while (node){
        void *key = node->key;
        void *value = node->value;
        LZHTableNode *next = node->next_table_node;

        if(destroy_value)
            destroy_value(key, value);
        
        destroy_node(node, table);

        node = next;
    }

    lzdealloc(table->buckets, BUCKET_SIZE * table->m, allocator);
    memset(table, 0, TABLE_SIZE);
    lzdealloc(table, TABLE_SIZE, allocator);
}

void lzhtable_print(LZHTable *table){
    for (size_t i = 0; i < LZHTABLE_LENGTH(table); i++){
        LZHTableBucket *bucket = &table->buckets[i];
        
        printf("bucket %ld (%p): ", i + 1, bucket);
        print_bucket(bucket);
        printf("\n");
    }
}

uint32_t lzhtable_hash(uint8_t *key, size_t key_size){
    return fnv_1a_hash((const uint8_t *)key, key_size);
}

LZHTableBucket *lzhtable_hash_contains(uint32_t hash, LZHTable *table, LZHTableNode **node_out){
    size_t index = hash % table->m;
    LZHTableBucket *bucket = &table->buckets[index];

    if (compare(hash, bucket, node_out))
        return bucket;

    return NULL;
}

LZHTableBucket *lzhtable_contains(uint8_t *key, size_t key_size, LZHTable *table, LZHTableNode **node_out){
    uint32_t k = fnv_1a_hash(key, key_size);
    return lzhtable_hash_contains(k, table, node_out);
}

void *lzhtable_hash_get(uint32_t hash, LZHTable *table){
    size_t index = hash % table->m;

    LZHTableBucket *bucket = &table->buckets[index];
    LZHTableNode *node = NULL;

    if (compare(hash, bucket, &node))
        return node->value;

    return NULL;
}

void *lzhtable_get(uint8_t *key, size_t key_size, LZHTable *table){
    uint32_t k = fnv_1a_hash(key, key_size);
    return lzhtable_hash_get(k, table);
}

int lzhtable_hash_put_key(void *key, uint32_t hash, void *value, LZHTable *table){
    if(LZHTABLE_COUNT(table) > 0 && LZHTABLE_LOAD_FACTOR(table) >= 0.7 && resize(table)){
        return 1;
    }

    size_t index = hash % table->m;

    LZHTableBucket *bucket = &table->buckets[index];
    LZHTableNode *node = NULL;

    if (bucket->size > 0 && compare(hash, bucket, &node)){
        node->value = value;
        return 0;
    }

    if (bucket_hash_insert(key, value, hash, bucket, &node, table->allocator)){
        return 1;
    }

    if(table->tail){
        table->tail->next_table_node = node;
        node->prev_table_node = table->tail;
    }else{
        table->head = node;
    }

    table->n++;
    table->tail = node;

    return 0;
}

int lzhtable_hash_put(uint32_t hash, void *value, LZHTable *table){
    return lzhtable_hash_put_key(NULL, hash, value, table);
}

int lzhtable_put(uint8_t *key, size_t key_size, void *value, LZHTable *table, uint32_t **hash_out){
    uint32_t k = fnv_1a_hash(key, key_size);
    if(hash_out) **hash_out = k;
    return lzhtable_hash_put(k, value, table);
}

int lzhtable_hash_remove(uint32_t hash, LZHTable *table, void **value){
    size_t index = hash % table->m;
    LZHTableBucket *bucket = &table->buckets[index];

    if (bucket->size == 0)
        return 1;

    LZHTableNode *node = NULL;

    if (compare(hash, bucket, &node)){
        if (value)
            *value = node->value;

        if(node == table->head)
            table->head = node->next_table_node;
        if(node == table->tail)
            table->tail = node->prev_table_node;
        
        if(node == bucket->head)
            bucket->head = node->next_bucket_node;
        if(node == bucket->tail)
            bucket->tail = node->prev_bucket_node;

        if (node->prev_table_node)
            node->prev_table_node->next_table_node = node->next_table_node;
        if (node->next_table_node)
            node->next_table_node->prev_table_node = node->prev_table_node;

        if (node->prev_bucket_node)
            node->prev_bucket_node->next_bucket_node = node->next_bucket_node;
        if (node->next_bucket_node)
            node->next_bucket_node->prev_bucket_node = node->prev_bucket_node;

        destroy_node(node, table);

        table->n--;

        return 0;
    }

    return 1;
}

int lzhtable_remove(uint8_t *key, size_t key_size, LZHTable *table, void **value){
    uint32_t k = fnv_1a_hash(key, key_size);
    return lzhtable_hash_remove(k, table, value);
}

void lzhtable_clear(void (*clear_fn)(void *value), LZHTable *table){
    table->n = 0;
    table->head = NULL;
    table->tail = NULL;

    memset(table->buckets, 0, BUCKET_SIZE * table->m);

    LZHTableNode *node = table->head;

    while (node){
        LZHTableNode *prev = node->prev_table_node;

        if (clear_fn)
            clear_fn(node->value);

        destroy_node(node, table);

        node = prev;
    }
}