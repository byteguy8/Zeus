#ifndef NATIVE_TIME_H
#define NATIVE_TIME_H

#include "vmu.h"
#include "utils.h"
#include <time.h>

NativeModule *time_native_module = NULL;

Value native_fn_time_millis_sleep(uint8_t argsc, Value *values, Value target, void *context){
    int64_t sleep_value = validate_value_int_range_arg(values[0], 1, "millis", 1, UINT32_MAX, VMU_VM);
    utils_sleep(sleep_value);
    return EMPTY_VALUE;
}

Value native_fn_time_millis(uint8_t argsc, Value *values, Value target, void *context){
    return INT_VALUE(utils_millis());
}

void time_module_init(Allocator *allocator){
    time_native_module = factory_create_native_module("time", allocator);

    factory_add_native_fn("msleep", 1, native_fn_time_millis_sleep, time_native_module);
    factory_add_native_fn("millis", 0, native_fn_time_millis, time_native_module);
}

#endif
