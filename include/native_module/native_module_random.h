#ifndef NATIVE_RANDOM_H
#define NATIVE_RANDOM_H

#include "obj.h"
#include "vm.h"
#include "xoshiro256.h"
#include "types.h"
#include "factory.h"
#include "vmu.h"
#include "types_utils.h"
#include "native/native_random.h"
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

NativeModule *random_native_module = NULL;

Value native_fn_random_create(uint8_t argsc, Value *values, Value target, void *context){
	RandomNative *random_native = random_native_create(VMU_NATIVE_FRONT_ALLOCATOR);
	NativeObj *native_obj = vmu_create_native(random_native, VMU_VM);

    random_native->xos256 = xoshiro256_init();

    return OBJ_VALUE(native_obj);
}

Value native_fn_random_create_seed(uint8_t argsc, Value *values, Value target, void *context){
    int64_t seed = validate_value_int_arg(values[0], 1, "seed", VMU_VM);
    RandomNative *random_native = random_native_create(VMU_NATIVE_FRONT_ALLOCATOR);
	NativeObj *native_obj = vmu_create_native(random_native, VMU_VM);

    random_native->xos256 = xoshiro256_init_seed((uint64_t)seed);

    return OBJ_VALUE(native_obj);
}

Value native_fn_random_next(uint8_t argsc, Value *values, Value target, void *context){
    RandomNative *random_native = random_native_validate_value_arg(values[0], 1, "generator", VMU_VM);
    return INT_VALUE(xoshiro256_next(&random_native->xos256));
}

void random_module_init(Allocator *allocator){
    random_native_module = factory_create_native_module("random", allocator);

    factory_add_native_fn("create", 0, native_fn_random_create, random_native_module);
    factory_add_native_fn("create_seed", 1, native_fn_random_create_seed, random_native_module);
    factory_add_native_fn("next", 1, native_fn_random_next, random_native_module);
}

#endif
