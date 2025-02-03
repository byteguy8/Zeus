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

Value native_fn_acos(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_FLOAT(raw_value))
		vm_utils_error(vm, "Expect float, but got something else");

    return FLOAT_VALUE(acos(TO_FLOAT(raw_value)));
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

Value native_fn_asin(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_FLOAT(raw_value))
		vm_utils_error(vm, "Expect float, but got something else");

    return FLOAT_VALUE(asin(TO_FLOAT(raw_value)));
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

Value native_fn_atan(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_FLOAT(raw_value))
		vm_utils_error(vm, "Expect float, but got something else");

    return FLOAT_VALUE(atan(TO_FLOAT(raw_value)));
}

Value native_fn_tanh(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_FLOAT(raw_value))
		vm_utils_error(vm, "Expect float, but got something else");

    return FLOAT_VALUE(tanh(TO_FLOAT(raw_value)));
}

// this function assumes the radio of the calculated radian value is of value 1
Value native_fn_rad2deg(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_FLOAT(raw_value))
		vm_utils_error(vm, "Expect float, but got something else");

	double rad = TO_FLOAT(raw_value);

    return FLOAT_VALUE(rad * (1.0 / (M_PI * 2.0 / 360.0)));
}

// this function assumes the radio of the calculated radian value is of value 1
Value native_fn_deg2rad(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_FLOAT(raw_value))
		vm_utils_error(vm, "Expect float, but got something else");

	double degrees = TO_FLOAT(raw_value);

    return FLOAT_VALUE(degrees / (1.0 / (M_PI * 2.0 / 360.0)));
}

#endif
