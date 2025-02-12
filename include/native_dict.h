#ifndef NATIVE_DICT
#define NATIVE_DICT

#include "types.h"
#include "value.h"
#include "vm_utils.h"

static LZHTable *dict_symbols = NULL;

static void clear_dict(void *key, void *value){
    free(key);
    free(value);
}

Value native_fn_dict_contains(uint8_t argsc, Value *values, void *target, VM *vm){
    LZHTable *dict = (LZHTable *)target;
    Value *value = &values[0];

    uint32_t hash = vm_utils_hash_value(value);
    uint8_t contains = lzhtable_hash_contains(hash, dict, NULL) != NULL;

    return BOOL_VALUE(contains);
}

Value native_fn_dict_get(uint8_t argsc, Value *values, void *target, VM *vm){
    LZHTable *dict = (LZHTable *)target;
    Value *key = &values[0];

    uint32_t hash = vm_utils_hash_value(key);
    LZHTableNode *node = NULL;

    lzhtable_hash_contains(hash, dict, &node);

    if(node) return *(Value *)node->value;
    else return EMPTY_VALUE;
}

Value native_fn_dict_put(uint8_t argsc, Value *values, void *target, VM *vm){
    LZHTable *dict = (LZHTable *)target;
    Value *key = &values[0];
    Value *value = &values[1];

    uint32_t hash = vm_utils_hash_value(key);
	LZHTableNode *node = NULL;

	if(lzhtable_hash_contains(hash, dict, &node)){
		Value *dict_value = (Value *)node->value;
		memcpy(dict_value, value, sizeof(Value));
	}else{
        Value *key_clone = vm_utils_clone_value(key, vm);
		Value *value_clone = vm_utils_clone_value(value, vm);

        if(!key_clone || !value_clone)
            vm_utils_error(vm, "Out of memory");

        if(lzhtable_hash_put_key(key_clone, hash, value_clone, dict))
            vm_utils_error(vm, "Out of memory");
	}

    return EMPTY_VALUE;
}

Value native_fn_dict_remove(uint8_t argsc, Value *values, void *target, VM *vm){
    LZHTable *dict = (LZHTable *)target;
    Value *key = &values[0];
    uint32_t key_hash = vm_utils_hash_value(key);
    LZHTableNode *node = NULL;

    if(!lzhtable_hash_contains(key_hash, dict, &node))
		vm_utils_error(vm, "Unknown dictionary key");

    Value *bucket_key = node->key;
    Value *bucket_value = NULL;

    lzhtable_hash_remove(key_hash, dict, (void **)&bucket_value);

    free(bucket_key);
    free(bucket_value);

    return EMPTY_VALUE;
}

Value native_fn_dict_clear(uint8_t argsc, Value *values, void *target, VM *vm){
    LZHTable *dict = (LZHTable *)target;
    
    if(lzhtable_clear_shrink(clear_dict, dict)){
        vm_utils_error(vm, "Fatal error when cleaning dictionary");
    }

    return EMPTY_VALUE;
}

Value native_fn_dict_keys(uint8_t argsc, Value *values, void *target, VM *vm){
    LZHTable *dict = (LZHTable *)target;
    Obj *array_obj = vm_utils_array_obj(dict->n, vm);
    
    if(!array_obj){
        vm_utils_error(vm, "Out of memory");
    }

    LZHTableNode *node = dict->head;
    Array *array = array_obj->value.array;

    for (int16_t i = 0; i < array->len; i++){
        LZHTableNode *next = node->next_table_node;
        array->values[i] = *(Value *)node->key;
        node = next;
    }
    
    return OBJ_VALUE(array_obj);
}

Value native_fn_dict_values(uint8_t argsc, Value *values, void *target, VM *vm){
    LZHTable *dict = (LZHTable *)target;
    Obj *array_obj = vm_utils_array_obj(dict->n, vm);
    
    if(!array_obj){
        vm_utils_error(vm, "Out of memory");
    }

    LZHTableNode *current = dict->head;
    Array *array = array_obj->value.array;

    for (int16_t i = 0; i < array->len; i++){
        LZHTableNode *next = current->next_table_node;
        array->values[i] = *(Value *)current->value;
        current = next;
    }
    
    return OBJ_VALUE(array_obj);
}

Obj *native_dict_get(char *symbol, void *target, VM *vm){
    if(!dict_symbols){
        dict_symbols = runtime_lzhtable();
        runtime_add_native_fn_info("contains", 1, native_fn_dict_contains, dict_symbols);
        runtime_add_native_fn_info("get", 1, native_fn_dict_get, dict_symbols);
        runtime_add_native_fn_info("put", 2, native_fn_dict_put, dict_symbols);
        runtime_add_native_fn_info("remove", 1, native_fn_dict_remove, dict_symbols);
        runtime_add_native_fn_info("clear", 0, native_fn_dict_clear, dict_symbols);
        runtime_add_native_fn_info("keys", 0, native_fn_dict_keys, dict_symbols);
        runtime_add_native_fn_info("values", 0, native_fn_dict_values, dict_symbols);
    }

    size_t key_size = strlen(symbol);
    NativeFnInfo *native_fn_info = (NativeFnInfo *)lzhtable_get((uint8_t *)symbol, key_size, dict_symbols);
    
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
