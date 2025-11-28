#ifndef NATIVE_DICT
#define NATIVE_DICT

#include "vm_factory.h"
#include "vmu.h"

static LZOHTable *dict_symbols = NULL;

Value native_dict_fn_len(uint8_t argsc, Value *values, Value target, void *context){
    DictObj *dict_obj = VALUE_TO_DICT(target);
    LZOHTable *key_values = dict_obj->key_values;
    return INT_VALUE((int64_t)key_values->n);
}

Value native_dict_fn_contains(uint8_t argsc, Value *values, Value target, void *context){
    DictObj *dict_obj = VALUE_TO_DICT(target);
    Value key = values[0];
    return BOOL_VALUE((int64_t)vmu_dict_contains(key, dict_obj));
}

Value native_dict_fn_clear(uint8_t argsc, Value *values, Value target, void *context){
    DictObj *dict_obj = VALUE_TO_DICT(target);
    LZOHTable *key_values = dict_obj->key_values;
    size_t len = key_values->n;

    LZOHTABLE_CLEAR(key_values);

    return INT_VALUE((int64_t)len);
}

Value native_dict_fn_remove(uint8_t argsc, Value *values, Value target, void *context){
    DictObj *dict_obj = VALUE_TO_DICT(target);
    Value key = values[0];

    vmu_dict_remove(key, dict_obj);

    return EMPTY_VALUE;
}

NativeFn *native_dict_get(size_t key_size, const char *key, VM *vm){
    if(!dict_symbols){
        Allocator *allocator = &vm->front_allocator;
        dict_symbols = MEMORY_LZOHTABLE(allocator);

        dynarr_insert_ptr(dict_symbols, vm->native_symbols);

        vm_factory_native_fn_add_info(dict_symbols, allocator, "len", 0, native_dict_fn_len);
        vm_factory_native_fn_add_info(dict_symbols, allocator, "contains", 1, native_dict_fn_contains);
        vm_factory_native_fn_add_info(dict_symbols, allocator, "clear", 0, native_dict_fn_clear);
        vm_factory_native_fn_add_info(dict_symbols, allocator, "remove", 1, native_dict_fn_remove);
    }

    NativeFn *native_fn = NULL;
    lzohtable_lookup(key_size, key, dict_symbols, (void **)(&native_fn));

    return native_fn;
}

#endif