#ifndef NATIVE_H
#define NATIVE_H

#include "tutils.h"

static void ls_native_module(NativeModule *native_module, ArrayObj *symbols, Allocator *allocator){
    LZHTable *raw_symbols = native_module->symbols;
    aidx_t i = 0;

    for (LZHTableNode *current = raw_symbols->head; current; current = current->next_table_node){
        NativeModuleSymbol *symbol = (NativeModuleSymbol *)current->value;

        if(symbol->type == NATIVE_FUNCTION_NMSYMTYPE){
            NativeFn *native_fn = symbol->value.native_fn;
            NativeFnObj *native_fn_obj = MEMORY_ALLOC(NativeFnObj, 1, allocator);

            native_fn_obj->header.type = NATIVE_FN_OTYPE;
            native_fn_obj->native_fn = native_fn;

            symbols->values[i++] = OBJ_VALUE(&native_fn_obj->header);
        }
    }
}

static void ls_module(Module *module, DynArr *symbols, Allocator *allocator){
    LZHTable *globals = module->submodule->globals;

    for (LZHTableNode *current = globals->head; current; current = current->next_table_node){
        GlobalValue *global_value = (GlobalValue *)current->value;

        if(global_value->access == PRIVATE_GVATYPE || !IS_VALUE_FN(global_value->value)){
            continue;
        }

        FnObj *fn_obj = VALUE_TO_FN(global_value->value);

        dynarr_insert(&OBJ_VALUE(fn_obj), symbols);
    }
}

Value native_fn_print_stack(uint8_t argsc, Value *values, Value *target, void *context){
    VM *vm = (VM *)context;

    for (Value *current = vm->stack; current < vm->stack_top; current++){
        vmu_print_value(stdout, current);
        fprintf(stdout, "\n");
    }

    return EMPTY_VALUE;
}

Value native_fn_ls(uint8_t argsc, Value *values, Value *target, void *context){
    Value *module_value = &values[0];

    VM *vm = (VM *)context;
    ObjHeader *array_obj_header = NULL;

    if(IS_VALUE_NATIVE_MODULE(module_value)){
        NativeModuleObj *native_module_obj = VALUE_TO_NATIVE_MODULE(module_value);
        NativeModule *native_module = native_module_obj->native_module;
        LZHTable *raw_symbols = native_module->symbols;
        size_t symbols_len = raw_symbols->n;

        VALIDATE_ARRAY_SIZE(symbols_len, vm)

        array_obj_header = vmu_create_array_obj((aidx_t)symbols_len, vm);
        ArrayObj *array_obj = OBJ_TO_ARRAY(array_obj_header);

        ls_native_module(native_module, array_obj, vm->fake_allocator);
    }

    if(IS_VALUE_MODULE(module_value)){
        ModuleObj *module_obj = VALUE_TO_MODULE(module_value);
        Module *module = module_obj->module;
        DynArr *raw_symbols = MODULE_SYMBOLS(module);
        size_t symbols_len = DYNARR_LEN(raw_symbols);

        VALIDATE_ARRAY_SIZE(symbols_len, vm)

        DynArr *temp_symbols = FACTORY_DYNARR_TYPE(Value, vm->allocator);

        ls_module(module, temp_symbols, vm->fake_allocator);

        array_obj_header = vmu_create_array_obj((aidx_t)DYNARR_LEN(temp_symbols), vm);
        ArrayObj *array_obj = OBJ_TO_ARRAY(array_obj_header);

        memcpy(array_obj->values, temp_symbols->items, VALUE_SIZE * DYNARR_LEN(temp_symbols));

        dynarr_destroy(temp_symbols);
    }

	return OBJ_VALUE(array_obj_header);
}

Value native_fn_exit(uint8_t argsc, Value *values, Value *target, void *context){
	Value *exit_code_value = &values[0];

    VALIDATE_VALUE_INT_ARG(exit_code_value, 1, "exit code", context);

    int64_t exit_code = VALUE_TO_INT(exit_code_value);
    VM *vm = (VM *)context;

    vm->halt = 1;
    vm->exit_code = (unsigned char)exit_code;

	return EMPTY_VALUE;
}

Value native_fn_assert(uint8_t argsc, Value *values, Value *target, void *context){
	Value *raw_value = &values[0];

    VALIDATE_VALUE_BOOL_ARG(raw_value, 1, "value", context)
    uint8_t value = VALUE_TO_BOOL(raw_value);

    if(!value){
        vmu_error(context, "Assertion failed");
    }

	return EMPTY_VALUE;
}

Value native_fn_assertm(uint8_t argsc, Value *values, Value *target, void *context){
	Value *raw_value = &values[0];
    Value *msg_value = &values[1];

    VALIDATE_VALUE_BOOL_ARG(raw_value, 1, "value", context)
    VALIDATE_VALUE_STR_ARG(msg_value, 2, "message", context)

    uint8_t value = VALUE_TO_BOOL(raw_value);
    StrObj *msg = VALUE_TO_STR(msg_value);

    if(!value){
        vmu_error(context, "Assertion failed: %s", msg->buff);
    }

	return EMPTY_VALUE;
}

