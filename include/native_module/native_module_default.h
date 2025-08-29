#ifndef NATIVE_H
#define NATIVE_H

#include "tutils.h"

Value native_fn_print_stack(uint8_t argsc, Value *values, Value *target, void *context){
    VM *vm = (VM *)context;

    for(Value *current = vm->stack; current < vm->stack_top; current++){
        vmu_print_value(stdout, current);
        fprintf(stdout, "\n");
    }

    return EMPTY_VALUE;
}

Value native_fn_exit(uint8_t argsc, Value *values, Value *target, void *context){
    VM *vm = (VM *)context;
    int64_t exit_code = validate_value_int_range_arg(&values[0], 1, "exit code", 0, 255, vm);

    vm->halt = 1;
    vm->exit_code = (unsigned char)exit_code;

	return EMPTY_VALUE;
}

Value native_fn_assert(uint8_t argsc, Value *values, Value *target, void *context){
    uint8_t value = validate_value_bool_arg(&values[0], 1, "assertion", VMU_VM);

    if(!value){
        vmu_error(VMU_VM, "ASSERTION FAILED");
    }

	return EMPTY_VALUE;
}

Value native_fn_assertm(uint8_t argsc, Value *values, Value *target, void *context){
    uint8_t value = validate_value_bool_arg(&values[0], 1, "assertion", VMU_VM);
    StrObj *str_obj = validate_value_str_arg(&values[1], 2, "message", VMU_VM);

    if(!value){
        vmu_error(VMU_VM, "%s", str_obj->buff);
    }

	return EMPTY_VALUE;
}

Value native_fn_is_str_int(uint8_t argsc, Value *values, Value *target, void *context){
    StrObj *str_obj = validate_value_str_arg(&values[0], 1, "string", VMU_VM);
    return BOOL_VALUE((uint8_t)vmu_str_is_int(str_obj));
}

Value native_fn_is_str_float(uint8_t argsc, Value *values, Value *target, void *context){
    StrObj *str_obj = validate_value_str_arg(&values[0], 1, "string", VMU_VM);
    return BOOL_VALUE((uint8_t)vmu_str_is_float(str_obj));
}

Value native_fn_to_str(uint8_t argsc, Value *values, Value *target, void *context){
    size_t len;
    char *raw_str = vmu_value_to_str(values[0], VMU_VM, &len);
    StrObj *str_obj = NULL;

    if(vmu_create_str(1, len, raw_str, VMU_VM, &str_obj)){
        MEMORY_DEALLOC(char, len + 1, raw_str, &(VMU_VM->fake_allocator));
    }

    return OBJ_VALUE(str_obj);
}

Value native_fn_to_int(uint8_t argsc, Value *values, Value *target, void *context){
    Value *raw_value = &values[0];

    if(IS_VALUE_BOOL(raw_value)){
        return INT_VALUE((int64_t)VALUE_TO_BOOL(raw_value));
    }

    if(IS_VALUE_INT(raw_value)){
        return *raw_value;
    }

    if(IS_VALUE_FLOAT(raw_value)){
        return INT_VALUE((int64_t)VALUE_TO_FLOAT(raw_value));
    }

    if(IS_VALUE_STR(raw_value)){
        int64_t value;
        StrObj *str_obj = VALUE_TO_STR(raw_value);

        if(!vmu_str_is_int(str_obj)){
            vmu_error(VMU_VM, "Failed to parse 'str' to 'int': contains not valid digits");
        }

        utils_decimal_str_to_i64(str_obj->buff, &value);

        return INT_VALUE(value);
    }

    vmu_error(VMU_VM, "Failed to parse to 'int': unsupported type");

    return EMPTY_VALUE;
}

Value native_fn_to_float(uint8_t argsc, Value *values, Value *target, void *context){
    Value *raw_value = &values[0];

    if(IS_VALUE_INT(raw_value)){
        return FLOAT_VALUE((double)VALUE_TO_INT(raw_value));
    }

    if(IS_VALUE_FLOAT(raw_value)){
        return *raw_value;
    }

    if(IS_VALUE_STR(raw_value)){
        double value;
        StrObj *str_obj = VALUE_TO_STR(raw_value);

        if(!vmu_str_is_float(str_obj)){
            vmu_error(VMU_VM, "Failed to parse 'str' to 'float': malformed float string");
        }

        utils_str_to_double(str_obj->buff, &value);

        return FLOAT_VALUE(value);
    }

    vmu_error(VMU_VM, "Failed to parse to 'int': unsupported type");

    return EMPTY_VALUE;
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
    return EMPTY_VALUE;
}

Value native_fn_gc(uint8_t argsc, Value *values, Value *target, void *context){
    vmu_gc(context);
    return EMPTY_VALUE;
}

Value native_fn_halt(uint8_t argsc, Value *values, Value *target, void *context){
    VM *vm = VMU_VM;
    longjmp(vm->exit_jmp, 1);
}

#endif