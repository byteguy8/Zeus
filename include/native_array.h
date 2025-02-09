#ifndef NATIVE_ARRAY_H
#define NATIVE_ARRAY_H

#include "types.h"
#include "value.h"
#include "memory.h"
#include "vm_utils.h"

static LZHTable *array_symbols = NULL;

Value native_fn_array_length(uint8_t argsc, Value *values, void *target, VM *vm){
    Array *array = (Array *)target;
    return INT_VALUE((int64_t)array->len);
}

Obj *native_array_get(char *symbol, void *target, VM *vm){
    if(!array_symbols){
        array_symbols = runtime_lzhtable();
        runtime_add_native_fn_info("length", 0, native_fn_array_length, array_symbols);
    }

    size_t key_size = strlen(symbol);
    NativeFnInfo *native_fn_info = (NativeFnInfo *)lzhtable_get((uint8_t *)symbol, key_size, array_symbols);
    
    if(native_fn_info){
        Obj *native_fn_obj = vm_utils_native_fn_obj(
            native_fn_info->arity,
            symbol,
            target,
            native_fn_info->raw_native,
            vm
        );

        if(!native_fn_obj){
            vm_utils_error(vm, "Out of memory");
        }

        return native_fn_obj;
    }

    return NULL;
}

#endif