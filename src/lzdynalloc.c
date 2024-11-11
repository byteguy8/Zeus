#include "lzdynalloc.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define NODE_SIZE (sizeof(LZDynAllocNode))
#define AVAILABLE_LEN(node)(node->len - NODE_SIZE)
#define BUFF_END(node)(((char *)node) + NODE_SIZE + node->len)

LZDynAllocNode *lzdynalloc_node_raw(size_t size, char *buff){
	assert(size > NODE_SIZE && buff);

	LZDynAllocNode *node = (LZDynAllocNode *)buff;
	node->used = 0;
	node->len = size - NODE_SIZE;
	node->prev = NULL;
	node->next = NULL;

	return node;
}

LZDynAllocNode *lzdynalloc_node_init(size_t size){
	char *buff = malloc(size);
	return lzdynalloc_node_raw(size, buff);
}

void lzdynalloc_node_deinit(LZDynAllocNode *node){
	if(!node) return;
	free(node);
}

void *lzdynalloc_node_alloc(size_t size, LZDynAllocNode *node){
	LZDynAllocNode *current = node;
    size_t asize = NODE_SIZE + size;

	while(current){
		if(current->used || asize > current->len){
			current = current->next;
			continue;
		}

		char *buff = BUFF_END(current) - asize;
		LZDynAllocNode *next = lzdynalloc_node_raw(asize, buff);

        if(current->next){
            LZDynAllocNode *current_next = current->next;
            current_next->prev = next;
            next->next = current_next;
            next->prev = current;
        }else next->prev = current;

        next->used = 1;
        current->len -= asize;
        current->next = next;

		return buff + NODE_SIZE;
	}

	return NULL;
}

#define GET_NODE(raw_ptr)((LZDynAllocNode *)(((char *)ptr) - NODE_SIZE))

void free_node(void *ptr, LZDynAllocNode *node){
	LZDynAllocNode *current = (LZDynAllocNode *)(((char *)ptr) - NODE_SIZE);
    current->used = 0;

    LZDynAllocNode *prev = current->prev;
    LZDynAllocNode *next = current->next;

	if(next && !next->used){
		current->len += next->len + NODE_SIZE;
		current->next = next->next;
		if(next->next) next->next->prev = current;
	}

    if(prev && !prev->used){ 
        prev->len += current->len + NODE_SIZE;
        prev->next = current->next;
        if(current->next) current->next->prev = prev;
    }
}

void *lzdynalloc_node_realloc(size_t size, void *ptr, LZDynAllocNode *node){
	if(!ptr) return lzdynalloc_node_alloc(size, node);

	LZDynAllocNode *old_node = GET_NODE(ptr);
	void *new_ptr = lzdynalloc_node_alloc(size, node);

	if(!new_ptr) return NULL;
	memcpy(new_ptr, ptr, old_node->len);
	free_node(ptr, node);

	return new_ptr;
}

void lzdynalloc_node_dealloc(void *ptr, LZDynAllocNode *node){
    if(!ptr) return;
	free_node(ptr, node);
}
