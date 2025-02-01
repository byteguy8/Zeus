#ifndef BSTR_H
#define BSTR_H

#include <stddef.h>

typedef struct bstr_allocator{
    void *ctx;
    void *(*alloc)(size_t size, void *ctx);
    void *(*realloc)(void *ptr, size_t new_size, size_t old_size, void *ctx);
    void (*dealloc)(void *ptr, size_t size, void *ctx);
}BStrAllocator;

typedef struct bstr{
    size_t len;
    size_t used;         // without NULL character
    unsigned char *buff; // NULL terminated
    struct bstr_allocator *allocator;
} BStr;

BStr *bstr_create_empty(BStrAllocator *allocator);
BStr *bstr_create_append(char *raw_str, BStrAllocator *allocator);
void bstr_destroy(BStr *str);

int bstr_grow_by(size_t percentage, BStr *str);
BStr *bstr_substr(size_t start, size_t end, BStr *str);
unsigned char *bstr_raw_substr(size_t start, size_t end, BStr *str);

#define BSTR_CLONE(str)(bstr_create_append(str->buff))

#define BSTR_LEN(str) (str->used)
#define BSTR_CHAR_AT(index, str) (str->buffer + index)
#define BSTR_CODE(index, str) ((int)(str->buffer + index))

int bstr_append(char *raw_str, BStr *str);
int bstr_append_args(BStr *str, char *format, ...);
int bstr_insert(size_t at, char *raw_str, BStr *str);
int bstr_insert_args(size_t at, BStr *str, char *format, ...);
void bstr_remove(size_t start, size_t end, BStr *str);

#endif