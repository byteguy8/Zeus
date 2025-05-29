#ifndef NATIVE_H
#define NATIVE_H

#include "tutils.h"

Value native_fn_print_stack(uint8_t argsc, Value *values, Value *target, VM *vm){
    for (Value *current = vm->stack; current < vm->stack_top; current++){
        vmu_print_value(stdout, current);
        fprintf(stdout, "\n");
    }
    return EMPTY_VALUE;
}

Value native_fn_ls(uint8_t argsc, Value *values, Value *target, VM *vm){
	Value *module_value = &values[0];
    Obj *array_obj = NULL;

    if(IS_VALUE_NATIVE_MODULE(module_value)){
        NativeModule *module = VALUE_TO_NATIVE_MODULE(module_value);
        LZHTable *symbols = module->symbols;

        if(LZHTABLE_COUNT(symbols) > (size_t)INT32_MAX){
            vmu_error(vm, "Module too long to analyze");
        }

        array_obj = vmu_array_obj((int32_t)LZHTABLE_COUNT(symbols), vm);

        Array *array = OBJ_TO_ARRAY(array_obj);
        int32_t i = 0;

        for(LZHTableNode *current = symbols->head; current; current = current->next_table_node){
            NativeModuleSymbol *symbol = (NativeModuleSymbol *)current->value;
            Obj *native_fn_obj = vmu_create_obj(NATIVE_FN_OTYPE, vm);

            native_fn_obj->content = symbol->value.fn;
            array->values[i++] = OBJ_VALUE(native_fn_obj);
        }
    }else if(IS_VALUE_MODULE(module_value)){
        Module *module = VALUE_TO_MODULE(module_value);
        LZHTable *global_values = MODULE_GLOBALS(module);

        DynArr *values = FACTORY_DYNARR(sizeof(Value), vm->fake_allocator);
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

        Array *array = OBJ_TO_ARRAY(array_obj);

        for (int32_t i = 0; i < array->len; i++){
            array->values[i] = DYNARR_GET_AS(Value, (size_t)i, values);
        }

        dynarr_destroy(values);
    }else{
        vmu_error(vm, "Expect module, but got something else");
    }

	return OBJ_VALUE(array_obj);
}

Value native_fn_exit(uint8_t argsc, Value *values, Value *target, VM *vm){
	Value *exit_code_value = &values[0];

    if(!IS_VALUE_INT(exit_code_value)){
        vmu_error(vm, "Expect integer, but got something else");
    }

    vm->halt = 1;
    vm->exit_code = (unsigned char)VALUE_TO_INT(exit_code_value);

	return EMPTY_VALUE;
}

Value native_fn_assert(uint8_t argsc, Value *values, Value *target, VM *vm){
	Value *raw_value = &values[0];

    if(!IS_VALUE_BOOL(raw_value)){
        vmu_error(vm, "Expect boolean, but got something else");
    }

    uint8_t value = VALUE_TO_BOOL(raw_value);

    if(!value){
        vmu_error(vm, "Assertion failed");
    }

	return EMPTY_VALUE;
}

Value native_fn_assertm(uint8_t argsc, Value *values, Value *target, VM *vm){
	Value *raw_value = &values[0];
    Value *msg_value = &values[1];

    if(!IS_VALUE_BOOL(raw_value)){
        vmu_error(vm, "Expect boolean at argument 0, but got something else");
    }

    uint8_t value = VALUE_TO_BOOL(raw_value);

    if(!IS_VALUE_STR(msg_value)){
        vmu_error(vm, "Expect string at argument 1, but got something else");
    }

    Str *msg = VALUE_TO_STR(msg_value);

    if(!value){
        vmu_error(vm, "Assertion failed: %s", msg->buff);
    }

	return EMPTY_VALUE;
}

Value native_fn_is_str_int(uint8_t argsc, Value *values, Value *target, VM *vm){
	Value *raw_value = &values[0];
	Str *str = NULL;

	if(!IS_VALUE_STR(raw_value)){
        vmu_error(vm, "Expect a string, but got something else");
    }

	str = VALUE_TO_STR(raw_value);

	return BOOL_VALUE((uint8_t)utils_is_integer(str->buff));
}

