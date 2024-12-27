#ifndef NATIVE_TIME_H
#define NATIVE_TIME_H

#define _POSIX_C_SOURCE 200809L

#include "types.h"
#include "value.h"
#include "vm_utils.h"
#include <time.h>
#include <unistd.h>

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
        vm_utils_error(vm, "parameter 0 (value) must be integer");
    
    value = TO_INT(raw_value);
    
    if(value < 0)
        vm_utils_error(vm, "parameter 0 (value: %ld) must be greater than 0", value);

    usleep(value * 1000);
    
    return EMPTY_VALUE;
}

#endif
