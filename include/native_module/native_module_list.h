#ifndef NATIVE_LIST
#define NATIVE_LIST

#include "memory.h"
#include "vmu.h"

static LZHTable *list_symbols = NULL;

Value native_fn_list_size(uint8_t argsc, Value *values, Value *target, void *context){
    ListObj *list_obj = VALUE_TO_LIST(target);
    DynArr *list = list_obj->list;
    return INT_VALUE((int64_t)DYNARR_LEN(list));
}

Value native_fn_list_capacity(uint8_t argsc, Value *values, Value *target, void *context){
    ListObj *list_obj = VALUE_TO_LIST(target);
    DynArr *list = list_obj->list;
    return INT_VALUE((int64_t)(list->count));
}

Value native_fn_list_available(uint8_t argsc, Value *values, Value *target, void *context){
    ListObj *list_obj = VALUE_TO_LIST(target);
    DynArr *list = list_obj->list;
    return INT_VALUE((int64_t)(DYNARR_AVAILABLE(list)));
}

Value native_fn_list_first(uint8_t argsc, Value *values, Value *target, void *context){
    ListObj *list_obj = VALUE_TO_LIST(target);
    DynArr *list = list_obj->list;

    if(DYNARR_LEN(list) == 0){
        return EMPTY_VALUE;
    }else{
        return DYNARR_GET_AS(Value, 0, list);
    }
}

Value native_fn_list_last(uint8_t argsc, Value *values, Value *target, void *context){
    ListObj *list_obj = VALUE_TO_LIST(target);
    DynArr *list = list_obj->list;

    if(DYNARR_LEN(list) == 0){
        return EMPTY_VALUE;
    }else{
        return DYNARR_GET_AS(Value, DYNARR_LEN(list) - 1, list);
    }
}

Value native_fn_list_reverse(uint8_t argsc, Value *values, Value *target, void *context){
    ListObj *list_obj = VALUE_TO_LIST(target);
    DynArr *list = list_obj->list;
    dynarr_reverse(list);
    return EMPTY_VALUE;
}

Value native_fn_list_get(uint8_t argsc, Value *values, Value *target, void *context){
    Value *index_value = &values[0];
    ListObj *list_obj = VALUE_TO_LIST(target);
    DynArr *list = list_obj->list;

    VALIDATE_LIST_INDEX(DYNARR_LEN(list), index_value, context);

    int64_t index = VALUE_TO_INT(index_value);

    return DYNARR_GET_AS(Value, (size_t)index, list);
}

Value native_fn_list_insert(uint8_t argsc, Value *values, Value *target, void *context){
    Value *value = &values[0];
    ListObj *list_obj = VALUE_TO_LIST(target);
    DynArr *list = list_obj->list;

    dynarr_insert(value, list);

    return EMPTY_VALUE;
}

Value native_fn_list_insert_at(uint8_t argsc, Value *values, Value *target, void *context){
    Value *index_value = &values[0];
    Value *element_value = &values[1];
    ListObj *list_obj = VALUE_TO_LIST(target);
    DynArr *list = list_obj->list;

    VALIDATE_LIST_INDEX(DYNARR_LEN(list), index_value, context)

    int64_t index = VALUE_TO_INT(index_value);
    Value old_value = DYNARR_GET_AS(Value, (size_t)index, list);

    dynarr_insert_at((size_t) index, element_value, list);

    return old_value;
}

Value native_fn_list_set(uint8_t argsc, Value *values, Value *target, void *context){
    Value *index_value = &values[0];
    Value *value = &values[1];
    ListObj *list_obj = VALUE_TO_LIST(target);
    DynArr *list = list_obj->list;

    VALIDATE_LIST_INDEX(DYNARR_LEN(list), index_value, context)

    int64_t index = VALUE_TO_INT(index_value);
    Value out_value = DYNARR_GET_AS(Value, (size_t)index, list);

    DYNARR_SET(value, (size_t)index, list);

    return out_value;
}

