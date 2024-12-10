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
		Value *clone_value = vm_utils_clone_value(value, vm);

		if(!clone_value) vm_utils_error(vm, "Out of memory");
	    lzhtable_hash_put(hash, clone_value, dict);
	}

    return EMPTY_VALUE;
}

#endif
