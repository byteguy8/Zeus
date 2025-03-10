#ifndef NATIVE_H
#define NATIVE_H

Value native_fn_exit(uint8_t argsc, Value *values, void *target, VM *vm){
	Value *exit_code_value = &values[0];
	
    if(!IS_INT(exit_code_value)){
        vmu_error(vm, "Expect integer, but got something else");
    }

    vm->halt = 1;
    vm->exit_code = (unsigned char)TO_INT(exit_code_value);

	return EMPTY_VALUE;
}

Value native_fn_assert(uint8_t argsc, Value *values, void *target, VM *vm){
	Value *raw_value = &values[0];
	
    if(!IS_BOOL(raw_value))
        vmu_error(vm, "Expect boolean, but got something else");

    uint8_t value = TO_BOOL(raw_value);
    
    if(!value)
		vmu_error(vm, "Assertion failed");

	return EMPTY_VALUE;
}

Value native_fn_assertm(uint8_t argsc, Value *values, void *target, VM *vm){
	Value *raw_value = &values[0];
    Value *msg_value = &values[1];
	
    if(!IS_BOOL(raw_value)){
        vmu_error(vm, "Expect boolean at argument 0, but got something else");
    }

    uint8_t value = TO_BOOL(raw_value);

    if(!IS_STR(msg_value)){
        vmu_error(vm, "Expect string at argument 1, but got something else");
    }

    Str *msg = TO_STR(msg_value);
    
    if(!value){
        vmu_error(vm, "Assertion failed: %s", msg->buff);
    }

	return EMPTY_VALUE;
}

Value native_fn_is_str_int(uint8_t argsc, Value *values, void *target, VM *vm){
	Value *raw_value = &values[0];
	Str *str = NULL;
	
	if(!IS_STR(raw_value))
		vmu_error(vm, "Expect a string, but got something else");

	str = TO_STR(raw_value);

	return BOOL_VALUE((uint8_t)utils_is_integer(str->buff));
}

Value native_fn_is_str_float(uint8_t argsc, Value *values, void *target, VM *vm){
	Value *raw_value = &values[0];
	Str *str = NULL;
	
	if(!IS_STR(raw_value))
		vmu_error(vm, "Expect a string, but got something else");

	str = TO_STR(raw_value);

	return BOOL_VALUE((uint8_t)utils_is_float(str->buff));
}

Value native_fn_str_to_int(uint8_t argsc, Value *values, void *target, VM *vm){
	Value *raw_value = &values[0];
	Str *str = NULL;
    int64_t value;
	
	if(!IS_STR(raw_value))
        vmu_error(vm, "Expect a string, but got something else");

	str = TO_STR(raw_value);

	if(utils_str_to_i64(str->buff, &value))
        vmu_error(vm, "String do not contains a valid integer");

	return INT_VALUE(value);
}

Value native_fn_int_to_str(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];
    int64_t value = 0;
    char buff[20];

    if(!IS_INT(raw_value))
		vmu_error(vm, "Expect integer, but got something else");
	
	Value str_value = {0};
	value = TO_INT(raw_value);
    int len = utils_i64_to_str(value, buff);

    if(!vmu_range_str_obj(0, len - 1, buff, &str_value, vm))
        vmu_error(vm, "Out of memory");

    return str_value;
}

Value native_fn_str_to_float(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];
    Str *str = NULL;
    double value;

    if(!IS_STR(raw_value)){
        vmu_error(vm, "Expect string, but got something else");
    }

    str = TO_STR(raw_value);

    if(utils_str_to_double(str->buff, &value)){
        vmu_error(vm, "String do not contains a valid integer");
    }

	return FLOAT_VALUE(value);
}

Value native_fn_float_to_str(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];
    double value = 0;
    size_t buff_len = 1024;
    char buff[buff_len];

    if(!IS_FLOAT(raw_value)){
        vmu_error(vm, "Expect float, but got something else");
    }
	
	Value str_value = {0};
	value = TO_FLOAT(raw_value);
    int len = utils_double_to_str(buff_len, value, buff);

    if(len == -1){
        vmu_error(vm, "Unexpected error");
    }

    if(!vmu_range_str_obj(0, len - 1, buff, &str_value, vm)){
        vmu_error(vm, "Out of memory");
    }

    return str_value;
}

Value native_fn_int_to_float(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_INT(raw_value))
		vmu_error(vm, "Expect integer, but got something else");
	
    return FLOAT_VALUE((double)TO_INT(raw_value));
}

Value native_fn_float_to_int(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];
    
    if(!IS_FLOAT(raw_value))
		vmu_error(vm, "Expect float, but got something else");
	
    return INT_VALUE((uint64_t)TO_FLOAT(raw_value));
}

Value native_fn_print(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];
    vmu_print_value(stdout, raw_value);
    return EMPTY_VALUE;
}

Value native_fn_printerr(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];
    vmu_print_value(stderr, raw_value);
    return EMPTY_VALUE;
}

#endif
