#ifndef TYPES_H
#define TYPES_H

#include "dynarr.h"
#include <stddef.h>

#define NAME_LEN 256

typedef struct value Value;
typedef struct vm VM;

typedef struct rawstr{
	size_t size;
	char *buff;
}RawStr;

typedef struct str{
	char core;
	size_t len;
	char *buff;
}Str;

typedef struct function{
    char *name;
    DynArr *chunks;
    DynArrPtr *params;
}Function;

typedef Value (*RawNativeFunction)(uint8_t argc, Value *values, void *target, VM *vm);

typedef struct native_function{
    char unique;
    int arity;
    size_t name_len;
    char name[NAME_LEN];
    void *target;
    RawNativeFunction native;
}NativeFunction;

#endif
