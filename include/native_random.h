#ifndef NATIVE_RANDOM_H
#define NATIVE_RANDOM_H

#include "types.h"
#include "lzhtable.h"
#include "factory.h"
#include "vmu.h"
#include "tutils.h"
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

static int seeded = 0;
NativeModule *random_module = NULL;

#define INIT_RANDOM() {    \
    if(!seeded){           \
        srand(time(NULL)); \
        seeded = 1;        \
    }                      \
}

Value native_fn_random_init(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *seed_value = &values[0];

    VALIDATE_VALUE_INT_ARG(seed_value, 1, "seed", vm)

    int64_t seed = VALUE_TO_INT(seed_value);

    srand((unsigned int)seed);

    if(!seeded){
        seeded = 1;
    }

    return EMPTY_VALUE;
}

Value native_fn_random_next_int(uint8_t argsc, Value *values, Value *target, VM *vm){
    INIT_RANDOM()
    return INT_VALUE((int64_t)rand());
}

Value native_fn_random_next_int_range(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *min_value = &values[0];
    Value *max_value = &values[1];

    VALIDATE_VALUE_INT_ARG(min_value, 1, "min", vm)
    VALIDATE_VALUE_INT_ARG(max_value, 1, "max", vm)

    INIT_RANDOM()

    int min = (int)VALUE_TO_INT(min_value);
    int max = (int)VALUE_TO_INT(max_value);
    int random = rand() % (max - min + 1) + min;

    return INT_VALUE((int64_t)random);
}

Value native_fn_random_next_ints(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *length_value = &values[0];

    VALIDATE_VALUE_INT_ARG(length_value, 1, "length", vm)
    VALIDATE_ARRAY_INDEX_ARG(1, "length", length_value, vm)

    int64_t length = VALUE_TO_INT(length_value);
    Obj *values_array_obj = vmu_array_obj(length, vm);
    Array *values_array = OBJ_TO_ARRAY(values_array_obj);

    INIT_RANDOM()

    for(int32_t i = 0; i < length; i++){
        values_array->values[i] = INT_VALUE((int64_t)rand());
    }

    return OBJ_VALUE(values_array_obj);
}

Value native_fn_random_next_ints_range(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *min_value = &values[0];
    Value *max_value = &values[1];
    Value *length_value = &values[2];

    VALIDATE_VALUE_INT_ARG(min_value, 1, "min", vm)
    VALIDATE_VALUE_INT_ARG(max_value, 2, "max", vm)
    VALIDATE_VALUE_INT_ARG(length_value, 3, "length", vm)
    VALIDATE_ARRAY_INDEX_ARG(3, "length", length_value, vm)

    int64_t min = VALUE_TO_INT(min_value);
    int64_t max = VALUE_TO_INT(max_value);
    int64_t length = VALUE_TO_INT(length_value);

    Obj *values_array_obj = vmu_array_obj(length, vm);
    Array *values_array = OBJ_TO_ARRAY(values_array_obj);

    INIT_RANDOM()

    for(int32_t i = 0; i < length; i++){
        values_array->values[i] = INT_VALUE((int64_t)(rand() % (max - min + 1) + min));
    }

    return OBJ_VALUE(values_array_obj);
}

void random_module_init(Allocator *allocator){
    random_module = factory_native_module("random", allocator);

    factory_add_native_fn("init", 1, native_fn_random_init, random_module, allocator);
    factory_add_native_fn("next_int", 0, native_fn_random_next_int, random_module, allocator);
    factory_add_native_fn("next_int_range", 2, native_fn_random_next_int_range, random_module, allocator);
    factory_add_native_fn("next_ints", 1, native_fn_random_next_ints, random_module, allocator);
    factory_add_native_fn("next_ints_range", 3, native_fn_random_next_ints_range, random_module, allocator);
}

#endif