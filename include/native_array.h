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

    VALIDATE_VALUE_INT_ARG(plus_value, 1, "plus", vm);

    int64_t plus = TO_INT(plus_value);
    VALIDATE_ARRAY_SIZE(plus)

    int new_size = array->len + plus;
    VALIDATE_ARRAY_SIZE(new_size)

    Obj *new_array_obj = vmu_array_obj(array->len + plus, vm);
    Array *new_array = new_array_obj->content.array;

    for(aidx_t i = 0; i < array->len; i++){
        new_array->values[i] = array->values[i];
    }

    return OBJ_VALUE(new_array_obj);
}

Value native_fn_array_size(uint8_t argsc, Value *values, Value *target, VM *vm){
    Array *array = TO_ARRAY(target);
    return INT_VALUE((int64_t)array->len);
}

Value native_fn_array_join(uint8_t argsc, Value *values, Value *target, VM *vm){
    Array *arr_a = TO_ARRAY(target);
    Value *arr_b_value = &values[0];

    VALIDATE_VALUE_ARRAY_ARG(arr_b_value, 1, "array b", vm)

    Array *arr_b = TO_ARRAY(arr_b_value);
    int64_t arr_a_size = (int64_t)arr_a->len;
    int64_t arr_b_size = (int64_t)arr_b->len;
    int64_t arr_c_size = arr_a_size + arr_b_size;

    VALIDATE_ARRAY_SIZE(arr_c_size)

    Obj *arr_c_obj = vmu_array_obj(arr_c_size, vm);
    Array *arr_c = arr_c_obj->content.array;

    for(aidx_t i = 0; i < arr_c->len; i++){
        if(i < arr_a->len){
            arr_c->values[i] = arr_a->values[i];
        }
        if(i < arr_b->len){
            arr_c->values[arr_a->len + i] = arr_b->values[i];
        }
    }

    return OBJ_VALUE(arr_c_obj);
}

Value native_fn_array_to_list(uint8_t argsc, Value *values, Value *target, VM *vm){
    Array *array = TO_ARRAY(target);
    Obj *list_obj = vmu_list_obj(vm);
    DynArr *list = list_obj->content.list;

    for(lidx_t i = 0; i < array->len; i++){
        Value value = array->values[i];
        dynarr_insert(&value, list);
    }

    return OBJ_VALUE(list_obj);
}

NativeFnInfo *native_array_get(char *symbol, VM *vm){
    if(!array_symbols){
        array_symbols = FACTORY_LZHTABLE(vm->fake_allocator);
        factory_add_native_fn_info("first", 0, native_fn_array_first, array_symbols, vm->fake_allocator);
        factory_add_native_fn_info("last", 0, native_fn_array_last, array_symbols, vm->fake_allocator);
        factory_add_native_fn_info("make_room", 1, native_fn_array_make_room, array_symbols, vm->fake_allocator);
        factory_add_native_fn_info("size", 0, native_fn_array_size, array_symbols, vm->fake_allocator);
        factory_add_native_fn_info("join", 1, native_fn_array_join, array_symbols, vm->fake_allocator);
        factory_add_native_fn_info("to_list", 0, native_fn_array_to_list, array_symbols, vm->fake_allocator);
    }

    return (NativeFnInfo *)lzhtable_get((uint8_t *)symbol, strlen(symbol), array_symbols);
}

#endif
