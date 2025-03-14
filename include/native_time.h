#ifndef NATIVE_TIME_H
#define NATIVE_TIME_H

#define _POSIX_C_SOURCE 200809L

#include "rtypes.h"
#include "vm_utils.h"
#include <time.h>
#include <unistd.h>

NativeModule *time_module = NULL;

Value native_fn_time(uint8_t argsc, Value *values, void *target, VM *vm){
    time_t t;
    t = time(NULL);
    return INT_VALUE((int64_t)t);
}

Value native_fn_time_millis(uint8_t argsc, Value *values, void *target, VM *vm){
    struct timespec spec = {0};
    clock_gettime(CLOCK_REALTIME, &spec);
    
    int64_t millis = spec.tv_nsec / 1e+6 + spec.tv_sec * 1000;
    
    return INT_VALUE(millis);
}

Value native_fn_time_sleep(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];
    int64_t value = -1;

    if(!IS_INT(raw_value))
        vmu_error(vm, "Parameter 0 (value) must be integer");
    
    value = TO_INT(raw_value);
    
    if(value < 0)
        vmu_error(vm, "Parameter 0 (value: %ld) must be greater than 0", value);

    usleep(value * 1000);
    
    return EMPTY_VALUE;
}

void time_module_init(){
    time_module = runtime_native_module("time");

    runtime_add_native_fn("time", 0, native_fn_time, time_module);
    runtime_add_native_fn("millis", 0, native_fn_time_millis, time_module);
    runtime_add_native_fn("sleep", 1, native_fn_time_sleep, time_module);
}

#endif
