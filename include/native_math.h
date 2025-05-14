#ifndef NATIVE_MATH_H
#define NATIVE_MATH_H

#include "vmu.h"
#include "tutils.h"
#include <math.h>

#define PI 3.1415926535897932384626433

NativeModule *math_module = NULL;

static void populate(const aidx_t len, int64_t *numbers){
    for(aidx_t i = 0, num = 2; i < len; i++, num++){
        numbers[i] = num;
    }
}

static size_t sieve_mark_multiples(aidx_t at, aidx_t len, int64_t *numbers, aidx_t *out_mark_count){
    aidx_t next_at = 0;
    aidx_t mark_count = 0;
    int64_t number;
    int64_t at_number = numbers[at];

    for(aidx_t i = at + 1; i < len; i++){
        number = numbers[i];

        if(number == -1){
            continue;
        }

        if(number % at_number == 0){
            numbers[i] = -1;
            mark_count++;
        }else{
            if(next_at == 0){
                next_at = i;
            }
        }
    }

    *out_mark_count = mark_count;

    return next_at;
}

static Obj *sieve_eratosthenes(const int64_t until, VM *vm){
    aidx_t len = until - 1;
    int64_t numbers[len];

    populate(len, numbers);

    aidx_t next = 0;
    aidx_t mark_count = 0;
    aidx_t mark_count_total = 0;

    while(1){
        next = sieve_mark_multiples(next, len, numbers, &mark_count);

        if(next == 0){
            break;
        }

        mark_count_total += mark_count;
    }

    int64_t number;
    aidx_t primes_len = len - mark_count_total;
    Obj *array_obj = vmu_array_obj(primes_len, vm);
    Array *array = OBJ_TO_ARRAY(array_obj);

    for(aidx_t i = 0, o = 0; i < len; i++){
        number = numbers[i];

        if(number == -1){
            continue;
        }

        array->values[o++] = INT_VALUE(number);
    }

    return array_obj;
}

Value native_fn_primes(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *until_value = &values[0];

    VALIDATE_VALUE_INT_ARG(until_value, 1, "until", vm)

    int64_t until = VALUE_TO_INT(until_value);

    VALIDATE_VALUE_INT_RANGE_ARG(until_value, 0, "until", 2, ARRAY_LENGTH_MAX, vm)

    Obj *array_obj = sieve_eratosthenes(until, vm);

    return OBJ_VALUE(array_obj);
}

Value native_fn_sqrt(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_VALUE_FLOAT(raw_value)){
        vmu_error(vm, "Expect float, but got something else");
    }

    return FLOAT_VALUE(sqrt(VALUE_TO_FLOAT(raw_value)));
}

Value native_fn_pow(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *base_value = &values[0];
    Value *exp_value = &values[1];

    if(!IS_VALUE_FLOAT(base_value)){
        vmu_error(vm, "Expect float at argument 0 (base), but got something else");
    }
    if(!IS_VALUE_FLOAT(base_value)){
        vmu_error(vm, "Expect float at argument 1 (exponent), but got something else");
    }

    return FLOAT_VALUE(pow(VALUE_TO_FLOAT(base_value), VALUE_TO_FLOAT(exp_value)));
}

Value native_fn_cos(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_VALUE_FLOAT(raw_value)){
        vmu_error(vm, "Expect float, but got something else");
    }

    return FLOAT_VALUE(cos(VALUE_TO_FLOAT(raw_value)));
}

Value native_fn_acos(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_VALUE_FLOAT(raw_value)){
        vmu_error(vm, "Expect float, but got something else");
    }

    return FLOAT_VALUE(acos(VALUE_TO_FLOAT(raw_value)));
}

Value native_fn_cosh(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_VALUE_FLOAT(raw_value)){
        vmu_error(vm, "Expect float, but got something else");
    }

    return FLOAT_VALUE(cosh(VALUE_TO_FLOAT(raw_value)));
}

Value native_fn_sin(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_VALUE_FLOAT(raw_value)){
        vmu_error(vm, "Expect float, but got something else");
    }

    return FLOAT_VALUE(sin(VALUE_TO_FLOAT(raw_value)));
}

Value native_fn_asin(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_VALUE_FLOAT(raw_value)){
        vmu_error(vm, "Expect float, but got something else");
    }

    return FLOAT_VALUE(asin(VALUE_TO_FLOAT(raw_value)));
}

Value native_fn_sinh(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_VALUE_FLOAT(raw_value)){
        vmu_error(vm, "Expect float, but got something else");
    }

    return FLOAT_VALUE(sinh(VALUE_TO_FLOAT(raw_value)));
}

Value native_fn_tan(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_VALUE_FLOAT(raw_value)){
        vmu_error(vm, "Expect float, but got something else");
    }

    return FLOAT_VALUE(tan(VALUE_TO_FLOAT(raw_value)));
}

Value native_fn_atan(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_VALUE_FLOAT(raw_value)){
        vmu_error(vm, "Expect float, but got something else");
    }

    return FLOAT_VALUE(atan(VALUE_TO_FLOAT(raw_value)));
}

Value native_fn_tanh(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_VALUE_FLOAT(raw_value)){
        vmu_error(vm, "Expect float, but got something else");
    }

    return FLOAT_VALUE(tanh(VALUE_TO_FLOAT(raw_value)));
}

// this function assumes the radio of the calculated radian value is of value 1
Value native_fn_rad2deg(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_VALUE_FLOAT(raw_value)){
        vmu_error(vm, "Expect float, but got something else");
    }

	double rad = VALUE_TO_FLOAT(raw_value);

    return FLOAT_VALUE(rad * (1.0 / (PI * 2.0 / 360.0)));
}

// this function assumes the radio of the calculated radian value is of value 1
Value native_fn_deg2rad(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *raw_value = &values[0];

    if(!IS_VALUE_FLOAT(raw_value)){
        vmu_error(vm, "Expect float, but got something else");
    }

	double degrees = VALUE_TO_FLOAT(raw_value);

    return FLOAT_VALUE(degrees / (1.0 / (PI * 2.0 / 360.0)));
}

void math_module_init(Allocator *allocator){
	math_module = factory_native_module("math", allocator);

    factory_add_native_fn("primes", 1, native_fn_primes, math_module, allocator);
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
