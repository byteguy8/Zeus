#ifndef NATIVE_H
#define NATIVE_H

Value native_fn_assert(uint8_t argsc, Value *values, void *target, VM *vm){
	Value *raw_value = &values[0];
	
    if(!IS_BOOL(raw_value))
        vm_utils_error(vm, "Expect boolean, but got something else");

    uint8_t value = TO_BOOL(raw_value);
    
    if(!value)
		vm_utils_error(vm, "Assertion failed");

	return EMPTY_VALUE;
}

Value native_fn_is_str_int(uint8_t argsc, Value *values, void *target, VM *vm){
	Value *raw_value = &values[0];
	Str *str = NULL;
	
	if(!IS_STR(raw_value))
		vm_utils_error(vm, "Expect a string, but got something else");

	str = TO_STR(raw_value);

	return BOOL_VALUE((uint8_t)utils_is_integer(str->buff));
}

Value native_fn_str_to_int(uint8_t argsc, Value *values, void *target, VM *vm){
	Value *raw_value = &values[0];
	Str *str = NULL;
    int64_t value;
	
	if(!IS_STR(raw_value))
        vm_utils_error(vm, "Expect a string, but got something else");

	str = TO_STR(raw_value);

	if(utils_str_to_i64(str->buff, &value))
        vm_utils_error(vm, "String do not contains a valid integer");

	return INT_VALUE(value);
}

Value native_fn_int_to_str(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];
    int64_t value = 0;
    char buff[20];

    if(!IS_INT(raw_value))
		vm_utils_error(vm, "Expect integer, but got something else");
	
	Value str_value = {0};
	value = TO_INT(raw_value);
    int len = utils_i64_to_str(value, buff);

    if(!vm_utils_range_str_obj(0, len - 1, buff, &str_value, vm))
        vm_utils_error(vm, "Out of memory");

    return str_value;
}

Value native_fn_int_to_float(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_INT(raw_value))
		vm_utils_error(vm, "Expect integer, but got something else");
	
    return FLOAT_VALUE((double)TO_INT(raw_value));
}

Value native_fn_float_to_int(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];
    
    if(!IS_FLOAT(raw_value))
		vm_utils_error(vm, "Expect float, but got something else");
	
    return INT_VALUE((uint64_t)TO_FLOAT(raw_value));
}

#endif
