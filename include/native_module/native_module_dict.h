#ifndef NATIVE_DICT
#define NATIVE_DICT

#include "vmu.h"

static LZOHTable *dict_symbols = NULL;

Value native_dict_fn_len(uint8_t argsc, Value *values, Value target, void *context){
    DictObj *dict_obj = VALUE_TO_DICT(target);
    LZOHTable *key_values = dict_obj->key_values;
    return INT_VALUE((int64_t)key_values->n);
}

Value native_dict_fn_clear(uint8_t argsc, Value *values, Value target, void *context){
    DictObj *dict_obj = VALUE_TO_DICT(target);
    LZOHTable *key_values = dict_obj->key_values;
    size_t len = key_values->n;

    LZOHTABLE_CLEAR(key_values);

    return INT_VALUE((int64_t)len);
}

NativeFn *native_dict_get(size_t key_size, const char *key, VM *vm){
    if(!dict_symbols){
        Allocator *allocator = &vm->front_allocator;
        dict_symbols = FACTORY_LZOHTABLE(allocator);

        dynarr_insert_ptr(dict_symbols, vm->native_symbols);

        factory_add_native_fn_info_n("len", 0, native_dict_fn_len, dict_symbols, allocator);
        factory_add_native_fn_info_n("clear", 0, native_dict_fn_clear, dict_symbols, allocator);
    }

    NativeFn *native_fn = NULL;
    lzohtable_lookup(key_size, key, dict_symbols, (void **)(&native_fn));

    return native_fn;
}

#endif