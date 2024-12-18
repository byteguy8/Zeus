#ifndef VM_H
#define VM_H

#include "value.h"
#include "dynarr.h"
#include "lzhtable.h"
#include <setjmp.h>

#define STACK_LENGTH 255
#define LOCALS_LENGTH 255
#define FRAME_LENGTH 255

typedef struct frame{
    size_t ip;
    size_t last_offset;
    Fn *fn;
    Value locals[LOCALS_LENGTH];
}Frame;

typedef struct vm{
    jmp_buf err_jmp;
    
    int stack_ptr;
    Value stack[STACK_LENGTH];

    int frame_ptr;
    Frame frame_stack[FRAME_LENGTH];

    LZHTable *natives;
    Module *module;
//> garbage collector
    Obj *head;
    Obj *tail;
    size_t objs_size;
//< garbage collector
}VM;

VM *vm_create();
void vm_print_stack(VM *vm);
int vm_execute(LZHTable *natives, Module *module, VM *vm);

#endif