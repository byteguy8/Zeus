#ifndef TYPES_H
#define TYPES_H

#include <stddef.h>

typedef struct rawstr{
	size_t size;
	char *buff;
}RawStr;

typedef struct allocator{
    void *ctx;
    void *(*alloc)(size_t size, void *ctx);
    void *(*realloc)(void *ptr, size_t old_size, size_t new_size, void *ctx);
    void (*dealloc)(void *ptr, size_t size, void *ctx);
    void *extra;
}Allocator;

#endif