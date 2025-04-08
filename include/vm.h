#ifndef VM_H
#define VM_H

#include "types.h"
#include "rtypes.h"
#include "dynarr.h"
#include "lzhtable.h"
#include <setjmp.h>

#define LOCALS_LENGTH 255
#define FRAME_LENGTH 255
#define STACK_LENGTH (LOCALS_LENGTH * FRAME_LENGTH)
#define MODULES_LENGTH 255

typedef enum vm_result{
    OK_VMRESULT,
    ERR_VMRESULT,
}VMResult;

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

    Value *locals;

    OutValue *outs_head;
    OutValue *outs_tail;
}Frame;

typedef struct vm{
    char halt;
    jmp_buf err_jmp;
    unsigned char exit_code;

    Value *stack_top;
    Value stack[STACK_LENGTH];

    int frame_ptr;
    Frame frame_stack[FRAME_LENGTH];

    LZHTable *natives;
    //Module *module;
    int module_ptr;
    Module *modules[MODULES_LENGTH];
//> GARBAGE COLLECTOR
    size_t objs_size;
    Obj *red_head;
    Obj *red_tail;

    Obj *blue_head;
    Obj *blue_tail;
//< GARBAGE COLLECTOR
    Allocator *rtallocator;
}VM;

VM *vm_create(Allocator *allocator);
int vm_execute(LZHTable *natives, Module *module, VM *vm);

#endif