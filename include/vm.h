#ifndef VM_H
#define VM_H

#include "value.h"
#include "dynarr.h"
#include "lzhtable.h"

#define STACK_LENGTH 255
#define LOCALS_LENGTH 255
#define FRAME_LENGTH 255

typedef struct frame{
    size_t ip;
    char *name;
    DynArr *chunks;
    Value locals[LOCALS_LENGTH];
}Frame;

typedef struct vm{
    int stack_ptr;
    Value stack[STACK_LENGTH];

    size_t frame_ptr;
    Frame frame_stack[FRAME_LENGTH];

    DynArr *constants;
	LZHTable *strings;
    DynArrPtr *functions;
    LZHTable *globals;
//> garbage collector
    Obj *head;
    Obj *tail;
//< garbage collector
}VM;

VM *vm_create();
void vm_print_stack(VM *vm);
int vm_execute(
    DynArr *constants,
    LZHTable *strings,
    DynArrPtr *functions,
    LZHTable *globals,
    VM *vm
);

#endif
