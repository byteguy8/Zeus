#ifndef VM_H
#define VM_H

#include "value.h"
#include "obj.h"
#include "memory.h"
#include "types.h"
#include "fn.h"
#include "closure.h"
#include "dynarr.h"
#include "lzhtable.h"
#include <setjmp.h>

#define LOCALS_LENGTH 255
#define FRAME_LENGTH 255
#define STACK_LENGTH (LOCALS_LENGTH * FRAME_LENGTH)
#define MODULES_LENGTH 255
#define ALLOCATE_START_LIMIT MEMORY_MIBIBYTES(32)
#define GROW_ALLOCATE_LIMIT_FACTOR 2

typedef enum vm_result{
    OK_VMRESULT,
    ERR_VMRESULT,
}VMResult;

typedef enum global_value_access_type{
    PRIVATE_GLOVAL_VALUE_TYPE,
    PUBLIC_GLOBAL_VALUE_TYPE,
}GlobalValueAccessType;

typedef struct global_value{
    GlobalValueAccessType access;
    Value value;
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
    jmp_buf exit_jmp;
    unsigned char exit_code;

    Value *stack_top;
    Value stack[STACK_LENGTH];

    Frame *frame_ptr;
    Frame frame_stack[FRAME_LENGTH];

    LZOHTable *native_fns;
    LZOHTable *runtime_strs;
//> MODULE
    int module_ptr;
    Module *main_module;
    Module *modules[MODULES_LENGTH];
//< MODULE
//> GARBAGE COLLECTOR
    size_t allocated_bytes;
    size_t allocation_limit_size;
    ObjList while_objs;
    ObjList gray_objs;
    ObjList black_objs;
//< GARBAGE COLLECTOR
    DynArr *native_symbols;
//> MEMORY HANDLERS
    Allocator *allocator;
    Allocator fake_allocator;
//< MEMORY HANDLERS
}VM;

VM *vm_create(Allocator *allocator);
void vm_destroy(VM *vm);
void vm_initialize(VM *vm);
int vm_execute(LZOHTable *native_fns, Module *module, VM *vm);

#endif
