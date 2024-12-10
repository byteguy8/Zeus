#ifndef NATIVE_DICT
#define NATIVE_DICT

#include "types.h"
#include "value.h"
#include "vm_utils.h"

Value native_dict_contains(uint8_t argc, Value *values, void *target, VM *vm){
    LZHTable *dict = (LZHTable *)target;
    Value *value = &values[0];

    uint32_t hash = vm_utils_hash_value(value);
    uint8_t contains = lzhtable_hash_contains(hash, dict, NULL) != NULL;

    return BOOL_VALUE(contains);
}

Value native_dict_get(uint8_t argc, Value *values, void *target, VM *vm){
    LZHTable *dict = (LZHTable *)target;
    Value *key = &values[0];

    uint32_t hash = vm_utils_hash_value(key);
    LZHTableNode *node = NULL;

    lzhtable_hash_contains(hash, dict, &node);

    if(node) return *(Value *)node->value;
    else return EMPTY_VALUE;
}

Value native_dict_put(uint8_t argc, Value *values, void *target, VM *vm){
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

Value native_fn_dict_keys(uint8_t argsc, Value *values, void *target, VM *vm){
    LZHTable *dict = (LZHTable *)target;
    Obj *list_obj = vm_utils_list_obj(vm);
    
    if(!list_obj)
        vm_utils_error(vm, "Out of memory");

    DynArr *list = list_obj->value.list;
    LZHTableNode *node = dict->head;

    while (node){
        LZHTableNode *next = node->next_table_node;
        
        if(dynarr_insert(node->key, list))
            vm_utils_error(vm, "Out of memory");

        node = next;
    }
    
    return OBJ_VALUE(list_obj);
}

Value native_fn_dict_values(uint8_t argsc, Value *values, void *target, VM *vm){
    LZHTable *dict = (LZHTable *)target;
    Obj *list_obj = vm_utils_list_obj(vm);
    
    if(!list_obj)
        vm_utils_error(vm, "Out of memory");

    DynArr *list = list_obj->value.list;
    LZHTableNode *node = dict->head;

    while (node){
        LZHTableNode *next = node->next_table_node;
        
        if(dynarr_insert(node->value, list))
            vm_utils_error(vm, "Out of memory");

        node = next;
    }
    
    return OBJ_VALUE(list_obj);
}

#endif
