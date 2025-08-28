#ifndef NATIVE_MATH_H
#define NATIVE_MATH_H

#define PI 3.1415926535897932384626433

#include "vmu.h"
#include "tutils.h"
#include <math.h>

NativeModule *math_module = NULL;

Value native_fn_sqrt(uint8_t argsc, Value *values, Value *target, void *context){
    double value = validate_value_ifloat_arg(&values[0], 1, "value", VMU_VM);
    return FLOAT_VALUE(sqrt(value));
}

Value native_fn_pow(uint8_t argsc, Value *values, Value *target, void *context){
    double base = validate_value_ifloat_arg(&values[0], 1, "base", VMU_VM);
    double ex = validate_value_ifloat_arg(&values[1], 2, "exp", VMU_VM);
    return FLOAT_VALUE(pow(base, ex));
}

Value native_fn_cos(uint8_t argsc, Value *values, Value *target, void *context){
    double value = validate_value_ifloat_arg(&values[0], 1, "value", VMU_VM);
    return FLOAT_VALUE(cos(value));
}

Value native_fn_acos(uint8_t argsc, Value *values, Value *target, void *context){
    double value = validate_value_ifloat_arg(&values[0], 1, "value", VMU_VM);
    return FLOAT_VALUE(acos(value));
}

Value native_fn_cosh(uint8_t argsc, Value *values, Value *target, void *context){
    double value = validate_value_ifloat_arg(&values[0], 1, "value", VMU_VM);
    return FLOAT_VALUE(cosh(value));
}

Value native_fn_sin(uint8_t argsc, Value *values, Value *target, void *context){
    double value = validate_value_ifloat_arg(&values[0], 1, "value", VMU_VM);
    return FLOAT_VALUE(sin(value));
}

Value native_fn_asin(uint8_t argsc, Value *values, Value *target, void *context){
    double value = validate_value_ifloat_arg(&values[0], 1, "value", VMU_VM);
    return FLOAT_VALUE(asin(value));
}

Value native_fn_sinh(uint8_t argsc, Value *values, Value *target, void *context){
    double value = validate_value_ifloat_arg(&values[0], 1, "value", VMU_VM);
    return FLOAT_VALUE(sinh(value));
}

Value native_fn_tan(uint8_t argsc, Value *values, Value *target, void *context){
    double value = validate_value_ifloat_arg(&values[0], 1, "value", VMU_VM);
    return FLOAT_VALUE(tan(value));
}

Value native_fn_atan(uint8_t argsc, Value *values, Value *target, void *context){
    double value = validate_value_ifloat_arg(&values[0], 1, "value", VMU_VM);
    return FLOAT_VALUE(atan(value));
}

Value native_fn_tanh(uint8_t argsc, Value *values, Value *target, void *context){
    double value = validate_value_ifloat_arg(&values[0], 1, "value", VMU_VM);
    return FLOAT_VALUE(tanh(value));
}

// this function assumes the radio of the calculated radian value is of value 1
Value native_fn_rad2deg(uint8_t argsc, Value *values, Value *target, void *context){
    double rad = validate_value_ifloat_arg(&values[0], 1, "rad", VMU_VM);
    return FLOAT_VALUE(rad * (1.0 / (PI * 2.0 / 360.0)));
}

// this function assumes the radio of the calculated radian value is of value 1
Value native_fn_deg2rad(uint8_t argsc, Value *values, Value *target, void *context){
    double degrees = validate_value_ifloat_arg(&values[0], 1, "value", VMU_VM);
    return FLOAT_VALUE(degrees / (1.0 / (PI * 2.0 / 360.0)));
}

void math_module_init(Allocator *allocator){
	math_module = factory_create_native_module("math", allocator);

	factory_add_native_fn("sqrt", 1, native_fn_sqrt, math_module, allocator);
    factory_add_native_fn("pow", 2, native_fn_pow, math_module, allocator);
    factory_add_native_fn("cos", 1, native_fn_cos, math_module, allocator);
    factory_add_native_fn("acos", 1, native_fn_acos, math_module, allocator);
    factory_add_native_fn("cosh", 1, native_fn_cosh, math_module, allocator);
    factory_add_native_fn("sin", 1, native_fn_sin, math_module, allocator);
    factory_add_native_fn("asin", 1, native_fn_asin, math_module, allocator);
    factory_add_native_fn("sinh", 1, native_fn_sinh, math_module, allocator);
    factory_add_native_fn("tan", 1, native_fn_tan, math_module, allocator);
    factory_add_native_fn("atan", 1, native_fn_atan, math_module, allocator);
    factory_add_native_fn("tanh", 1, native_fn_tanh, math_module, allocator);
    factory_add_native_fn("rad2deg", 1, native_fn_rad2deg, math_module, allocator);
    factory_add_native_fn("deg2rad", 1, native_fn_deg2rad, math_module, allocator);
}

#endif
