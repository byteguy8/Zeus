#ifndef NATIVE_MATH_H
#define NATIVE_MATH_H

#define PI 3.1415926535897932384626433
#define E  2.7182818284590452353602874

#include "vm/types_utils.h"
#include "vm/vm_factory.h"
#include "vm/vmu.h"

#include <math.h>

NativeModule *math_native_module = NULL;

static inline int64_t calc_min(int64_t x, int64_t y){
    return y ^ ((x ^ y) & -(x < y));
}

static inline int64_t calc_max(int64_t x, int64_t y){
    return x ^ ((x ^ y) & -(x < y));
}

Value native_fn_min(uint8_t argsc, Value *values, Value target, void *context){
    Value left_value = values[0];
    Value right_value = values[1];

    if(!vm_is_value_numeric(left_value) || !vm_is_value_numeric(right_value)){
        vmu_error(VMU_VM, "Expect both arguments of type of type 'int' and/or 'float'");
    }

    if(IS_VALUE_FLOAT(left_value) || IS_VALUE_FLOAT(right_value)){
        double left;
        double right;

        if(IS_VALUE_FLOAT(left_value)){
            left = VALUE_TO_FLOAT(left_value);
            right = (double)VALUE_TO_INT(right_value);
        }else{
            left = (double)VALUE_TO_INT(left_value);
            right = VALUE_TO_FLOAT(right_value);
        }

        return FLOAT_VALUE(fmin(left, right));
    }

    int64_t left = VALUE_TO_INT(left_value);
    int64_t right = VALUE_TO_INT(right_value);

    return INT_VALUE(calc_min(left, right));
}

Value native_fn_max(uint8_t argsc, Value *values, Value target, void *context){
    Value left_value = values[0];
    Value right_value = values[1];

    if(!vm_is_value_numeric(left_value) || !vm_is_value_numeric(right_value)){
        vmu_error(VMU_VM, "Expect both arguments of type of type 'int' and/or 'float'");
    }

    if(IS_VALUE_FLOAT(left_value) || IS_VALUE_FLOAT(right_value)){
        double left;
        double right;

        if(IS_VALUE_FLOAT(left_value)){
            left = VALUE_TO_FLOAT(left_value);
            right = (double)VALUE_TO_INT(right_value);
        }else{
            left = (double)VALUE_TO_INT(left_value);
            right = VALUE_TO_FLOAT(right_value);
        }

        return FLOAT_VALUE(fmax(left, right));
    }

    int64_t left = VALUE_TO_INT(left_value);
    int64_t right = VALUE_TO_INT(right_value);

    return INT_VALUE(calc_max(left, right));
}

Value native_fn_sqrt(uint8_t argsc, Value *values, Value target, void *context){
    double value = validate_value_ifloat_arg(values[0], 1, "value", VMU_VM);
    return FLOAT_VALUE(sqrt(value));
}

Value native_fn_pow(uint8_t argsc, Value *values, Value target, void *context){
    double base = validate_value_ifloat_arg(values[0], 1, "base", VMU_VM);
    double ex = validate_value_ifloat_arg(values[1], 2, "exp", VMU_VM);
    return FLOAT_VALUE(pow(base, ex));
}

Value native_fn_cos(uint8_t argsc, Value *values, Value target, void *context){
    double value = validate_value_ifloat_arg(values[0], 1, "value", VMU_VM);
    return FLOAT_VALUE(cos(value));
}

Value native_fn_acos(uint8_t argsc, Value *values, Value target, void *context){
    double value = validate_value_ifloat_arg(values[0], 1, "value", VMU_VM);
    return FLOAT_VALUE(acos(value));
}

Value native_fn_cosh(uint8_t argsc, Value *values, Value target, void *context){
    double value = validate_value_ifloat_arg(values[0], 1, "value", VMU_VM);
    return FLOAT_VALUE(cosh(value));
}

