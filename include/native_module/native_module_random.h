#ifndef NATIVE_RANDOM_H
#define NATIVE_RANDOM_H

#include "xoshiro256.h"
#include "types.h"
#include "lzhtable.h"
#include "factory.h"
#include "vmu.h"
#include "tutils.h"
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

NativeModule *random_module = NULL;

Value native_fn_random_is_random(uint8_t argsc, Value *values, Value *target, void *context){
    Value *random_value = &values[0];
    return BOOL_VALUE(IS_VALUE_RECORD(random_value) && VALUE_TO_RECORD(random_value)->type == RANDOM_RTYPE);
}

Value native_fn_random_init(uint8_t argsc, Value *values, Value *target, void *context){
    ObjHeader *record_header = vmu_create_record_random_obj(context);
    RecordObj *record_obj = OBJ_TO_RECORD(record_header);
    XOShiro256 *random = RECORD_RANDOM(record_obj);

    *random = xoshiro256_init();

    return OBJ_VALUE(record_header);
}

Value native_fn_random_init_seed(uint8_t argsc, Value *values, Value *target, void *context){
    Value *seed_value = &values[0];

    VALIDATE_VALUE_INT_ARG(seed_value, 1, "seed", context)

    int64_t seed = VALUE_TO_INT(seed_value);
    ObjHeader *record_header = vmu_create_record_random_obj(context);
    RecordObj *record_obj = OBJ_TO_RECORD(record_header);
    XOShiro256 *random = RECORD_RANDOM(record_obj);

    *random = xoshiro256_init_seed((uint64_t)seed);

    return OBJ_VALUE(record_header);
}

Value native_fn_random_next_int(uint8_t argsc, Value *values, Value *target, void *context){
    Value *record_value = &values[0];

    VALIDATE_VALUE_RECORD_ARG(record_value, 1, "context", context)
    RecordObj *record_obj = VALUE_TO_RECORD(record_value);

    if(record_obj->type != RANDOM_RTYPE){
        vmu_error(context, "Illegal context. Expect record representing random context");
    }

    XOShiro256 *xos256 = RECORD_RANDOM(record_obj);
    int64_t next_int = xoshiro256_next(xos256);

    return INT_VALUE(next_int);
}

Value native_fn_random_next_int_range(uint8_t argsc, Value *values, Value *target, void *context){
    Value *min_value = &values[0];
    Value *max_value = &values[1];
    Value *record_value = &values[2];

    VALIDATE_VALUE_INT_ARG(min_value, 1, "min", context)
    VALIDATE_VALUE_INT_ARG(max_value, 2, "max", context)
    VALIDATE_VALUE_RECORD_ARG(record_value, 3, "context", context)

    VALIDATE_VALUE_INT_RANGE_ARG(min_value, 1, "min", 0, INT64_MAX, context);
    VALIDATE_VALUE_INT_RANGE_ARG(max_value, 2, "max", VALUE_TO_INT(min_value), INT64_MAX, context);

    int64_t min = VALUE_TO_INT(min_value);
    int64_t max = VALUE_TO_INT(max_value);
    RecordObj *record_obj = VALUE_TO_RECORD(record_value);

    if(record_obj->type != RANDOM_RTYPE){
        vmu_error(context, "Illegal context. Expect record representing random context");
    }

    XOShiro256 *xos256 = RECORD_RANDOM(record_obj);
    int64_t next_int = XOSHIRO256_NEXT_RANGE(min, max, xos256);

    return INT_VALUE(next_int);
}

Value native_fn_random_next_ints(uint8_t argsc, Value *values, Value *target, void *context){
    Value *length_value = &values[0];
    Value *record_value = &values[1];

    VALIDATE_VALUE_INT_ARG(length_value, 1, "length", context)
    VALIDATE_VALUE_RECORD_ARG(record_value, 2, "context", context)

    VALIDATE_VALUE_ARRAY_INDEX_ARG(length_value, 1, "length", context)

    int64_t length = VALUE_TO_INT(length_value);
    RecordObj *record_obj = VALUE_TO_RECORD(record_value);

    if(record_obj->type != RANDOM_RTYPE){
        vmu_error(context, "Illegal context. Expect record representing random context");
    }

    XOShiro256 *xos256 = RECORD_RANDOM(record_obj);
    ObjHeader *values_array_obj = vmu_create_array_obj(length, context);
    ArrayObj *values_array = OBJ_TO_ARRAY(values_array_obj);

    for(int32_t i = 0; i < length; i++){
        values_array->values[i] = INT_VALUE((int64_t)xoshiro256_next(xos256));
    }

    return OBJ_VALUE(values_array_obj);
}

Value native_fn_random_next_ints_range(uint8_t argsc, Value *values, Value *target, void *context){
    Value *min_value = &values[0];
    Value *max_value = &values[1];
    Value *length_value = &values[2];
    Value *record_value = &values[3];

    VALIDATE_VALUE_INT_ARG(min_value, 1, "min", context)
    VALIDATE_VALUE_INT_ARG(max_value, 2, "max", context)
    VALIDATE_VALUE_INT_ARG(length_value, 3, "length", context)
    VALIDATE_VALUE_RECORD_ARG(record_value, 4, "context", context)

    VALIDATE_VALUE_INT_RANGE_ARG(min_value, 1, "min", 0, INT64_MAX, context);
    VALIDATE_VALUE_INT_RANGE_ARG(max_value, 2, "max", VALUE_TO_INT(min_value), INT64_MAX, context);
    VALIDATE_VALUE_ARRAY_INDEX_ARG(length_value, 3, "length", context)

    int64_t min = VALUE_TO_INT(min_value);
    int64_t max = VALUE_TO_INT(max_value);
    int64_t length = VALUE_TO_INT(length_value);
    RecordObj *record_obj = VALUE_TO_RECORD(record_value);

    if(record_obj->type != RANDOM_RTYPE){
        vmu_error(context, "Illegal context. Expect record representing random context");
    }

    XOShiro256 *xos256 = RECORD_RANDOM(record_obj);
    ObjHeader *values_array_obj = vmu_create_array_obj(length, context);
    ArrayObj *values_array = OBJ_TO_ARRAY(values_array_obj);

    for(aidx_t i = 0; i < length; i++){
        values_array->values[i] = INT_VALUE((int64_t)XOSHIRO256_NEXT_RANGE(min, max, xos256));
    }

    return OBJ_VALUE(values_array_obj);
}

void random_module_init(Allocator *allocator){
    random_module = factory_native_module("random", allocator);

    factory_add_native_fn("is_random", 1, native_fn_random_is_random, random_module, allocator);
    factory_add_native_fn("init", 0, native_fn_random_init, random_module, allocator);
    factory_add_native_fn("init_seed", 1, native_fn_random_init_seed, random_module, allocator);
    factory_add_native_fn("next_int", 1, native_fn_random_next_int, random_module, allocator);
    factory_add_native_fn("next_int_range", 3, native_fn_random_next_int_range, random_module, allocator);
    factory_add_native_fn("next_ints", 2, native_fn_random_next_ints, random_module, allocator);
    factory_add_native_fn("next_ints_range", 4, native_fn_random_next_ints_range, random_module, allocator);
}

#endif