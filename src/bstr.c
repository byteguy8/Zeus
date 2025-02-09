#include "bstr.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#define BSTR_SIZE (sizeof(BStr))
#define BSTR_OUTOFSPACE(str_len, str) ((str)->used + (str_len) >= (str)->len)
#define BSTR_APPENDLEN(str_len, str) ((str)->len + (str_len))
#define BSTR_NEWLEN(str_len, str) (BSTR_APPENDLEN(str_len, str) + (BSTR_APPENDLEN(str_len, str) * 0.25) + 1)

static void *lzalloc(size_t size, BStrAllocator *allocator){
    return allocator ? allocator->alloc(size, allocator->ctx) : malloc(size);
}

static void *lzrealloc(void *ptr, size_t old_size, size_t new_size, BStrAllocator *allocator){
    return allocator ? allocator->realloc(ptr, old_size, new_size, allocator->ctx) : realloc(ptr, new_size);
}

static void lzdealloc(void *ptr, size_t size, BStrAllocator *allocator){
    if (!ptr) return;

    if (allocator) allocator->dealloc(ptr, size, allocator->ctx);
    else free(ptr);
}

static int grow(size_t new_len, BStr *str){
    unsigned char *buff = lzrealloc((void *)str->buff, str->len, new_len, str->allocator);

    if (!buff)
        return 1;

    str->len = new_len;
    str->buff = buff;

    return 0;
}

BStr *bstr_create_empty(BStrAllocator *allocator){
    BStr *str = (BStr *)lzalloc(BSTR_SIZE, allocator);

    if (!str) return NULL;

    str->len = 0;
    str->used = 0;
    str->buff = NULL;
    str->allocator = allocator;

    return str;
}

BStr *bstr_create_append(char *raw_str, BStrAllocator *allocator){
    BStr *str = bstr_create_empty(allocator);

    if (!raw_str || !str){
        bstr_destroy(str);
        return NULL;
    }

    if (bstr_append(raw_str, str)){
        bstr_destroy(str);
        return NULL;
    }

    return str;
}

void bstr_destroy(BStr *str){
    if (!str) return;

    BStrAllocator *allocator = str->allocator;

    if (str->buff){
        memset(str->buff, 0, str->len);
        lzdealloc(str->buff, str->len, allocator);
    }

    memset(str, 0, BSTR_SIZE);
    lzdealloc(str, BSTR_SIZE, allocator);
}

int bstr_grow_by(size_t percentage, BStr *str){
    size_t len = str->used == 0 ? percentage : str->used * (percentage / 100.0);
    return grow(str->len + len, str);
}

BStr *bstr_substr(size_t start, size_t end, BStr *str){
    char *buff = (char *)bstr_raw_substr(start, end, str);
    BStr *new_str = bstr_create_append(buff, str->allocator);

    if (!buff || !new_str){
        lzdealloc(buff, end - start + 2, str->allocator);
        bstr_destroy(new_str);
        return NULL;
    }

    memset(buff, 0, strlen(buff));
    lzdealloc(buff, end - start + 2, str->allocator);

    return new_str;
}

unsigned char *bstr_raw_substr(size_t start, size_t end, BStr *str){
    size_t len = end - start + 1;
    unsigned char *buff = (unsigned char *)lzalloc(len + 1, str->allocator);

    if(!buff) return NULL;

    memcpy(buff, str->buff + start, len);
    buff[len] = 0;

    return buff;
}

int bstr_append(char *raw_str, BStr *str){
    size_t str_len = strlen(raw_str);

    if (BSTR_OUTOFSPACE(str_len, str)){
        size_t new_len = BSTR_NEWLEN(str_len, str);
        if (grow(new_len, str)) return 1;
    }

    memcpy(str->buff + str->used, raw_str, str_len);

    str->used += str_len;
    str->buff[str->used] = 0;

    return 0;
}

int bstr_append_args(BStr *str, char *format, ...){
    va_list args;
    va_start(args, format);

    size_t str_len = vsnprintf(NULL, 0, format, args);

    if (BSTR_OUTOFSPACE(str_len, str)){
        size_t new_len = BSTR_NEWLEN(str_len, str);
        if (grow(new_len, str)) return 1;
    }

    va_start(args, format);

    // vsnprintf puts NULL character at end of buffer
    vsnprintf((char *)str->buff + str->used, str_len + 1, format, args);

    str->used += str_len;

    va_end(args);

    return 0;
}

int bstr_append_range(char *raw_str, size_t start, size_t end, BStr *str){
    assert(start <= end);
    size_t str_len = end - start + 1;

    if(BSTR_OUTOFSPACE(str_len, str)){
        size_t new_len = BSTR_NEWLEN(str_len, str);
        
        if(grow(new_len, str)){
            return 1;
        }
    }

    memcpy(str->buff + str->used, raw_str + start, str_len);
    
    str->used += str_len;
    str->buff[str->used] = 0;

    return 0;
}

int bstr_insert(size_t at, char *raw_str, BStr *str){
    size_t str_len = strlen(raw_str);

    if (BSTR_OUTOFSPACE(str_len, str)){
        size_t new_len = BSTR_NEWLEN(str_len, str);

        if (grow(new_len, str)) return 1;
    }

    size_t left_len = at;
    size_t right_len = str->used - left_len;

    // Making space for raw_str
    memmove(str->buff + at + str_len, str->buff + at, right_len);
    memcpy(str->buff + at, raw_str, str_len);

    str->used += str_len;
    str->buff[str->used] = 0;

    return 0;
}

int bstr_insert_args(size_t at, BStr *str, char *format, ...){
    va_list args;
    va_start(args, format);

    size_t str_len = vsnprintf(NULL, 0, format, args);

    if (BSTR_OUTOFSPACE(str_len, str)){
        size_t new_len = BSTR_NEWLEN(str_len, str);

        if (grow(new_len, str)) return 1;
    }

    size_t left_len = at;
    size_t right_len = str->used - left_len;

    // Making space for raw_str
    memmove(str->buff + at + str_len, str->buff + at, right_len);

    // Due to vsnprintf puts a \0 at end of buffer, we need to
    // saved the char it will replace with the \0 character.
    unsigned char replaced_char = *(str->buff + at + str_len);

    va_start(args, format);

    vsnprintf((char *)str->buff + at, str_len + 1, format, args);

    str->used += str_len;
    str->buff[str->used] = 0;
    str->buff[at + str_len] = replaced_char;

    va_end(args);

    return 0;
}

void bstr_remove(size_t start, size_t end, BStr *str){
    size_t len = end - start + 1;
    size_t right_len = str->used - end - 1;

    if (end + 1 < str->used)
        memmove(str->buff + start, str->buff + end + 1, right_len);

    str->used -= len;
    str->buff[str->used] = 0;
}