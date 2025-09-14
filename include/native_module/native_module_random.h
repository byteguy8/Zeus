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

Value native_fn_random_next(uint8_t argsc, Value *values, Value *target, void *context){
    return INT_VALUE(xoshiro256_next(&xos256));
}

void random_module_init(Allocator *allocator){
    xos256 = xoshiro256_init();
    random_native_module = factory_create_native_module("random", allocator);

    factory_add_native_fn("next", 0, native_fn_random_next, random_native_module, allocator);
}

#endif