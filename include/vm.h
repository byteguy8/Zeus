#ifndef VM_H
#define VM_H

#include "lzpool.h"
#include "lzbstr.h"
#include "value.h"
#include "obj.h"
#include "memory.h"
#include "types.h"
#include "fn.h"
#include "closure.h"
#include "dynarr.h"
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


typedef struct template{
    LZBStr *str;
    struct template *prev;
}Template;

typedef struct exception{
    size_t catch_ip;
    Value throw_value;
    Value *stack_top;
    Frame *frame;
    struct exception *prev;
}Exception;

typedef struct vm{
    char halt;
    jmp_buf exit_jmp;
    unsigned char exit_code;
//-----------------------------  VALUE STACK  ------------------------------//
    Value *stack_top;
    Value stack[STACK_LENGTH];
//-----------------------------  FRAME STACK  ------------------------------//
    Frame *frame_ptr;
    Frame frame_stack[FRAME_LENGTH];
//--------------------------------  OTHER  ---------------------------------//
    LZOHTable *native_fns;
    DynArr *native_symbols;
    LZOHTable *runtime_strs;
    Template *templates;
    Exception *exception_stack;
//--------------------------------  MODULE  --------------------------------//
    int modules_stack_len;
    Module *modules_stack;
//--------------------------  GARBAGE COLLECTOR  ---------------------------//
    size_t allocated_bytes;
    size_t allocation_limit_size;
    ObjList white_objs;
    ObjList gray_objs;
    ObjList black_objs;
//--------------------------------  POOLS  ---------------------------------//
    LZPool str_objs_pool;
    LZPool array_objs_pool;
    LZPool list_objs_pool;
    LZPool dict_objs_pool;
    LZPool record_objs_pool;
    LZPool native_fn_objs_pool;
    LZPool fn_objs_pool;
    LZPool closures_pool;
    LZPool closure_objs_pool;
    LZPool native_module_objs_pool;
    LZPool module_objs_pool;
//------------------------------  ALLOCATORS  ------------------------------//
    Allocator *allocator;
    Allocator back_allocator;
    Allocator front_allocator;
}VM;

VM *vm_create(Allocator *allocator);
void vm_destroy(VM *vm);
void vm_initialize(VM *vm);
int vm_execute(LZOHTable *native_fns, Module *module, VM *vm);

#endif
