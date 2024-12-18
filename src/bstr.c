#include "bstr.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

int _bstr_grow(size_t new_len, struct bstr *str)
{
    unsigned char *buff = realloc((void *)str->buff, new_len);

    if (!buff)
        return 1;

    str->len = new_len;
    str->buff = buff;

    return 0;
}

struct bstr *bstr_create_empty()
{
    struct bstr *str = (struct bstr *)malloc(sizeof(struct bstr));

    if (!str)
        return NULL;

    str->len = 0;
    str->used = 0;
    str->buff = NULL;

    return str;
}

struct bstr *bstr_create_append(char *raw_str)
{
    if (!raw_str)
        return NULL;

    struct bstr *str = bstr_create_empty();

    if (!str)
        return NULL;

    if (bstr_append(raw_str, str))
    {
        bstr_destroy(str);
        return NULL;
    }

    return str;
}

void bstr_destroy(struct bstr *str)
{
    if (!str)
        return;

    if (str->buff)
    {
        memset(str->buff, 0, str->len);
        free(str->buff);
    }

    memset(str, 0, sizeof(struct bstr));
    free(str);
}

#define BSTR_OUTOFSPACE(str_len, str) (str->used + str_len >= str->len)
#define BSTR_APPENDLEN(str_len, str) (str->len + str_len)
#define BSTR_NEWLEN(str_len, str) (BSTR_APPENDLEN(str_len, str) + (BSTR_APPENDLEN(str_len, str) * 0.25) + 1)

int bstr_grow_by(size_t percentage, struct bstr *str)
{
    size_t len = str->used == 0 ? percentage : str->used * (percentage / 100.0);
    return _bstr_grow(str->len + len, str);
}

struct bstr *bstr_substr(size_t start, size_t end, struct bstr *str)
{
    char *buff = (char *)bstr_raw_substr(start, end, str);

    if (!buff)
        return NULL;

    struct bstr *new_str = bstr_create_append(buff);

    memset(buff, 0, strlen(buff));
    free(buff);

    return new_str;
}

unsigned char *bstr_raw_substr(size_t start, size_t end, struct bstr *str)
{
    size_t len = end - start + 1;
    unsigned char *buff = (unsigned char *)malloc(len + 1);

    memcpy(buff, str->buff + start, len);
    buff[len] = 0;

    return buff;
}

int bstr_append(char *raw_str, struct bstr *str)
{
    size_t str_len = strlen(raw_str);

    if (BSTR_OUTOFSPACE(str_len, str))
    {
        size_t new_len = BSTR_NEWLEN(str_len, str);

        if (_bstr_grow(new_len, str))
            return 1;
    }

    memcpy(str->buff + str->used, raw_str, str_len);

    str->used += str_len;
    str->buff[str->used] = 0;

    return 0;
}

int bstr_append_args(struct bstr *str, char *format, ...)
{
    va_list args;
    va_start(args, format);

    size_t str_len = vsnprintf(NULL, 0, format, args);

    if (BSTR_OUTOFSPACE(str_len, str))
    {
        size_t new_len = BSTR_NEWLEN(str_len, str);

        if (_bstr_grow(new_len, str))
            return 1;
    }

    va_start(args, format);

    // vsnprintf puts NULL character at end of buffer
    vsnprintf((char *)str->buff + str->used, str_len + 1, format, args);

    str->used += str_len;

    va_end(args);

    return 0;
}

int bstr_insert(size_t at, char *raw_str, struct bstr *str)
{
    size_t str_len = strlen(raw_str);

    if (BSTR_OUTOFSPACE(str_len, str))
    {
        size_t new_len = BSTR_NEWLEN(str_len, str);

        if (_bstr_grow(new_len, str))
            return 1;
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

int bstr_insert_args(size_t at, struct bstr *str, char *format, ...)
{
    va_list args;
    va_start(args, format);

    size_t str_len = vsnprintf(NULL, 0, format, args);

    if (BSTR_OUTOFSPACE(str_len, str))
    {
        size_t new_len = BSTR_NEWLEN(str_len, str);

        if (_bstr_grow(new_len, str))
            return 1;
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

void bstr_remove(size_t start, size_t end, struct bstr *str)
{
    size_t len = end - start + 1;
    size_t right_len = str->used - end - 1;

    if (end + 1 < str->used)
        memmove(str->buff + start, str->buff + end + 1, right_len);

    str->used -= len;
    str->buff[str->used] = 0;
}