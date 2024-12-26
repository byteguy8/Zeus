#ifndef VM_UTILS_H
#define VM_UTILS_H

#include "value.h"
#include "vm.h"

void vm_utils_error(VM *vm, char *msg, ...);
int vm_utils_is_list(Value *value, DynArr **list);
int vm_utils_is_dict(Value *value, LZHTable **dict);
int vm_utils_is_record(Value *value, Record **record);
int vm_utils_is_function(Value *value, Fn **out_fn);
int vm_utils_is_native_function(Value *value, NativeFn **out_native_fn);
int vm_utils_is_module(Value *value, Module **out_module);

char *vm_utils_clone_buff(char *buff, VM *vm);
Value *vm_utils_clone_value(Value *value, VM *vm);
Obj *vm_utils_obj(ObjType type, VM *vm);
Str *vm_utils_core_str(char *buff, uint32_t hash, VM *vm);
Str *vm_utils_uncore_str(char *buff, VM *vm);
Str *vm_utils_uncore_alloc_str(char *buff, VM *vm);

Obj *vm_utils_core_str_obj(char *buff, VM *vm);
Obj *vm_utils_uncore_str_obj(char *buff, VM *vm);
Obj *vm_utils_empty_str_obj(Value *out_value, VM *vm);
Obj *vm_utils_clone_str_obj(char *buff, Value *out_value, VM *vm);
Obj *vm_utils_range_str_obj(size_t from, size_t to, char *buff, Value *out_value, VM *vm);

NativeFn *vm_utils_native_function(
    int arity,
    char *name,
    void *target,
    RawNativeFn native,
    VM *vm
);
DynArr *vm_utils_dyarr(VM *vm);
Obj *vm_utils_list_obj(VM *vm);
LZHTable *vm_utils_dict(VM *vm);
Obj *vm_utils_dict_obj(VM *vm);
Obj *vm_utils_record_obj(char empty, VM *vm);

void *assert_ptr(void *ptr, VM *vm);

#define IS_EMPTY(v)((v)->type == EMPTY_VTYPE)
#define IS_BOOL(v)((v)->type == BOOL_VTYPE)
#define IS_INT(v)((v)->type == INT_VTYPE)
#define IS_OBJ(v)((v)->type == OBJ_VTYPE)
#define IS_STR(v)((v)->type == OBJ_VTYPE && (v)->literal.obj->type == STR_OTYPE)
#define IS_RECORD(v)((v)->type == OBJ_VTYPE && value->literal.obj->type == RECORD_OTYPE)

#define TO_BOOL(v)((v)->literal.bool)
#define TO_INT(v)((v)->literal.i64)
#define TO_OBJ(v)((v)->literal.obj)
#define TO_STR(v)((v)->literal.obj->value.str)
#define TO_RECORD(v)((v)->literal.obj->value.record)

#define EMPTY_VALUE ((Value){.type = EMPTY_VTYPE})
#define INT_VALUE(value)((Value){.type = INT_VTYPE, .literal.i64 = value})
#define BOOL_VALUE(value)((Value){.type = BOOL_VTYPE, .literal.bool = value})
#define OBJ_VALUE(value)((Value){.type = OBJ_VTYPE, .literal.obj = value})

#define VALIDATE_INDEX(value, index, len) \
    if(!IS_INT((value))) \
         vm_utils_error(vm, "Unexpected index value. Expect int, but got something else"); \
    index = TO_INT((value)); \
    if(index < 0 || index >= (int64_t)len) \
        vm_utils_error(vm, "Index out of bounds. Must be 0 >= index(%ld) < len(%ld)", index, len);
    
#define VALIDATE_INDEX_NAME(value, index, len, name) \
    if(!IS_INT((value))) \
        vm_utils_error(vm, "Unexpected value for '%s'. Expect int, but got something else", name);\
    index = TO_INT((value)); \
    if(index < 0 || index >= (int64_t)len) \
        vm_utils_error(vm, "'%s' out of bounds. Must be 0 >= index(%ld) < len(%ld)", name, index, len);


uint32_t vm_utils_hash_obj(Obj *obj);
uint32_t vm_utils_hash_value(Value *value);

void vm_utils_clean_up(VM *vm);

#endif
