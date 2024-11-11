#ifndef LZMALLOC_H
#define LZMALLOC_H

#include <stddef.h>

typedef struct LZDynAllocNode_node{
	char used;
	size_t len;
	struct LZDynAllocNode_node *prev;
	struct LZDynAllocNode_node *next;
}LZDynAllocNode;

LZDynAllocNode *lzdynalloc_node_raw(size_t size, char *buff);
LZDynAllocNode *lzdynalloc_node_init(size_t size);
void lzdynalloc_node_deinit(LZDynAllocNode *node);
void *lzdynalloc_node_alloc(size_t size, LZDynAllocNode *node);
void *lzdynalloc_node_realloc(size_t size, void *ptr, LZDynAllocNode *node);
void lzdynalloc_node_dealloc(void *ptr, LZDynAllocNode *node);

#endif