Value native_fn_is_str_int(uint8_t argsc, Value *values, Value *target, void *context){
	Value *raw_value = &values[0];
	StrObj *str = NULL;

    VALIDATE_VALUE_STR_ARG(raw_value, 1, "value", context)
	str = VALUE_TO_STR(raw_value);

	return BOOL_VALUE((uint8_t)utils_is_integer(str->buff));
}

Value native_fn_is_str_float(uint8_t argsc, Value *values, Value *target, void *context){
	Value *raw_value = &values[0];
	StrObj *str = NULL;

    VALIDATE_VALUE_STR_ARG(raw_value, 1, "value", context)
	str = VALUE_TO_STR(raw_value);

	return BOOL_VALUE((uint8_t)utils_is_float(str->buff));
}

Value native_fn_str_to_int(uint8_t argsc, Value *values, Value *target, void *context){
	Value *raw_value = &values[0];
	StrObj *str = NULL;
    int64_t value;

    VALIDATE_VALUE_STR_ARG(raw_value, 1, "value", context)
	str = VALUE_TO_STR(raw_value);

	if(utils_str_to_i64(str->buff, &value)){
        vmu_error(context, "String do not contains a valid integer");
    }

	return INT_VALUE(value);
}

Value native_fn_int_to_str(uint8_t argsc, Value *values, Value *target, void *context){
    Value *number_value = &values[0];
    char buff[21] = {0};

    VALIDATE_VALUE_INT_ARG(number_value, 1, "value", context)

	int64_t number = VALUE_TO_INT(number_value);
    int len = utils_i64_to_str(number, buff);
    char *raw_number_str = factory_clone_raw_str_range(0, len, buff, ((VM *)context)->fake_allocator);
    ObjHeader *number_str_obj = vmu_create_str_obj(&raw_number_str, context);

    return OBJ_VALUE(number_str_obj);
}

Value native_fn_str_to_float(uint8_t argsc, Value *values, Value *target, void *context){
    Value *raw_value = &values[0];
    StrObj *str = NULL;
    double value;

    VALIDATE_VALUE_STR_ARG(raw_value, 1, "value", context)
    str = VALUE_TO_STR(raw_value);

    if(utils_str_to_double(str->buff, &value)){
        vmu_error(context, "String do not contains a valid float");
    }

	return FLOAT_VALUE(value);
}

Value native_fn_float_to_str(uint8_t argsc, Value *values, Value *target, void *context){
    Value *number_value = &values[0];
    size_t buff_len = 1024;
    char buff[buff_len];

    VALIDATE_VALUE_FLOAT_ARG(number_value, 1, "value", context)

	double number = VALUE_TO_FLOAT(number_value);
    int len = utils_double_to_str(buff_len, number, buff);
    char *raw_number_str = factory_clone_raw_str_range(0, len, buff, ((VM *)context)->fake_allocator);
    ObjHeader *number_str_obj = vmu_create_str_obj(&raw_number_str, context);

    return OBJ_VALUE(number_str_obj);
}

Value native_fn_int_to_float(uint8_t argsc, Value *values, Value *target, void *context){
    Value *raw_value = &values[0];

    VALIDATE_VALUE_INT_ARG(raw_value, 1, "value", context)

    return FLOAT_VALUE((double)VALUE_TO_INT(raw_value));
}

Value native_fn_float_to_int(uint8_t argsc, Value *values, Value *target, void *context){
    Value *raw_value = &values[0];

    VALIDATE_VALUE_FLOAT_ARG(raw_value, 1, "value", context)

    return INT_VALUE((uint64_t)VALUE_TO_FLOAT(raw_value));
}

Value native_fn_print(uint8_t argsc, Value *values, Value *target, void *context){
    Value *raw_value = &values[0];
    vmu_print_value(stdout, raw_value);
    return EMPTY_VALUE;
}

Value native_fn_println(uint8_t argsc, Value *values, Value *target, void *context){
    Value *raw_value = &values[0];
    vmu_print_value(stdout, raw_value);
    printf("\n");
    return EMPTY_VALUE;
}

Value native_fn_eprint(uint8_t argsc, Value *values, Value *target, void *context){
    Value *raw_value = &values[0];
    vmu_print_value(stderr, raw_value);
    return EMPTY_VALUE;
}

Value native_fn_eprintln(uint8_t argsc, Value *values, Value *target, void *context){
    Value *raw_value = &values[0];
    vmu_print_value(stderr, raw_value);
    fprintf(stderr, "\n");
    return EMPTY_VALUE;
}

Value native_fn_readln(uint8_t argsc, Value *values, Value *target, void *context){
    VM *vm = (VM *)context;
    BStr *bstr = FACTORY_BSTR(vm->allocator);

    while (1){
        int character = fgetc(stdin);

        if(character == EOF){
            break;
        }

        char value[] = {character, 0};

        bstr_append(value, bstr);

        if(character == '\n'){
            break;
        }
    }

    ObjHeader *str_obj_header = vmu_create_str_cpy_obj((char *)bstr->buff, vm);

    bstr_destroy(bstr);

	return OBJ_VALUE(str_obj_header);
}

Value native_fn_gc(uint8_t argsc, Value *values, Value *target, void *context){
    vmu_gc(context);
    return EMPTY_VALUE;
}

#endif