#ifndef NATIVE_TIME_H
#define NATIVE_TIME_H

#include "types.h"
#include "value.h"
#include "vm_utils.h"
#include <time.h>
#include <unistd.h>

Value native_time(uint8_t argc, Value *values, void *target, VM *vm){
    time_t t;
    t = time(NULL);
    return INT_VALUE((int64_t)t);
}

Value native_sleep(uint8_t argc, Value *values, void *target, VM *vm){
    Value *raw_value = &values[0];
    int64_t value = -1;

    if(!vm_utils_is_i64(raw_value, &value))
        vm_utils_error(vm, "parameter 0(value) must be integer");
    if(value < 0)
        vm_utils_error(vm, "parameter 0(value) must be greater than 0");
    
    usleep(value * 1000);
    
    return EMPTY_VALUE;
}

#endif