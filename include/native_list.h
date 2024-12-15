#ifndef NATIVE_LIST
#define NATIVE_LIST

#include "types.h"
#include "value.h"
#include "vm_utils.h"

Value native_fn_list_get(uint8_t argc, Value *values, void *target, VM *vm){
    int64_t index = -1;
    DynArr *list = (DynArr *)target;

    VALIDATE_INDEX(&values[0], index, list->used)

    return DYNARR_GET_AS(Value, (size_t)index, list);
}

Value native_fn_list_insert(uint8_t argc, Value *values, void *target, VM *vm){
    Value *value = &values[0];
    DynArr *list = (DynArr *)target;

    if(dynarr_insert(value, list))
        vm_utils_error(vm, "Failed to insert value in list: out of memory");

    return EMPTY_VALUE;
}

Value native_fn_list_insert_at(uint8_t argc, Value *values, void *target, VM *vm){
    DynArr *list = (DynArr *)target;
    Value *index_value = &values[0];
    Value *value = &values[1];
    int64_t index = -1;

    VALIDATE_INDEX(index_value, index, list->used)

    Value out_value = DYNARR_GET_AS(Value, (size_t)index, list);

    if(dynarr_insert_at((size_t) index, value, list))
        vm_utils_error(vm, "Failed to insert value in list: out of memory");

    return out_value;
}

Value native_fn_list_set(uint8_t argc, Value *values, void *target, VM *vm){
    DynArr *list = (DynArr *)target;
    Value *index_value = &values[0];
    Value *value = &values[1];
    int64_t index = -1;

    VALIDATE_INDEX(index_value, index, list->used)

    Value out_value = DYNARR_GET_AS(Value, (size_t)index, list);

    DYNARR_SET(value, (size_t)index, list);

    return out_value;
}

Value native_fn_list_remove(uint8_t argc, Value *values, void *target, VM *vm){
    DynArr *list = (DynArr *)target;
    Value *index_value = &values[0];
    int64_t index = -1;

    VALIDATE_INDEX(index_value, index, list->used)

    Value out_value = DYNARR_GET_AS(Value, (size_t)index, list);

    dynarr_remove_index((size_t)index, list);
    
    return out_value;
}

Value native_fn_list_append(uint8_t argc, Value *values, void *target, VM *vm){
    DynArr *to = (DynArr *)target;
    Value *list_value = &values[0];
    DynArr *from = NULL;

    if(!vm_utils_is_list(list_value, &from))
        vm_utils_error(vm, "Failed to append list: expect another list, but got something else");

    int64_t from_len = (int64_t)from->used;

    if(dynarr_append(from, to))
        vm_utils_error(vm, "Failed to append to list: out of memory");
    
    return INT_VALUE(from_len);
}

Value native_fn_list_append_new(uint8_t argsc, Value *values, void *target, VM *vm){
    DynArr *to = (DynArr *)target;
    Value *list_value = &values[0];
    DynArr *from = NULL;

    if(!vm_utils_is_list(list_value, &from)){
        vm_utils_error(vm, "Failed to append list: expect another list, but got something else");
	}

	Obj *list_obj = vm_utils_list_obj(vm);
	if(!list_obj) vm_utils_error(vm, "Out of memory");

	DynArr *list = list_obj->value.list;

	for(size_t i = 0; i < to->used; i++){
		Value value = DYNARR_GET_AS(Value, i, to);
	
		if(dynarr_insert(&value, list))
			vm_utils_error(vm, "Out of memory");
	}

	for(size_t i = 0; i < from->used; i++){
		Value value = DYNARR_GET_AS(Value, i, from);
	
		if(dynarr_insert(&value, list))
			vm_utils_error(vm, "Out of memory");
	}

    return OBJ_VALUE(list_obj);
}

Value native_fn_list_clear(uint8_t argc, Value *values, void *target, VM *vm){
    DynArr *list = (DynArr *)target;
    int64_t list_len = (int64_t)list->used;

    dynarr_remove_all(list);

    return INT_VALUE(list_len);
}

#endif
