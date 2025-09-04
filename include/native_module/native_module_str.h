#ifndef NATIVE_STR
#define NATIVE_STR

#include "memory.h"
#include "factory.h"
#include "vmu.h"
#include "tutils.h"

static LZOHTable *str_symbols = NULL;

Value native_fn_str_len(uint8_t argsc, Value *values, Value *target, void *context){
	StrObj *target_str = VALUE_TO_STR(target);
	return INT_VALUE(vmu_str_len(target_str));
}

Value native_fn_str_code(uint8_t argsc, Value *values, Value *target, void *context){
	StrObj *target_str = VALUE_TO_STR(target);
    int64_t at = validate_value_int_arg(&values[0], 1, "at", VMU_VM);
    return INT_VALUE(vmu_str_code(at, target_str, VMU_VM));
}

Value native_fn_str_insert(uint8_t argsc, Value *values, Value *target, void *context){
    StrObj *target_str_obj = VALUE_TO_STR(target);
    int64_t at = validate_value_int_arg(&values[0], 1, "at", VMU_VM);
    StrObj *str_obj = validate_value_str_arg(&values[1], 1, "string", VMU_VM);
    StrObj *new_str_obj = vmu_str_insert_at(at, target_str_obj, str_obj, VMU_VM);

    return OBJ_VALUE(new_str_obj);
}

Value native_fn_str_remove(uint8_t argsc, Value *values, Value *target, void *context){
    StrObj *target_str_obj = VALUE_TO_STR(target);
    int64_t from = validate_value_int_arg(&values[0], 1, "from", VMU_VM);
    int64_t to = validate_value_int_arg(&values[1], 1, "to", VMU_VM);
    StrObj *new_str_obj = vmu_str_remove(from, to, target_str_obj, VMU_VM);

    return OBJ_VALUE(new_str_obj);
}

Value native_fn_str_remove_first(uint8_t argsc, Value *values, Value *target, void *context){
    StrObj *target_str_obj = VALUE_TO_STR(target);

    if(target_str_obj->len == 0){
        vmu_error(VMU_VM, "String is empty");
    }

    StrObj *new_str_obj = vmu_str_remove(0, 1, target_str_obj, VMU_VM);

    return OBJ_VALUE(new_str_obj);
}

Value native_fn_str_remove_last(uint8_t argsc, Value *values, Value *target, void *context){
    StrObj *target_str_obj = VALUE_TO_STR(target);
    size_t len = target_str_obj->len;

    if(target_str_obj->len == 0){
        vmu_error(VMU_VM, "String is empty");
    }

    StrObj *new_str_obj = vmu_str_remove(len - 1, len, target_str_obj, VMU_VM);

    return OBJ_VALUE(new_str_obj);
}

Value native_fn_str_substr(uint8_t argsc, Value *values, Value *target, void *context){
    StrObj *target_str_obj = VALUE_TO_STR(target);
    int64_t from = validate_value_int_arg(&values[0], 1, "from", VMU_VM);
    int64_t to = validate_value_int_arg(&values[1], 1, "to", VMU_VM);
    StrObj *new_str_obj = vmu_str_sub_str(from, to, target_str_obj, VMU_VM);

    return OBJ_VALUE(new_str_obj);
}

NativeFn *native_str_get(size_t key_size, const char *key, VM *vm){
    if(!str_symbols){
        Allocator *allocator = &vm->fake_allocator;
        str_symbols = FACTORY_LZOHTABLE(allocator);

        dynarr_insert_ptr(str_symbols, vm->native_symbols);

        factory_add_native_fn_info_n("len", 0, native_fn_str_len, str_symbols, allocator);
        factory_add_native_fn_info_n("code", 1, native_fn_str_code, str_symbols, allocator);
        factory_add_native_fn_info_n("insert", 2, native_fn_str_insert, str_symbols, allocator);
        factory_add_native_fn_info_n("remove", 2, native_fn_str_remove, str_symbols, allocator);
        factory_add_native_fn_info_n("remove_first", 0, native_fn_str_remove_first, str_symbols, allocator);
        factory_add_native_fn_info_n("remove_last", 0, native_fn_str_remove_last, str_symbols, allocator);
        factory_add_native_fn_info_n("substr", 2, native_fn_str_substr, str_symbols, allocator);
    }

    NativeFn *native_fn = NULL;
    lzohtable_lookup(key_size, key, str_symbols, (void **)(&native_fn));

    return native_fn;
}

#endif