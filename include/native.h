#ifndef NATIVE_H
#define NATIVE_H

Value native_fn_is_str_int(uint8_t argsc, Value *values, void *target, VM *vm){
	Value *raw_value = &values[0];
	Str *str = NULL;
	
	if(!vm_utils_is_str(raw_value, &str))
		vm_utils_error(vm, "Expect a string, but got something else");

	return BOOL_VALUE((uint8_t)utils_is_integer(str->buff));
}

Value native_fn_str_to_int(uint8_t argsc, Value *values, void *target, VM *vm){
	Value *raw_value = &values[0];
	Str *str = NULL;
    int64_t value;
	
	if(!vm_utils_is_str(raw_value, &str))
        vm_utils_error(vm, "Expect a string, but got something else");

	if(utils_str_to_i64(str->buff, &value))
        vm_utils_error(vm, "String do not contains a valid integer");

	return INT_VALUE(value);
}

Value native_fn_int_to_str(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];
    int64_t value = 0;
    char buff[20];

    if(!vm_utils_is_i64(raw_value, &value))
        vm_utils_error(vm, "Expect integer, but got something else");

    int len = utils_i64_to_str(value, buff);
    Value str_value = {0};

    if(!vm_utils_range_str_obj(0, len - 1, buff, &str_value, vm))
        vm_utils_error(vm, "Out of memory");

    return str_value;
}

#endif
