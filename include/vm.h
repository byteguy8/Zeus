#ifndef VM_H
#define VM_H

#include "value.h"
#include "dynarr.h"
#include "lzhtable.h"
#include <setjmp.h>

#define STACK_LENGTH 255
#define LOCALS_LENGTH 255
#define FRAME_LENGTH 255

typedef enum global_value_access_type{
    PRIVATE_GVATYPE,
    PUBLIC_GVATYPE,
}GlobalValueAccessType;

typedef struct global_value{
    GlobalValueAccessType access;
    Value *value;
}GlobalValue;

typedef struct frame{
    size_t ip;
    size_t last_offset;
    
    Fn *fn;
    Closure *closure;
    
    Value locals[LOCALS_LENGTH];
    
    OutValue *values_head;
    OutValue *values_tail;
}Frame;

typedef struct vm{
    jmp_buf err_jmp;
    
    int stack_ptr;
    Value stack[STACK_LENGTH];

    int frame_ptr;
    Frame frame_stack[FRAME_LENGTH];

    LZHTable *natives;
    Module *module;
//> GARBAGE COLLECTOR
    Obj *head;
    Obj *tail;
    size_t objs_size;
//< GARBAGE COLLECTOR
}VM;

VM *vm_create();
void vm_print_stack(VM *vm);
int vm_execute(LZHTable *natives, Module *module, VM *vm);

#endif