Value native_fn_is_str_float(uint8_t argsc, Value *values, Value *target, VM *vm){
	Value *raw_value = &values[0];
	Str *str = NULL;

	if(!IS_VALUE_STR(raw_value)){
        vmu_error(vm, "Expect a string, but got something else");
    }

	str = VALUE_TO_STR(raw_value);

	return BOOL_VALUE((uint8_t)utils_is_float(str->buff));
}

Value native_fn_str_to_int(uint8_t argsc, Value *values, Value *target, VM *vm){
	Value *raw_value = &values[0];
	Str *str = NULL;
    int64_t value;

	if(!IS_VALUE_STR(raw_value)){
        vmu_error(vm, "Expect a string, but got something else");
    }

	str = VALUE_TO_STR(raw_value);

	if(utils_str_to_i64(str->buff, &value)){
        vmu_error(vm, "String do not contains a valid integer");
    }

	return INT_VALUE(value);
}

Value native_fn_int_to_str(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *number_value = &values[0];
    char buff[20];

    if(!IS_VALUE_INT(number_value)){
        vmu_error(vm, "Expect integer, but got something else");
    }

	int64_t number = VALUE_TO_INT(number_value);
    int len = utils_i64_to_str(number, buff);
    char *raw_number_str = factory_clone_raw_str_range(0, len, buff, vm->fake_allocator);
    Obj *number_str_obj = vmu_str_obj(&raw_number_str, vm);

    return OBJ_VALUE(number_str_obj);
}

Value native_fn_str_to_float(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *raw_value = &values[0];
    Str *str = NULL;
    double value;

    if(!IS_VALUE_STR(raw_value)){
        vmu_error(vm, "Expect string, but got something else");
    }

    str = VALUE_TO_STR(raw_value);

    if(utils_str_to_double(str->buff, &value)){
        vmu_error(vm, "String do not contains a valid integer");
    }

	return FLOAT_VALUE(value);
}

Value native_fn_float_to_str(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *number_value = &values[0];
    size_t buff_len = 1024;
    char buff[buff_len];

    if(!IS_VALUE_FLOAT(number_value)){
        vmu_error(vm, "Expect float, but got something else");
    }

	double number = VALUE_TO_FLOAT(number_value);
    int len = utils_double_to_str(buff_len, number, buff);
    char *raw_number_str = factory_clone_raw_str_range(0, len, buff, vm->fake_allocator);
    Obj *number_str_obj = vmu_str_obj(&raw_number_str, vm);

    return OBJ_VALUE(number_str_obj);
}

Value native_fn_int_to_float(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_VALUE_INT(raw_value)){
        vmu_error(vm, "Expect integer, but got something else");
    }

    return FLOAT_VALUE((double)VALUE_TO_INT(raw_value));
}

Value native_fn_float_to_int(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_VALUE_FLOAT(raw_value)){
        vmu_error(vm, "Expect float, but got something else");
    }

    return INT_VALUE((uint64_t)VALUE_TO_FLOAT(raw_value));
}

Value native_fn_print(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *raw_value = &values[0];
    vmu_print_value(stdout, raw_value);
    return EMPTY_VALUE;
}

Value native_fn_println(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *raw_value = &values[0];
    vmu_print_value(stdout, raw_value);
    printf("\n");
    return EMPTY_VALUE;
}

Value native_fn_eprint(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *raw_value = &values[0];
    vmu_print_value(stderr, raw_value);
    return EMPTY_VALUE;
}

Value native_fn_eprintln(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *raw_value = &values[0];
    vmu_print_value(stderr, raw_value);
    fprintf(stderr, "\n");
    return EMPTY_VALUE;
}

Value native_fn_readln(uint8_t argsc, Value *values, Value *target, VM *vm){
	size_t buff_len = 1024;
	char buff[buff_len];
	char *out_buff = fgets(buff, buff_len, stdin);

	if(!out_buff){
        vmu_error(vm, "Failed to read input");
    }

    size_t out_buff_len = strlen(out_buff);

    char *raw_str = factory_clone_raw_str_range(0, out_buff_len - 1, buff, vm->fake_allocator);
    Obj *str_obj = vmu_str_obj(&raw_str, vm);

	return OBJ_VALUE(str_obj);
}

#endif
