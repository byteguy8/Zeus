#ifndef NATIVE_H
#define NATIVE_H

Value native_fn_ls(uint8_t argsc, Value *values, void *target, VM *vm){
	Value *module_value = &values[0];
    Obj *array_obj = NULL;

    if(IS_NATIVE_MODULE(module_value)){
        NativeModule *module = TO_NATIVE_MODULE(module_value);
        LZHTable *symbols = module->symbols;

        if(LZHTABLE_COUNT(symbols) > (size_t)INT32_MAX){
            vmu_error(vm, "Module too long to analyze");
        }

        array_obj = vmu_array_obj((int32_t)LZHTABLE_COUNT(symbols), vm);

        if(!array_obj){
            vmu_error(vm, "Out of memory");
        }

        Array *array = array_obj->content.array;
        int32_t i = 0;

        for(LZHTableNode *current = symbols->head; current; current = current->next_table_node){
            NativeModuleSymbol *symbol = (NativeModuleSymbol *)current->value;
            Obj *native_fn_obj = vmu_obj(NATIVE_FN_OTYPE, vm);

            if(!native_fn_obj){
                vmu_error(vm, "Out of memory");
            }

            native_fn_obj->content.native_fn = symbol->value.fn;
            array->values[i++] = OBJ_VALUE(native_fn_obj);
        }
    }else if(IS_MODULE(module_value)){
        Module *module = TO_MODULE(module_value);
        LZHTable *global_values = MODULE_GLOBALS(module);

        DynArr *values = memory_dynarr(sizeof(Value));
        if(!values){vmu_error(vm, "Out of memory");}

        for(LZHTableNode *current = global_values->head; current; current = current->next_table_node){
            GlobalValue *global_value = (GlobalValue *)current->value;
            if(global_value->access == PRIVATE_GVATYPE){continue;}

            Value *value = global_value->value;

            if(dynarr_insert(value, values)){
                dynarr_destroy(values);
                vmu_error(vm, "Out of memory");
            }
        }

        if(DYNARR_LEN(values) > (size_t)INT32_MAX){
            vmu_error(vm, "Module too long to analyze");
        }

        array_obj = vmu_array_obj((int32_t)DYNARR_LEN(values), vm);
        if(!array_obj){vmu_error(vm, "Out of memory");}

        Array *array = array_obj->content.array;

        for (int32_t i = 0; i < array->len; i++){
            array->values[i] = DYNARR_GET_AS(Value, (size_t)i, values);
        }

        dynarr_destroy(values);
    }else{
        vmu_error(vm, "Expect module, but got something else");
    }

	return OBJ_VALUE(array_obj);
}

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
