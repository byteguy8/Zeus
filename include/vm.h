#ifndef VM_H
#define VM_H

#include "dynarr.h"

#define STACK_SIZE 255

typedef enum value_type{
    BOOL_VTYPE,
    INT_VTYPE
}ValueType;

typedef struct value
{
    ValueType type;
    union{
        uint8_t bool;
        int64_t i64;
    }literal;
}Value;

typedef struct vm
{
    size_t ip;

    int stack_ptr;
    Value stack[STACK_SIZE];

    DynArr *constants;
    DynArr *chunks;
}VM;

VM *vm_create();
void vm_print_stack(VM *vm);
int vm_execute(DynArr *constants, DynArr *chunks, VM *vm);

#endif