#ifndef NATIVE_LIST
#define NATIVE_LIST

#include "essentials/memory.h"
#include "vmu.h"

static LZOHTable *list_symbols = NULL;

Value native_fn_list_size(uint8_t argsc, Value *values, Value target, void *context){
    ListObj *list_obj = VALUE_TO_LIST(target);
    return INT_VALUE(vmu_list_len(list_obj));
}

Value native_fn_list_clear(uint8_t argsc, Value *values, Value target, void *context){
    ListObj *list_obj = VALUE_TO_LIST(target);
    return INT_VALUE(vmu_list_clear(list_obj));
}

Value native_fn_list_to_array(uint8_t argsc, Value *values, Value target, void *context){
    ListObj *list_obj = VALUE_TO_LIST(target);
    DynArr *items = list_obj->items;
    size_t len = DYNARR_LEN(items);
    ArrayObj *array_obj = vmu_create_array(len, VMU_VM);

    for (size_t i = 0; i < len; i++){
        array_obj->values[i] = DYNARR_GET_AS(Value, i, items);
    }

    return OBJ_VALUE(array_obj);
}

Value native_fn_list_first(uint8_t argsc, Value *values, Value target, void *context){
    ListObj *list_obj = VALUE_TO_LIST(target);
    DynArr *items = list_obj->items;
    size_t len = DYNARR_LEN(items);

    if(len == 0){
        vmu_error(VMU_VM, "Failed to get fist list item: list is empty");
    }

    return DYNARR_GET_AS(Value, 0, items);
}

Value native_fn_list_last(uint8_t argsc, Value *values, Value target, void *context){
    ListObj *list_obj = VALUE_TO_LIST(target);
    DynArr *items = list_obj->items;
    size_t len = DYNARR_LEN(items);

    if(len == 0){
        vmu_error(VMU_VM, "Failed to get fist list item: list is empty");
    }

    return DYNARR_GET_AS(Value, len - 1, items);
}

Value native_fn_list_insert(uint8_t argsc, Value *values, Value target, void *context){
    ListObj *target_list_obj = VALUE_TO_LIST(target);
    Value value = values[0];

    vmu_list_insert(value, target_list_obj, VMU_VM);

    return value;
}

Value native_fn_list_insert_at(uint8_t argsc, Value *values, Value target, void *context){
    ListObj *target_list_obj = VALUE_TO_LIST(target);
    int64_t at = validate_value_int_arg(values[0], 1, "at", VMU_VM);
    Value value = values[1];

    vmu_list_insert_at(at, value, target_list_obj, VMU_VM);

    return value;
}

Value native_fn_list_remove(uint8_t argsc, Value *values, Value target, void *context){
    ListObj *target_list_obj = VALUE_TO_LIST(target);
    int64_t at = validate_value_int_arg(values[0], 1, "at", VMU_VM);
    return vmu_list_remove_at(at, target_list_obj, VMU_VM);
}

NativeFn *native_list_get(size_t key_size, const char *key, VM *vm){
    if(!list_symbols){
        Allocator *allocator = &vm->front_allocator;
        list_symbols = MEMORY_LZOHTABLE(allocator);

        dynarr_insert_ptr(list_symbols, vm->native_symbols);

        vm_factory_native_fn_add_info(list_symbols, allocator, "len", 0, native_fn_list_size);
        vm_factory_native_fn_add_info(list_symbols, allocator, "clear", 0, native_fn_list_clear);
        vm_factory_native_fn_add_info(list_symbols, allocator, "to_array", 0, native_fn_list_to_array);
        vm_factory_native_fn_add_info(list_symbols, allocator, "first", 0, native_fn_list_first);
        vm_factory_native_fn_add_info(list_symbols, allocator, "last", 0, native_fn_list_last);
        vm_factory_native_fn_add_info(list_symbols, allocator, "insert", 1, native_fn_list_insert);
        vm_factory_native_fn_add_info(list_symbols, allocator, "insert_at", 2, native_fn_list_insert_at);
        vm_factory_native_fn_add_info(list_symbols, allocator, "remove", 1, native_fn_list_remove);
    }

    NativeFn *native_fn = NULL;
    lzohtable_lookup(key_size, key, list_symbols, (void **)(&native_fn));

    return native_fn;
}

#endif