#ifndef NATIVE_LIST
#define NATIVE_LIST

#include "rtypes.h"
#include "memory.h"
#include "vmu.h"

static LZHTable *list_symbols = NULL;

Value native_fn_list_size(uint8_t argsc, Value *values, Value *target, VM *vm){
    DynArr *list = TO_LIST(target);
    return INT_VALUE((int64_t)DYNARR_LEN(list));
}

Value native_fn_list_capacity(uint8_t argsc, Value *values, Value *target, VM *vm){
    DynArr *list = TO_LIST(target);
    return INT_VALUE((int64_t)(list->count));
}

Value native_fn_list_available(uint8_t argsc, Value *values, Value *target, VM *vm){
    DynArr *list = TO_LIST(target);
    return INT_VALUE((int64_t)(DYNARR_AVAILABLE(list)));
}

Value native_fn_list_first(uint8_t argsc, Value *values, Value *target, VM *vm){
    DynArr *list = TO_LIST(target);

    if(DYNARR_LEN(list) == 0){
        return EMPTY_VALUE;
    }else{
        return DYNARR_GET_AS(Value, 0, list);
    }
}

Value native_fn_list_last(uint8_t argsc, Value *values, Value *target, VM *vm){
    DynArr *list = TO_LIST(target);

    if(DYNARR_LEN(list) == 0){
        return EMPTY_VALUE;
    }else{
        return DYNARR_GET_AS(Value, DYNARR_LEN(list) - 1, list);
    }
}

Value native_fn_list_reverse(uint8_t argsc, Value *values, Value *target, VM *vm){
    DynArr *list = TO_LIST(target);
    dynarr_reverse(list);
    return EMPTY_VALUE;
}

Value native_fn_list_get(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *index_value = &values[0];
    DynArr *list = TO_LIST(target);

    VALIDATE_LIST_INDEX(DYNARR_LEN(list), index_value, vm);

    int64_t index = TO_INT(index_value);

    return DYNARR_GET_AS(Value, (size_t)index, list);
}

Value native_fn_list_insert(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *value = &values[0];
    DynArr *list = TO_LIST(target);

    dynarr_insert(value, list);

    return EMPTY_VALUE;
}

Value native_fn_list_insert_at(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *index_value = &values[0];
    Value *element_value = &values[1];
    DynArr *list = TO_LIST(target);

    VALIDATE_LIST_INDEX(DYNARR_LEN(list), index_value, vm)

    int64_t index = TO_INT(index_value);
    Value old_value = DYNARR_GET_AS(Value, (size_t)index, list);

    dynarr_insert_at((size_t) index, element_value, list);

    return old_value;
}

Value native_fn_list_set(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *index_value = &values[0];
    Value *value = &values[1];
    DynArr *list = TO_LIST(target);

    VALIDATE_LIST_INDEX(DYNARR_LEN(list), index_value, vm)

    int64_t index = TO_INT(index_value);
    Value out_value = DYNARR_GET_AS(Value, (size_t)index, list);

    DYNARR_SET(value, (size_t)index, list);

    return out_value;
}

Value native_fn_list_append(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *from_value = &values[0];
    DynArr *to_list = TO_LIST(target);

    VALIDATE_VALUE_LIST_ARG(from_value, 1, "from list", vm)

	DynArr *from_list = TO_LIST(from_value);
    int64_t from_len = (int64_t)from_list->used;

    dynarr_append(from_list, to_list);

    return INT_VALUE(from_len);
}

Value native_fn_list_remove(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *index_value = &values[0];
    DynArr *list = TO_LIST(target);

    VALIDATE_LIST_INDEX(DYNARR_LEN(list), index_value, vm)

    int64_t index = TO_INT(index_value);
    Value out_value = DYNARR_GET_AS(Value, (size_t)index, list);

    dynarr_remove_index((size_t)index, list);

    return out_value;
}

Value native_fn_list_append_new(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *from_value = &values[0];
    DynArr *to = TO_LIST(target);

    VALIDATE_VALUE_LIST_ARG(from_value, 1, "from list", vm)

	DynArr *from = TO_LIST(from_value);
	Obj *new_list_obj = vmu_list_obj(vm);
	DynArr *new_list = new_list_obj->content.list;

	for(lidx_t i = 0; i < (lidx_t)DYNARR_LEN(to); i++){
		Value value = DYNARR_GET_AS(Value, i, to);
		dynarr_insert(&value, new_list);
	}

	for(lidx_t i = 0; i < (lidx_t)DYNARR_LEN(from); i++){
		Value value = DYNARR_GET_AS(Value, i, from);
		dynarr_insert(&value, new_list);
	}

    return OBJ_VALUE(new_list_obj);
}

Value native_fn_list_clear(uint8_t argsc, Value *values, Value *target, VM *vm){
    DynArr *list = TO_LIST(target);
    int64_t list_len = (int64_t)list->used;

    dynarr_remove_all(list);

    return INT_VALUE(list_len);
}

NativeFnInfo *native_list_get(char *symbol, VM *vm){
    if(!list_symbols){
        list_symbols = FACTORY_LZHTABLE(vm->rtallocator);
        factory_add_native_fn_info("size", 0, native_fn_list_size, list_symbols, vm->rtallocator);
        factory_add_native_fn_info("capacity", 0, native_fn_list_capacity, list_symbols, vm->rtallocator);
        factory_add_native_fn_info("available", 0, native_fn_list_available, list_symbols, vm->rtallocator);
        factory_add_native_fn_info("first", 0, native_fn_list_first, list_symbols, vm->rtallocator);
        factory_add_native_fn_info("last", 0, native_fn_list_last, list_symbols, vm->rtallocator);
        factory_add_native_fn_info("reverse", 0, native_fn_list_reverse, list_symbols, vm->rtallocator);
        factory_add_native_fn_info("get", 1, native_fn_list_get, list_symbols, vm->rtallocator);
        factory_add_native_fn_info("insert", 1, native_fn_list_insert, list_symbols, vm->rtallocator);
        factory_add_native_fn_info("insert_at", 2, native_fn_list_insert_at, list_symbols, vm->rtallocator);
        factory_add_native_fn_info("set", 2, native_fn_list_set, list_symbols, vm->rtallocator);
        factory_add_native_fn_info("append", 1, native_fn_list_append, list_symbols, vm->rtallocator);
        factory_add_native_fn_info("remove", 1, native_fn_list_remove, list_symbols, vm->rtallocator);
        factory_add_native_fn_info("clear", 0, native_fn_list_clear, list_symbols, vm->rtallocator);
    }

    return (NativeFnInfo *)lzhtable_get((uint8_t *)symbol, strlen(symbol), list_symbols);
}

#endif
