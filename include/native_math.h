#ifndef NATIVE_MATH_H
#define NATIVE_MATH_H

#include "types.h"
#include "value.h"
#include "vm_utils.h"
#include <math.h>

NativeModule *math_module = NULL;

Value native_fn_sqrt(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_FLOAT(raw_value))
		vmu_error(vm, "Expect float, but got something else");

    return FLOAT_VALUE(sqrt(TO_FLOAT(raw_value)));
}

Value native_fn_pow(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *base_value = &values[0];
    Value *exp_value = &values[1];

    if(!IS_FLOAT(base_value))
		vmu_error(vm, "Expect float at argument 0 (base), but got something else");

    if(!IS_FLOAT(base_value))
		vmu_error(vm, "Expect float at argument 1 (exponent), but got something else");

    return FLOAT_VALUE(pow(TO_FLOAT(base_value), TO_FLOAT(exp_value)));
}

Value native_fn_cos(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_FLOAT(raw_value))
		vmu_error(vm, "Expect float, but got something else");

    return FLOAT_VALUE(cos(TO_FLOAT(raw_value)));
}

Value native_fn_acos(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_FLOAT(raw_value))
		vmu_error(vm, "Expect float, but got something else");

    return FLOAT_VALUE(acos(TO_FLOAT(raw_value)));
}

Value native_fn_cosh(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_FLOAT(raw_value))
		vmu_error(vm, "Expect float, but got something else");

    return FLOAT_VALUE(cosh(TO_FLOAT(raw_value)));
}

Value native_fn_sin(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_FLOAT(raw_value))
		vmu_error(vm, "Expect float, but got something else");

    return FLOAT_VALUE(sin(TO_FLOAT(raw_value)));
}

Value native_fn_asin(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_FLOAT(raw_value))
		vmu_error(vm, "Expect float, but got something else");

    return FLOAT_VALUE(asin(TO_FLOAT(raw_value)));
}

Value native_fn_sinh(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_FLOAT(raw_value))
		vmu_error(vm, "Expect float, but got something else");

    return FLOAT_VALUE(sinh(TO_FLOAT(raw_value)));
}

Value native_fn_tan(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_FLOAT(raw_value))
		vmu_error(vm, "Expect float, but got something else");

    return FLOAT_VALUE(tan(TO_FLOAT(raw_value)));
}

Value native_fn_atan(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_FLOAT(raw_value))
		vmu_error(vm, "Expect float, but got something else");

    return FLOAT_VALUE(atan(TO_FLOAT(raw_value)));
}

Value native_fn_tanh(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_FLOAT(raw_value))
		vmu_error(vm, "Expect float, but got something else");

    return FLOAT_VALUE(tanh(TO_FLOAT(raw_value)));
}

// this function assumes the radio of the calculated radian value is of value 1
Value native_fn_rad2deg(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_FLOAT(raw_value))
		vmu_error(vm, "Expect float, but got something else");

	double rad = TO_FLOAT(raw_value);

    return FLOAT_VALUE(rad * (1.0 / (M_PI * 2.0 / 360.0)));
}

// this function assumes the radio of the calculated radian value is of value 1
Value native_fn_deg2rad(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_FLOAT(raw_value))
		vmu_error(vm, "Expect float, but got something else");

	double degrees = TO_FLOAT(raw_value);

    return FLOAT_VALUE(degrees / (1.0 / (M_PI * 2.0 / 360.0)));
}

void math_module_init(){
	  math_module = runtime_native_module("math");
	
	runtime_add_native_fn("sqrt", 1, native_fn_sqrt, math_module);
    runtime_add_native_fn("pow", 2, native_fn_pow, math_module);
    runtime_add_native_fn("cos", 1, native_fn_cos, math_module);
    runtime_add_native_fn("acos", 1, native_fn_acos, math_module);
    runtime_add_native_fn("cosh", 1, native_fn_cosh, math_module);
    runtime_add_native_fn("sin", 1, native_fn_sin, math_module);
    runtime_add_native_fn("asin", 1, native_fn_asin, math_module);
    runtime_add_native_fn("sinh", 1, native_fn_sinh, math_module);
    runtime_add_native_fn("tan", 1, native_fn_tan, math_module);
    runtime_add_native_fn("atan", 1, native_fn_atan, math_module);
    runtime_add_native_fn("tanh", 1, native_fn_tanh, math_module);
    runtime_add_native_fn("rad2deg", 1, native_fn_rad2deg, math_module);
    runtime_add_native_fn("deg2rad", 1, native_fn_deg2rad, math_module);
}

#endif