Value native_fn_sin(uint8_t argsc, Value *values, Value target, void *context){
    double value = validate_value_ifloat_arg(values[0], 1, "value", VMU_VM);
    return FLOAT_VALUE(sin(value));
}

Value native_fn_asin(uint8_t argsc, Value *values, Value target, void *context){
    double value = validate_value_ifloat_arg(values[0], 1, "value", VMU_VM);
    return FLOAT_VALUE(asin(value));
}

Value native_fn_sinh(uint8_t argsc, Value *values, Value target, void *context){
    double value = validate_value_ifloat_arg(values[0], 1, "value", VMU_VM);
    return FLOAT_VALUE(sinh(value));
}

Value native_fn_tan(uint8_t argsc, Value *values, Value target, void *context){
    double value = validate_value_ifloat_arg(values[0], 1, "value", VMU_VM);
    return FLOAT_VALUE(tan(value));
}

Value native_fn_atan(uint8_t argsc, Value *values, Value target, void *context){
    double value = validate_value_ifloat_arg(values[0], 1, "value", VMU_VM);
    return FLOAT_VALUE(atan(value));
}

Value native_fn_tanh(uint8_t argsc, Value *values, Value target, void *context){
    double value = validate_value_ifloat_arg(values[0], 1, "value", VMU_VM);
    return FLOAT_VALUE(tanh(value));
}

// this function assumes the radio of the calculated radian value is of value 1
Value native_fn_rad2deg(uint8_t argsc, Value *values, Value target, void *context){
    double rad = validate_value_ifloat_arg(values[0], 1, "rad", VMU_VM);
    return FLOAT_VALUE(rad * (1.0 / (PI * 2.0 / 360.0)));
}

// this function assumes the radio of the calculated radian value is of value 1
Value native_fn_deg2rad(uint8_t argsc, Value *values, Value target, void *context){
    double degrees = validate_value_ifloat_arg(values[0], 1, "value", VMU_VM);
    return FLOAT_VALUE(degrees / (1.0 / (PI * 2.0 / 360.0)));
}

void math_module_init(const Allocator *allocator){
	math_native_module = vm_factory_native_module_create(allocator, "math");

    vm_factory_native_module_add_value(math_native_module, "PI", FLOAT_VALUE(PI));
    vm_factory_native_module_add_value(math_native_module, "E", FLOAT_VALUE(E));

    vm_factory_native_module_add_native_fn(math_native_module, "min", 2, native_fn_min);
    vm_factory_native_module_add_native_fn(math_native_module, "max", 2, native_fn_max);
	vm_factory_native_module_add_native_fn(math_native_module, "sqrt", 1, native_fn_sqrt);
    vm_factory_native_module_add_native_fn(math_native_module, "pow", 2, native_fn_pow);
    vm_factory_native_module_add_native_fn(math_native_module, "cos", 1, native_fn_cos);
    vm_factory_native_module_add_native_fn(math_native_module, "acos", 1, native_fn_acos);
    vm_factory_native_module_add_native_fn(math_native_module, "cosh", 1, native_fn_cosh);
    vm_factory_native_module_add_native_fn(math_native_module, "sin", 1, native_fn_sin);
    vm_factory_native_module_add_native_fn(math_native_module, "asin", 1, native_fn_asin);
    vm_factory_native_module_add_native_fn(math_native_module, "sinh", 1, native_fn_sinh);
    vm_factory_native_module_add_native_fn(math_native_module, "tan", 1, native_fn_tan);
    vm_factory_native_module_add_native_fn(math_native_module, "atan", 1, native_fn_atan);
    vm_factory_native_module_add_native_fn(math_native_module, "tanh", 1, native_fn_tanh);
    vm_factory_native_module_add_native_fn(math_native_module, "rad2deg", 1, native_fn_rad2deg);
    vm_factory_native_module_add_native_fn(math_native_module, "deg2rad", 1, native_fn_deg2rad);
}

#endif
