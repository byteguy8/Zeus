#ifndef NATIVE_ARRAY_H
#define NATIVE_ARRAY_H

#include "memory.h"
#include "factory.h"
#include "vmu.h"
#include "tutils.h"
#include <stdint.h>

static LZHTable *array_symbols = NULL;

Value native_fn_array_first(uint8_t argsc, Value *values, Value *target, void *context){
    ArrayObj *array_obj = VALUE_TO_ARRAY(target);

    if(array_obj->len == 0){
        return EMPTY_VALUE;
    }

    return array_obj->values[0];
}

Value native_fn_array_last(uint8_t argsc, Value *values, Value *target, void *context){
    ArrayObj *array_obj = VALUE_TO_ARRAY(target);

    if(array_obj->len == 0){
        return EMPTY_VALUE;
    }

    return array_obj->values[array_obj->len - 1];
}

Value native_fn_array_make_room(uint8_t argsc, Value *values, Value *target, void *context){
    ArrayObj *array_obj = VALUE_TO_ARRAY(target);
    Value *plus_value = &values[0];

    VALIDATE_VALUE_INT_ARG(plus_value, 1, "plus", context);

    int64_t plus = VALUE_TO_INT(plus_value);
    VALIDATE_ARRAY_SIZE(plus, context)

    int new_size = array_obj->len + plus;
    VALIDATE_ARRAY_SIZE(new_size, context)

    ObjHeader *new_array_header = vmu_create_array_obj(array_obj->len + plus, context);
    ArrayObj *new_array_obj = OBJ_TO_ARRAY(new_array_header);

    memcpy(new_array_obj->values, array_obj->values, VALUE_SIZE * array_obj->len);

    return OBJ_VALUE(new_array_header);
}

Value native_fn_array_size(uint8_t argsc, Value *values, Value *target, void *context){
    ArrayObj *array_obj = VALUE_TO_ARRAY(target);
    return INT_VALUE((int64_t)array_obj->len);
}

Value native_fn_array_join(uint8_t argsc, Value *values, Value *target, void *context){
    ArrayObj *arr_a_obj = VALUE_TO_ARRAY(target);
    Value *arr_b_value = &values[0];

    VALIDATE_VALUE_ARRAY_ARG(arr_b_value, 1, "array b", context)

    ArrayObj *arr_b_obj = VALUE_TO_ARRAY(arr_b_value);
    int64_t arr_a_size = (int64_t)arr_a_obj->len;
    int64_t arr_b_size = (int64_t)arr_b_obj->len;
    int64_t arr_c_size = arr_a_size + arr_b_size;

    VALIDATE_ARRAY_SIZE(arr_c_size, context)

    ObjHeader *arr_c_header = vmu_create_array_obj(arr_c_size, context);
    ArrayObj *arr_c_obj = OBJ_TO_ARRAY(arr_c_header);

    memcpy(arr_c_obj->values, arr_a_obj->values, VALUE_SIZE * arr_a_obj->len);
    memcpy(arr_c_obj->values + arr_a_obj->len, arr_b_obj->values, VALUE_SIZE * arr_b_obj->len);

    return OBJ_VALUE(arr_c_header);
}

Value native_fn_array_to_list(uint8_t argsc, Value *values, Value *target, void *context){
    ArrayObj *array_obj = VALUE_TO_ARRAY(target);
    ObjHeader *list_header = vmu_create_list_obj(context);
    ListObj *list_obj = OBJ_TO_LIST(list_header);
    DynArr *list = list_obj->list;

    for(aidx_t i = 0; i < array_obj->len; i++){
        Value value = array_obj->values[i];
        dynarr_insert(&value, list);
    }

    return OBJ_VALUE(list_header);
}

NativeFnInfo *native_array_get(char *symbol, Allocator *allocator){
    if(!array_symbols){
        array_symbols = FACTORY_LZHTABLE(allocator);

        factory_add_native_fn_info("first", 0, native_fn_array_first, array_symbols, allocator);
        factory_add_native_fn_info("last", 0, native_fn_array_last, array_symbols, allocator);
        factory_add_native_fn_info("make_room", 1, native_fn_array_make_room, array_symbols, allocator);
        factory_add_native_fn_info("size", 0, native_fn_array_size, array_symbols, allocator);
        factory_add_native_fn_info("join", 1, native_fn_array_join, array_symbols, allocator);
        factory_add_native_fn_info("to_list", 0, native_fn_array_to_list, array_symbols, allocator);
    }

    return (NativeFnInfo *)lzhtable_get((uint8_t *)symbol, strlen(symbol), array_symbols);
}

#endif