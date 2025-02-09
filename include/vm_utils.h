#ifndef VM_UTILS_H
#define VM_UTILS_H

#include "value.h"
#include "vm.h"
#include "bstr.h"

void vm_utils_error(VM *vm, char *msg, ...);
void *assert_ptr(void *ptr, VM *vm);

uint32_t vm_utils_hash_obj(Obj *obj);
uint32_t vm_utils_hash_value(Value *value);
int vm_utils_obj_to_str(Obj *obj, BStr *bstr);
int vm_utils_value_to_str(Value *value, BStr *bstr);

void vm_utils_clean_up(VM *vm);

char *vm_utils_clone_buff(char *buff, VM *vm);
Value *vm_utils_clone_value(Value *value, VM *vm);

Obj *vm_utils_obj(ObjType type, VM *vm);
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
Obj *vm_utils_array_obj(int16_t len, VM *vm);
Obj *vm_utils_list_obj(VM *vm);
Obj *vm_utils_dict_obj(VM *vm);
Obj *vm_utils_record_obj(char empty, VM *vm);
Obj *vm_utils_native_lib_obj(void *handler, VM *vm);

#define IS_EMPTY(v)((v)->type == EMPTY_VTYPE)
#define IS_BOOL(v)((v)->type == BOOL_VTYPE)
#define IS_INT(v)((v)->type == INT_VTYPE)
#define IS_FLOAT(v)((v)->type == FLOAT_VTYPE)
#define IS_OBJ(v)((v)->type == OBJ_VTYPE)
#define IS_STR(v)(IS_OBJ(v) && (v)->literal.obj->type == STR_OTYPE)
#define IS_ARRAY(v)(IS_OBJ(v) && (v)->literal.obj->type == ARRAY_OTYPE)
#define IS_LIST(v)(IS_OBJ(v) && (v)->literal.obj->type == LIST_OTYPE)
#define IS_DICT(v)(IS_OBJ(v) && (v)->literal.obj->type == DICT_OTYPE)
#define IS_RECORD(v)(IS_OBJ(v) && (v)->literal.obj->type == RECORD_OTYPE)
#define IS_FN(v)(IS_OBJ(v) && (v)->literal.obj->type == FN_OTYPE)
#define IS_NATIVE_FN(v)(IS_OBJ(v) && (v)->literal.obj->type == NATIVE_FN_OTYPE)
#define IS_NATIVE_MODULE(v)(IS_OBJ(v) && (v)->literal.obj->type == NATIVE_MODULE_OTYPE)
#define IS_MODULE(v)(IS_OBJ(v) && (v)->literal.obj->type == MODULE_OTYPE)
#define IS_NATIVE_LIBRARY(v)(IS_OBJ(v) && (v)->literal.obj->type == NATIVE_LIB_OTYPE)
#define IS_FOREIGN_FN(v)(IS_OBJ(v) && (v)->literal.obj->type == FOREIGN_FN_OTYPE)

#define TO_BOOL(v)((v)->literal.bool)
#define TO_INT(v)((v)->literal.i64)
#define TO_FLOAT(v)((v)->literal.fvalue)
#define TO_OBJ(v)((v)->literal.obj)
#define TO_STR(v)((v)->literal.obj->value.str)
#define TO_ARRAY(v)((v)->literal.obj->value.array)
#define TO_LIST(v)((v)->literal.obj->value.list)
#define TO_DICT(v)((v)->literal.obj->value.dict)
#define TO_RECORD(v)((v)->literal.obj->value.record)
#define TO_FN(v)((v)->literal.obj->value.fn)
#define TO_NATIVE_FN(v)((v)->literal.obj->value.native_fn)
#define TO_NATIVE_MODULE(v)((v)->literal.obj->value.native_module)
#define TO_MODULE(v)((v)->literal.obj->value.module)
#define TO_NATIVE_LIBRARY(v)((v)->literal.obj->value.native_lib)
#define TO_FOREIGN_FN(v)((v)->literal.obj->value.foreign_fn)

#define EMPTY_VALUE ((Value){.type = EMPTY_VTYPE})
#define BOOL_VALUE(value)((Value){.type = BOOL_VTYPE, .literal.bool = value})
#define INT_VALUE(value)((Value){.type = INT_VTYPE, .literal.i64 = value})
#define FLOAT_VALUE(value)((Value){.type = FLOAT_VTYPE, .literal.fvalue = value})
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

#endif
