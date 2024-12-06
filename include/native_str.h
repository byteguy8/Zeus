#ifndef NATIVE_STR
#define NATIVE_STR

#include "types.h"
#include "value.h"
#include "vm_utils.h"

Value native_fn_char_at(uint8_t argc, Value *values, void *target, VM *vm){
    int64_t index = -1;
    Str *in_str = (Str *)target;

    VALIDATE_INDEX(&values[0], index, in_str->len)
    
    Value value = {0};

    if(!vm_utils_range_str_obj(index, index, in_str->buff, &value, vm))
		vm_utils_error(vm, "Out of memory");
    
    return value;
}

Value native_fn_sub_str(uint8_t argc, Value *values, void *target, VM *vm){
    Str *in_str = (Str *)target;
    int64_t from = -1;
    int64_t to = -1;

    VALIDATE_INDEX_NAME(&values[0], from, in_str->len, "from")
    VALIDATE_INDEX_NAME(&values[1], to, in_str->len, "to")

    if(from > to) 
        vm_utils_error(vm, "Illegal index 'from'(%ld). Must be less or equals to 'to'(%ld)", from, to);

    Value value = {0};

    if(!vm_utils_range_str_obj(from, to, in_str->buff, &value, vm))
		vm_utils_error(vm, "Out of memory");

    return value;
}

Value native_fn_char_code(uint8_t argc, Value *values, void *target, VM *vm){
    Str *str = (Str *)target;
    int64_t index = -1;

    VALIDATE_INDEX(&values[0], index, str->len)
    int64_t code = (int64_t)str->buff[index];

    return INT_VALUE(code);
}

Value native_fn_split(uint8_t argc, Value *values, void *target, VM *vm){
    Str *str = (Str *)target;
    Str *by = NULL;

    if(!vm_utils_is_str(&values[0], &by))
        vm_utils_error(vm, "Expect string as 'by' to split");
    if(by->len != 1)
        vm_utils_error(vm, "Expect string of length 1 as 'by' to split");

    DynArr *list = assert_ptr(vm_utils_dyarr(vm), vm);

	char coincidence = 0;
    size_t from = 0;
    size_t to = 0;
    size_t i = 0;

    while (i < str->len){
        char c = str->buff[i++];
        
        if(c == by->buff[0]){
            to = i - 1;
			size_t len = to - from;

			if(!coincidence) coincidence = 1;
			
			Value str_value = {0};

			if(len == 0){
				if(!vm_utils_empty_str_obj(&str_value, vm)) goto FAIL;
			}else{
				if(!vm_utils_range_str_obj(from, to - 1, str->buff, &str_value, vm)) goto FAIL;
			}

			if(dynarr_insert(&str_value, list)) goto FAIL;

            from = i;
        }
    }

	if(coincidence){
		Value str_value = {0};

    	if(!vm_utils_range_str_obj(from, i - 1, str->buff, &str_value, vm)) goto FAIL;
		if(dynarr_insert(&str_value, list)) goto FAIL;
	}

    Obj *obj_list = vm_utils_obj(LIST_OTYPE, vm);
	if(!obj_list) goto FAIL;

    obj_list->value.list = list;

	goto OK;

FAIL:
	dynarr_destroy(list);
	vm_utils_error(vm, "Out of memory");

OK:
    return OBJ_VALUE(obj_list);
}

#endif
