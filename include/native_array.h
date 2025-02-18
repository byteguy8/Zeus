#ifndef NATIVE_ARRAY_H
#define NATIVE_ARRAY_H

#include "types.h"
#include "value.h"
#include "memory.h"
#include "vm_utils.h"

static LZHTable *array_symbols = NULL;

Value native_fn_array_length(uint8_t argsc, Value *values, void *target, VM *vm){
    Array *array = (Array *)target;
    return INT_VALUE((int64_t)array->len);
}

Value native_fn_array_join(uint8_t argsc, Value *values, void *target, VM *vm){
    Array *arr0 = (Array *)target;
    Value *arr1_value = &values[0];

    if(!IS_ARRAY(arr1_value)){
        vm_utils_error(vm, "Expect array at argument 1, but got something else");
    }

    Array *arr1 = TO_ARRAY(arr1_value);
    int64_t arr0_len = (int64_t)arr0->len;
    int64_t arr1_len = (int64_t)arr1->len;
    int64_t arr2_len = arr0_len + arr1_len;

    if(arr2_len < 0 || arr2_len > INT32_MAX){
        vm_utils_error(vm, "Illegal result array length. Must be 0 <= LENGTH(%ld) <= %d", arr2_len, INT32_MAX);
    }
    
    Obj *arr2_obj = vm_utils_array_obj(arr2_len, vm);
    Array *arr2 = arr2_obj->value.array;

    for(int32_t i = 0; i < arr2->len; i++){
        if(i < arr0->len){
            arr2->values[i] = arr0->values[i];
        }
        if(i < arr1->len){
            arr2->values[arr0->len + i] = arr1->values[i];
        }
    }
    
    return OBJ_VALUE(arr2_obj);
}

Value native_fn_array_to_list(uint8_t argsc, Value *values, void *target, VM *vm){
    Array *arr0 = (Array *)target;
    Obj *list_obj = vm_utils_list_obj(vm);

    if(!list_obj){
        vm_utils_error(vm, "Out of memory");
    }

    DynArr *list = list_obj->value.list;

    for(int32_t i = 0; i < arr0->len; i++){
        Value value = arr0->values[i];

        if(dynarr_insert(&value, list)){
            vm_utils_error(vm, "Out of memory");
        }
    }

    return OBJ_VALUE(list_obj);
}

Obj *native_array_get(char *symbol, void *target, VM *vm){
    if(!array_symbols){
        array_symbols = runtime_lzhtable();
        runtime_add_native_fn_info("length", 0, native_fn_array_length, array_symbols);
        runtime_add_native_fn_info("join", 1, native_fn_array_join, array_symbols);
        runtime_add_native_fn_info("to_list", 0, native_fn_array_to_list, array_symbols);
    }

    size_t key_size = strlen(symbol);
    NativeFnInfo *native_fn_info = (NativeFnInfo *)lzhtable_get((uint8_t *)symbol, key_size, array_symbols);
    
    if(native_fn_info){
        Obj *native_fn_obj = vm_utils_native_fn_obj(
            native_fn_info->arity,
            symbol,
            target,
            native_fn_info->raw_native,
            vm
        );

        if(!native_fn_obj){
            vm_utils_error(vm, "Out of memory");
        }

        return native_fn_obj;
    }

    return NULL;
}

#endif
