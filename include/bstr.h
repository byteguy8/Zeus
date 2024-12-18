#ifndef _BSTR_H_
#define _BSTR_H_

#include <stddef.h>

typedef struct bstr
{
    size_t len;
    size_t used;         // without NULL character
    unsigned char *buff; // NULL terminated
} BStr;

struct bstr *bstr_create_empty();

struct bstr *bstr_create_append(char *raw_str);

void bstr_destroy(struct bstr *str);

int bstr_grow_by(size_t percentage, struct bstr *str);

struct bstr *bstr_substr(size_t start, size_t end, struct bstr *str);

unsigned char *bstr_raw_substr(size_t start, size_t end, struct bstr *str);

#define BSTR_CLONE(str)(bstr_create_append(str->buff))

#define BSTR_LEN(str) (str->used)
#define BSTR_CHAR_AT(index, str) (str->buffer + index)
#define BSTR_CODE(index, str) ((int)(str->buffer + index))

int bstr_append(char *raw_str, struct bstr *str);

int bstr_append_args(struct bstr *str, char *format, ...);

int bstr_insert(size_t at, char *raw_str, struct bstr *str);

int bstr_insert_args(size_t at, struct bstr *str, char *format, ...);

void bstr_remove(size_t start, size_t end, struct bstr *str);

#endif