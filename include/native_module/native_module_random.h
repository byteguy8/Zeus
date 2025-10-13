#ifndef NATIVE_RANDOM_H
#define NATIVE_RANDOM_H

#include "xoshiro256.h"
#include "types.h"
#include "factory.h"
#include "vmu.h"
#include "types_utils.h"
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

NativeModule *random_native_module = NULL;

static void destroy_random(void *context, void *value){
    MEMORY_DEALLOC(XOShiro256, 1, value, VMU_NATIVE_FRONT_ALLOCATOR);
}

static inline XOShiro256 *validate_random_arg(Value value, uint8_t param, char *name, VM *vm){
    RecordObj *record_obj = validate_value_record_arg(value, param, name, vm);
    RecordExtra *extra = &(record_obj->extra);

    if(extra->type != RANDOM_RECORD_EXTRA_TYPE){
        vmu_error(vm, "Illegal type of argument %" PRIu8 ": expect '%s' of type 'random'", param, name);
    }

    return (XOShiro256 *)(extra->value);
}

Value native_fn_random_create(uint8_t argsc, Value *values, Value target, void *context){
    XOShiro256 *xos256 = MEMORY_ALLOC(XOShiro256, 1, VMU_NATIVE_FRONT_ALLOCATOR);
    RecordObj *random_record_obj = vmu_create_record(0, VMU_VM);
    RecordExtra *random_extra = &random_record_obj->extra;

    *xos256 = xoshiro256_init();

    random_extra->type = RANDOM_RECORD_EXTRA_TYPE;
    random_extra->value = xos256;
    random_extra->ctx = context;
    random_extra->destroy_value = destroy_random;

    return OBJ_VALUE(random_record_obj);
}

Value native_fn_random_create_seed(uint8_t argsc, Value *values, Value target, void *context){
    uint64_t seed = (uint64_t)validate_value_int_arg(values[0], 1, "seed", VMU_VM);
    XOShiro256 *xos256 = MEMORY_ALLOC(XOShiro256, 1, VMU_NATIVE_FRONT_ALLOCATOR);
    RecordObj *random_record_obj = vmu_create_record(0, VMU_VM);
    RecordExtra *random_extra = &random_record_obj->extra;

    *xos256 = xoshiro256_init_seed(seed);

    random_extra->type = RANDOM_RECORD_EXTRA_TYPE;
    random_extra->value = xos256;
    random_extra->ctx = context;
    random_extra->destroy_value = destroy_random;

    return OBJ_VALUE(random_record_obj);
}

Value native_fn_random_next(uint8_t argsc, Value *values, Value target, void *context){
    XOShiro256 *xos256 = validate_random_arg(values[0], 1, "generator", VMU_VM);
    return INT_VALUE(xoshiro256_next(xos256));
}

void random_module_init(Allocator *allocator){
    random_native_module = factory_create_native_module("random", allocator);

    factory_add_native_fn("create", 0, native_fn_random_create, random_native_module);
    factory_add_native_fn("create_seed", 1, native_fn_random_create_seed, random_native_module);
    factory_add_native_fn("next", 1, native_fn_random_next, random_native_module);
}

#endif