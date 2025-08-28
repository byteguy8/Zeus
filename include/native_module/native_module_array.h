#ifndef NATIVE_ARRAY_H
#define NATIVE_ARRAY_H

#include "memory.h"
#include "factory.h"
#include "vmu.h"
#include "tutils.h"
#include <stdint.h>

static inline int64_t validate_int_rng_arg(uint8_t which, char *name, Value raw_value, int64_t from, int64_t to, VM *vm){
    if(!IS_VALUE_INT(&raw_value)){
        vmu_error(vm, "Illegal parameter %" PRIu8 " - '%s': expect 'INT'\n", which, name);
    }

    int64_t value = VALUE_TO_INT(&raw_value);

    if(value < from){
        vmu_error(vm, "Illegal parameter %" PRIu8 " - '%s': must be greater than " PRId64 "\n", value, from);
    }

    if(value >= to){
        vmu_error(vm, "Illegal parameter %" PRIu8 " - '%s': must less than " PRId64 "\n", value, to);
    }

    return value;
}

static LZOHTable *array_symbols = NULL;

Value native_fn_array_first(uint8_t argsc, Value *values, Value *target, void *context){
    ArrayObj *array_obj = VALUE_TO_ARRAY(target);
    return vmu_array_first(array_obj, VMU_VM);
}

Value native_fn_array_last(uint8_t argsc, Value *values, Value *target, void *context){
    ArrayObj *array_obj = VALUE_TO_ARRAY(target);
    return vmu_array_last(array_obj, VMU_VM);
}

Value native_fn_array_make_room(uint8_t argsc, Value *values, Value *target, void *context){
    ArrayObj *target_array_obj = VALUE_TO_ARRAY(target);
    Value *by_value = &values[0];

    VALIDATE_VALUE_INT_ARG(by_value, 1, "by", VMU_VM);
    int64_t by = VALUE_TO_INT(by_value);
    ArrayObj *new_array_obj = vmu_array_grow(by, target_array_obj, context);

    return OBJ_VALUE(new_array_obj);
}

Value native_fn_array_len(uint8_t argsc, Value *values, Value *target, void *context){
    ArrayObj *target_array_obj = VALUE_TO_ARRAY(target);
    return INT_VALUE(vmu_array_len(target_array_obj));
}

Value native_fn_array_join(uint8_t argsc, Value *values, Value *target, void *context){
    ArrayObj *target_array_obj = VALUE_TO_ARRAY(target);
    Value *array_value = &values[0];

    VALIDATE_VALUE_ARRAY_ARG(array_value, 1, "values", VMU_VM);
    ArrayObj *array_obj = VALUE_TO_ARRAY(array_value);
    ArrayObj *new_array_obj = vmu_array_join(target_array_obj, array_obj, context);

    return OBJ_VALUE(new_array_obj);
}

Value native_fn_array_to_list(uint8_t argsc, Value *values, Value *target, void *context){
    ArrayObj *target_array_obj = VALUE_TO_ARRAY(target);
    ListObj *list_obj = vmu_create_list(VMU_VM);
    size_t len = target_array_obj->len;
    Value *array_values = target_array_obj->values;

    for (size_t i = 0; i < len; i++){
        vmu_list_insert(array_values[i], list_obj, VMU_VM);
    }

    return OBJ_VALUE(list_obj);
}

NativeFn *native_array_get(size_t len, char *symbol, VM *vm){
    if(!array_symbols){
        Allocator *allocator = &vm->fake_allocator;
        array_symbols = FACTORY_LZOHTABLE(allocator);

        dynarr_insert_ptr(array_symbols, vm->native_symbols);

        factory_add_native_fn_info_n("len", 0, native_fn_array_len, array_symbols, allocator);
        factory_add_native_fn_info_n("grow", 1, native_fn_array_make_room, array_symbols, allocator);
        factory_add_native_fn_info_n("join", 1, native_fn_array_join, array_symbols, allocator);
        factory_add_native_fn_info_n("to_list", 0, native_fn_array_to_list, array_symbols, allocator);
        factory_add_native_fn_info_n("first", 0, native_fn_array_first, array_symbols, allocator);
        factory_add_native_fn_info_n("last", 0, native_fn_array_last, array_symbols, allocator);
    }

    NativeFn *native_fn = NULL;
    lzohtable_lookup(symbol, len, array_symbols, (void **)(&native_fn));

    return native_fn;
}

#endif