#ifndef NATIVE_LIST
#define NATIVE_LIST

#include "rtypes.h"
#include "memory.h"
#include "vm_utils.h"

static LZHTable *list_symbols = NULL;

Value native_fn_list_size(uint8_t argsc, Value *values, void *target, VM *vm){
    DynArr *list = (DynArr *)target;
    return INT_VALUE((int64_t)DYNARR_LEN(list));
}

Value native_fn_list_capacity(uint8_t argsc, Value *values, void *target, VM *vm){
    DynArr *list = (DynArr *)target;
    return INT_VALUE((int64_t)(list->count));
}

Value native_fn_list_available(uint8_t argsc, Value *values, void *target, VM *vm){
    DynArr *list = (DynArr *)target;
    return INT_VALUE((int64_t)(DYNARR_AVAILABLE(list)));
}

Value native_fn_list_first(uint8_t argsc, Value *values, void *target, VM *vm){
    DynArr *list = (DynArr *)target;

    if(DYNARR_LEN(list) == 0){
        return EMPTY_VALUE;
    }else{
        return DYNARR_GET_AS(Value, 0, list);
    }
}

Value native_fn_list_last(uint8_t argsc, Value *values, void *target, VM *vm){
    DynArr *list = (DynArr *)target;

    if(DYNARR_LEN(list) == 0){
        return EMPTY_VALUE;
    }else{
        return DYNARR_GET_AS(Value, DYNARR_LEN(list) - 1, list);
    }
}

Value native_fn_list_reverse(uint8_t argsc, Value *values, void *target, VM *vm){
    DynArr *list = (DynArr *)target;
    dynarr_reverse(list);
    return EMPTY_VALUE;
}

Value native_fn_list_get(uint8_t argsc, Value *values, void *target, VM *vm){
    int64_t index = -1;
    DynArr *list = (DynArr *)target;

    VALIDATE_INDEX(&values[0], index, list->used)

    return DYNARR_GET_AS(Value, (size_t)index, list);
}

Value native_fn_list_insert(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *value = &values[0];
    DynArr *list = (DynArr *)target;

    if(dynarr_insert(value, list)){
        vmu_error(vm, "Failed to insert value in list: out of memory");
    }

    return EMPTY_VALUE;
}

Value native_fn_list_insert_at(uint8_t argsc, Value *values, void *target, VM *vm){
    DynArr *list = (DynArr *)target;
    Value *index_value = &values[0];
    Value *value = &values[1];
    int64_t index = -1;

    VALIDATE_INDEX(index_value, index, list->used)

    Value out_value = DYNARR_GET_AS(Value, (size_t)index, list);

    if(dynarr_insert_at((size_t) index, value, list)){
        vmu_error(vm, "Failed to insert value in list: out of memory");
    }

    return out_value;
}

Value native_fn_list_set(uint8_t argsc, Value *values, void *target, VM *vm){
    DynArr *list = (DynArr *)target;
    Value *index_value = &values[0];
    Value *value = &values[1];
    int64_t index = -1;

    VALIDATE_INDEX(index_value, index, list->used)

    Value out_value = DYNARR_GET_AS(Value, (size_t)index, list);

    DYNARR_SET(value, (size_t)index, list);

    return out_value;
}

Value native_fn_list_append(uint8_t argsc, Value *values, void *target, VM *vm){
    DynArr *to = (DynArr *)target;
    Value *vfrom = &values[0];
    DynArr *from = NULL;

    if(!IS_LIST(vfrom)){
        vmu_error(vm, "Failed to append list: expect another list, but got something else");
    }

	from = TO_LIST(vfrom);
    int64_t from_len = (int64_t)from->used;

    if(dynarr_append(from, to)){
        vmu_error(vm, "Failed to append to list: out of memory");
    }

    return INT_VALUE(from_len);
}

Value native_fn_list_remove(uint8_t argsc, Value *values, void *target, VM *vm){
    DynArr *list = (DynArr *)target;
    Value *index_value = &values[0];
    int64_t index = -1;

    VALIDATE_INDEX(index_value, index, list->used)

    Value out_value = DYNARR_GET_AS(Value, (size_t)index, list);

    dynarr_remove_index((size_t)index, list);

    return out_value;
}

Value native_fn_list_append_new(uint8_t argsc, Value *values, void *target, VM *vm){
    DynArr *to = (DynArr *)target;
    Value *vfrom = &values[0];
    DynArr *from = NULL;

    if(!IS_LIST(vfrom)){
        vmu_error(vm, "Failed to append list: expect another list, but got something else");
    }

	from = TO_LIST(vfrom);
	Obj *list_obj = vmu_list_obj(vm);

	if(!list_obj){vmu_error(vm, "Out of memory");}

	DynArr *list = list_obj->content.list;

	for(size_t i = 0; i < to->used; i++){
		Value value = DYNARR_GET_AS(Value, i, to);
		if(dynarr_insert(&value, list)){vmu_error(vm, "Out of memory");}
	}

	for(size_t i = 0; i < from->used; i++){
		Value value = DYNARR_GET_AS(Value, i, from);
		if(dynarr_insert(&value, list)){vmu_error(vm, "Out of memory");}
	}

    return OBJ_VALUE(list_obj);
}

Value native_fn_list_clear(uint8_t argsc, Value *values, void *target, VM *vm){
    DynArr *list = (DynArr *)target;
    int64_t list_len = (int64_t)list->used;

    if(!dynarr_remove_all(list)){
        vmu_error(vm, "Failed to clear list. Memory error");
    }

    return INT_VALUE(list_len);
}

Obj *native_list_get(char *symbol, void *target, VM *vm){
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

    size_t key_size = strlen(symbol);
    NativeFnInfo *native_fn_info = (NativeFnInfo *)lzhtable_get((uint8_t *)symbol, key_size, list_symbols);

    if(native_fn_info){
        Obj *native_fn_obj = vmu_native_fn_obj(
            native_fn_info->arity,
            symbol,
            target,
            native_fn_info->raw_native,
            vm
        );

        if(!native_fn_obj){
            vmu_error(vm, "Out of memory");
        }

        return native_fn_obj;
    }

    return NULL;
}

#endif
