#ifndef NATIVE_ARRAY_H
#define NATIVE_ARRAY_H

#include "memory.h"
#include "factory.h"
#include "vmu.h"
#include "types_utils.h"
#include <stdint.h>

static LZOHTable *array_symbols = NULL;

Value native_fn_array_len(uint8_t argsc, Value *values, Value *target, void *context){
    ArrayObj *target_array_obj = VALUE_TO_ARRAY(*target);
    return INT_VALUE(vmu_array_len(target_array_obj));
}

Value native_fn_array_make_room(uint8_t argsc, Value *values, Value *target, void *context){
    ArrayObj *target_array_obj = VALUE_TO_ARRAY(*target);
    int64_t by = validate_value_int_arg(values[0], 1, "by", VMU_VM);
    ArrayObj *new_array_obj = vmu_array_grow(by, target_array_obj, context);

    return OBJ_VALUE(new_array_obj);
}

Value native_fn_array_to_list(uint8_t argsc, Value *values, Value *target, void *context){
    ArrayObj *target_array_obj = VALUE_TO_ARRAY(*target);
    ListObj *list_obj = vmu_create_list(VMU_VM);
    size_t len = target_array_obj->len;
    Value *array_values = target_array_obj->values;

    for (size_t i = 0; i < len; i++){
        vmu_list_insert(array_values[i], list_obj, VMU_VM);
    }

    return OBJ_VALUE(list_obj);
}

Value native_fn_array_first(uint8_t argsc, Value *values, Value *target, void *context){
    ArrayObj *array_obj = VALUE_TO_ARRAY(*target);
    return vmu_array_first(array_obj, VMU_VM);
}

Value native_fn_array_last(uint8_t argsc, Value *values, Value *target, void *context){
    ArrayObj *array_obj = VALUE_TO_ARRAY(*target);
    return vmu_array_last(array_obj, VMU_VM);
}

NativeFn *native_array_get(size_t key_size, const char *key, VM *vm){
    if(!array_symbols){
        Allocator *allocator = &vm->front_allocator;
        array_symbols = FACTORY_LZOHTABLE(allocator);

        dynarr_insert_ptr(array_symbols, vm->native_symbols);

        factory_add_native_fn_info_n("len", 0, native_fn_array_len, array_symbols, allocator);
        factory_add_native_fn_info_n("grow", 1, native_fn_array_make_room, array_symbols, allocator);
        factory_add_native_fn_info_n("to_list", 0, native_fn_array_to_list, array_symbols, allocator);
        factory_add_native_fn_info_n("first", 0, native_fn_array_first, array_symbols, allocator);
        factory_add_native_fn_info_n("last", 0, native_fn_array_last, array_symbols, allocator);
    }

    NativeFn *native_fn = NULL;
    lzohtable_lookup(key_size, key, array_symbols, (void **)(&native_fn));

    return native_fn;
}

#endif