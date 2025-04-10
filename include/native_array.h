#ifndef NATIVE_ARRAY_H
#define NATIVE_ARRAY_H

#include "rtypes.h"
#include "memory.h"
#include "factory.h"
#include "vmu.h"
#include <stdint.h>

static LZHTable *array_symbols = NULL;

Value native_fn_array_first(uint8_t argsc, Value *values, Value *target, VM *vm){
    Array *array = TO_ARRAY(target);
    if(array->len == 0){return EMPTY_VALUE;}
    return array->values[0];
}

Value native_fn_array_last(uint8_t argsc, Value *values, Value *target, VM *vm){
    Array *array = TO_ARRAY(target);
    if(array->len == 0){return EMPTY_VALUE;}
    return array->values[array->len - 1];
}

Value native_fn_array_make_room(uint8_t argsc, Value *values, Value *target, VM *vm){
    Array *array = TO_ARRAY(target);
    Value *plus_value = &values[0];

    if(!IS_INT(plus_value)){
        vmu_error(vm, "Expect integer at argument 1, but got something else");
    }

    int64_t plus = TO_INT(plus_value);

    if(plus < 0 || plus > INT32_MAX){
        vmu_error(vm, "Illegal array length. Must be 0 <= Argument 1(%ld) <= %d", plus, INT32_MAX);
    }

    Obj *new_array_obj = vmu_array_obj(array->len + plus, vm);
    Array *new_array = new_array_obj->content.array;

    if(!new_array_obj){
        vmu_error(vm, "Out of memory");
    }

    for(int32_t i = 0; i < array->len; i++){
        new_array->values[i] = array->values[i];
    }

    return OBJ_VALUE(new_array_obj);
}

Value native_fn_array_size(uint8_t argsc, Value *values, Value *target, VM *vm){
    Array *array = TO_ARRAY(target);
    return INT_VALUE((int64_t)array->len);
}

Value native_fn_array_join(uint8_t argsc, Value *values, Value *target, VM *vm){
    Array *array = TO_ARRAY(target);
    Value *arr1_value = &values[0];

    if(!IS_ARRAY(arr1_value)){
        vmu_error(vm, "Expect array at argument 1, but got something else");
    }

    Array *arr1 = TO_ARRAY(arr1_value);
    int64_t arr0_len = (int64_t)array->len;
    int64_t arr1_len = (int64_t)arr1->len;
    int64_t arr2_len = arr0_len + arr1_len;

    if(arr2_len < 0 || arr2_len > INT32_MAX){
        vmu_error(vm, "Illegal result array length. Must be 0 <= LENGTH(%ld) <= %d", arr2_len, INT32_MAX);
    }

    Obj *arr2_obj = vmu_array_obj(arr2_len, vm);
    Array *arr2 = arr2_obj->content.array;

    for(int32_t i = 0; i < arr2->len; i++){
        if(i < array->len){
            arr2->values[i] = array->values[i];
        }
        if(i < arr1->len){
            arr2->values[array->len + i] = arr1->values[i];
        }
    }

    return OBJ_VALUE(arr2_obj);
}

Value native_fn_array_to_list(uint8_t argsc, Value *values, Value *target, VM *vm){
    Array *array = TO_ARRAY(target);
    Obj *list_obj = vmu_list_obj(vm);
    DynArr *list = list_obj->content.list;

    for(int32_t i = 0; i < array->len; i++){
        Value value = array->values[i];
        dynarr_insert(&value, list);
    }

    return OBJ_VALUE(list_obj);
}

NativeFnInfo *native_array_get(char *symbol, VM *vm){
    if(!array_symbols){
        array_symbols = FACTORY_LZHTABLE(vm->rtallocator);
        factory_add_native_fn_info("first", 0, native_fn_array_first, array_symbols, vm->rtallocator);
        factory_add_native_fn_info("last", 0, native_fn_array_last, array_symbols, vm->rtallocator);
        factory_add_native_fn_info("make_room", 1, native_fn_array_make_room, array_symbols, vm->rtallocator);
        factory_add_native_fn_info("size", 0, native_fn_array_size, array_symbols, vm->rtallocator);
        factory_add_native_fn_info("join", 1, native_fn_array_join, array_symbols, vm->rtallocator);
        factory_add_native_fn_info("to_list", 0, native_fn_array_to_list, array_symbols, vm->rtallocator);
    }

    return (NativeFnInfo *)lzhtable_get((uint8_t *)symbol, strlen(symbol), array_symbols);
}

#endif
