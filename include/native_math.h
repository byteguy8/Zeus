#ifndef NATIVE_MATH_H
#define NATIVE_MATH_H

#include "types.h"
#include "value.h"
#include "vm_utils.h"
#include <math.h>

Value native_fn_sqrt(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_FLOAT(raw_value))
		vm_utils_error(vm, "Expect float, but got something else");

    return FLOAT_VALUE(sqrt(TO_FLOAT(raw_value)));
}

Value native_fn_pow(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *base_value = &values[0];
    Value *exp_value = &values[1];

    if(!IS_FLOAT(base_value))
		vm_utils_error(vm, "Expect float at argument 0 (base), but got something else");

    if(!IS_FLOAT(base_value))
		vm_utils_error(vm, "Expect float at argument 1 (exponent), but got something else");

    return FLOAT_VALUE(pow(TO_FLOAT(base_value), TO_FLOAT(exp_value)));
}

Value native_fn_cos(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_FLOAT(raw_value))
		vm_utils_error(vm, "Expect float, but got something else");

    return FLOAT_VALUE(cos(TO_FLOAT(raw_value)));
}

Value native_fn_cosh(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_FLOAT(raw_value))
		vm_utils_error(vm, "Expect float, but got something else");

    return FLOAT_VALUE(cosh(TO_FLOAT(raw_value)));
}

Value native_fn_sin(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_FLOAT(raw_value))
		vm_utils_error(vm, "Expect float, but got something else");

    return FLOAT_VALUE(sin(TO_FLOAT(raw_value)));
}

Value native_fn_sinh(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_FLOAT(raw_value))
		vm_utils_error(vm, "Expect float, but got something else");

    return FLOAT_VALUE(sinh(TO_FLOAT(raw_value)));
}

Value native_fn_tan(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_FLOAT(raw_value))
		vm_utils_error(vm, "Expect float, but got something else");

    return FLOAT_VALUE(tan(TO_FLOAT(raw_value)));
}

Value native_fn_tanh(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_FLOAT(raw_value))
		vm_utils_error(vm, "Expect float, but got something else");

    return FLOAT_VALUE(tanh(TO_FLOAT(raw_value)));
}

#endif