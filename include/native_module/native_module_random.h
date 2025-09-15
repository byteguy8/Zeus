#ifndef NATIVE_RANDOM_H
#define NATIVE_RANDOM_H

#include "xoshiro256.h"
#include "types.h"
#include "factory.h"
#include "vmu.h"
#include "tutils.h"
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

XOShiro256 xos256;
NativeModule *random_native_module = NULL;

Value native_fn_random_seed(uint8_t argsc, Value *values, Value *target, void *context){
    uint64_t seed = (uint64_t)validate_value_int_arg(&values[0], 1, "seed", VMU_VM);
    xos256 = xoshiro256_init_seed(seed);
    return EMPTY_VALUE;
}

Value native_fn_random_next(uint8_t argsc, Value *values, Value *target, void *context){
    return INT_VALUE(xoshiro256_next(&xos256));
}

void random_module_init(Allocator *allocator){
    xos256 = xoshiro256_init();
    random_native_module = factory_create_native_module("random", allocator);

    factory_add_native_fn("seed", 1, native_fn_random_seed, random_native_module, allocator);
    factory_add_native_fn("next", 0, native_fn_random_next, random_native_module, allocator);
}

#endif