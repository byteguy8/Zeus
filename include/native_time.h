#ifndef NATIVE_TIME_H
#define NATIVE_TIME_H

#include "vmu.h"
#include "utils.h"
#include <time.h>

NativeModule *time_module = NULL;

Value native_fn_time(uint8_t argsc, Value *values, Value *target, VM *vm){
    time_t t;
    t = time(NULL);
    return INT_VALUE((int64_t)t);
}

Value native_fn_time_millis(uint8_t argsc, Value *values, Value *target, VM *vm){
    return INT_VALUE(utils_millis());
}

Value native_fn_time_sleep(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *raw_time = &values[0];

    VALIDATE_VALUE_INT_ARG(raw_time, 1, "time", vm)
    VALIDATE_VALUE_INT_RANGE_ARG(0, "time", raw_time, 0, INT32_MAX, vm)

    utils_sleep(VALUE_TO_INT(raw_time));

    return EMPTY_VALUE;
}

void time_module_init(Allocator *allocator){
    time_module = factory_native_module("time", allocator);

    factory_add_native_fn("time", 0, native_fn_time, time_module, allocator);
    factory_add_native_fn("millis", 0, native_fn_time_millis, time_module, allocator);
    factory_add_native_fn("sleep", 1, native_fn_time_sleep, time_module, allocator);
}

#endif
