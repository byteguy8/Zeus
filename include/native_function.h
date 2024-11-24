#ifndef NATIVE_FUNCTION_H
#define NATIVE_FUNCTION_H

#include <stddef.h>
#define NAME_LEN 256

typedef struct vm VM;

typedef struct native_function
{
    int arity;
    size_t name_len;
    char name[NAME_LEN];
    void *target;
    void(*native)(void *target, VM *vm);
}NativeFunction;

#endif