Value native_fn_list_append(uint8_t argsc, Value *values, Value *target, void *context){
    Value *from_value = &values[0];
    ListObj *to_list_obj = VALUE_TO_LIST(target);
    DynArr *to_list = to_list_obj->list;

    VALIDATE_VALUE_LIST_ARG(from_value, 1, "from list", context)

	ListObj *from_list_obj = VALUE_TO_LIST(from_value);
    DynArr *from_list = from_list_obj->list;
    int64_t from_len = (int64_t)from_list->used;

    dynarr_append(from_list, to_list);

    return INT_VALUE(from_len);
}

Value native_fn_list_remove(uint8_t argsc, Value *values, Value *target, void *context){
    Value *index_value = &values[0];
    ListObj *list_obj = VALUE_TO_LIST(target);
    DynArr *list = list_obj->list;

    VALIDATE_LIST_INDEX(DYNARR_LEN(list), index_value, context)

    int64_t index = VALUE_TO_INT(index_value);
    Value out_value = DYNARR_GET_AS(Value, (size_t)index, list);

    dynarr_remove_index((size_t)index, list);

    return out_value;
}

Value native_fn_list_append_new(uint8_t argsc, Value *values, Value *target, void *context){
    Value *from_value = &values[0];
    ListObj *target_list_obj = VALUE_TO_LIST(target);
    DynArr *to = target_list_obj->list;

    VALIDATE_VALUE_LIST_ARG(from_value, 1, "from list", context)

	ListObj *from_list_obj = VALUE_TO_LIST(from_value);
    DynArr *from = from_list_obj->list;
	ObjHeader *new_list_header = vmu_create_list_obj(context);
	ListObj *list_obj = OBJ_TO_LIST(new_list_header);
    DynArr *new_list = list_obj->list;

	for(lidx_t i = 0; i < (lidx_t)DYNARR_LEN(to); i++){
		Value value = DYNARR_GET_AS(Value, i, to);
		dynarr_insert(&value, new_list);
	}

	for(lidx_t i = 0; i < (lidx_t)DYNARR_LEN(from); i++){
		Value value = DYNARR_GET_AS(Value, i, from);
		dynarr_insert(&value, new_list);
	}

    return OBJ_VALUE(new_list_header);
}

Value native_fn_list_clear(uint8_t argsc, Value *values, Value *target, void *context){
    ListObj *list_obj = VALUE_TO_LIST(target);
    DynArr *list = list_obj->list;
    int64_t list_len = (int64_t)list->used;

    dynarr_remove_all(list);

    return INT_VALUE(list_len);
}

NativeFnInfo *native_list_get(char *symbol, Allocator *allocator){
    if(!list_symbols){
        list_symbols = FACTORY_LZHTABLE(allocator);

        factory_add_native_fn_info("size", 0, native_fn_list_size, list_symbols, allocator);
        factory_add_native_fn_info("capacity", 0, native_fn_list_capacity, list_symbols, allocator);
        factory_add_native_fn_info("available", 0, native_fn_list_available, list_symbols, allocator);
        factory_add_native_fn_info("first", 0, native_fn_list_first, list_symbols, allocator);
        factory_add_native_fn_info("last", 0, native_fn_list_last, list_symbols, allocator);
        factory_add_native_fn_info("reverse", 0, native_fn_list_reverse, list_symbols, allocator);
        factory_add_native_fn_info("get", 1, native_fn_list_get, list_symbols, allocator);
        factory_add_native_fn_info("insert", 1, native_fn_list_insert, list_symbols, allocator);
        factory_add_native_fn_info("insert_at", 2, native_fn_list_insert_at, list_symbols, allocator);
        factory_add_native_fn_info("set", 2, native_fn_list_set, list_symbols, allocator);
        factory_add_native_fn_info("append", 1, native_fn_list_append, list_symbols, allocator);
        factory_add_native_fn_info("remove", 1, native_fn_list_remove, list_symbols, allocator);
        factory_add_native_fn_info("clear", 0, native_fn_list_clear, list_symbols, allocator);
    }

    return (NativeFnInfo *)lzhtable_get((uint8_t *)symbol, strlen(symbol), list_symbols);
}

#endif
