#ifndef NATIVE_DICT
#define NATIVE_DICT

#include "vmu.h"

static LZHTable *dict_symbols = NULL;

static void clear_dict(void *key, void *value, void *vm){
    vmu_destroy_value(key, vm);
    vmu_destroy_value(value, vm);
}

Value native_fn_dict_size(uint8_t argsc, Value *values, Value *target, void *context){
    DictObj *dict_obj = VALUE_TO_DICT(target);
    LZHTable *dict = dict_obj->dict;
    return INT_VALUE(LZHTABLE_COUNT(dict));
}

Value native_fn_dict_contains(uint8_t argsc, Value *values, Value *target, void *context){
    DictObj *dict_obj = VALUE_TO_DICT(target);
    LZHTable *dict = dict_obj->dict;
    Value *value = &values[0];

    uint32_t hash = vmu_hash_value(value);
    uint8_t contains = lzhtable_hash_contains(hash, dict, NULL) != NULL;

    return BOOL_VALUE(contains);
}

Value native_fn_dict_get(uint8_t argsc, Value *values, Value *target, void *context){
    DictObj *dict_obj = VALUE_TO_DICT(target);
    LZHTable *dict = dict_obj->dict;
    Value *key = &values[0];

    uint32_t hash = vmu_hash_value(key);
    LZHTableNode *node = NULL;

    lzhtable_hash_contains(hash, dict, &node);

    if(node){
        return *(Value *)node->value;
    }

    return EMPTY_VALUE;
}

Value native_fn_dict_put(uint8_t argsc, Value *values, Value *target, void *context){
    DictObj *dict_obj = VALUE_TO_DICT(target);
    LZHTable *dict = dict_obj->dict;
    Value *key = &values[0];
    Value *value = &values[1];

    uint32_t hash = vmu_hash_value(key);
	LZHTableNode *node = NULL;

	if(lzhtable_hash_contains(hash, dict, &node)){
		Value *dict_value = (Value *)node->value;
        *dict_value = *value;
	}else{
        Value *key_clone = vmu_clone_value(key, context);
		Value *value_clone = vmu_clone_value(value, context);

        lzhtable_hash_put_key(key_clone, hash, value_clone, dict);
	}

    return EMPTY_VALUE;
}

Value native_fn_dict_remove(uint8_t argsc, Value *values, Value *target, void *context){
    DictObj *dict_obj = VALUE_TO_DICT(target);
    LZHTable *dict = dict_obj->dict;
    Value *key = &values[0];

    uint32_t key_hash = vmu_hash_value(key);
    LZHTableNode *node = NULL;

    if(!lzhtable_hash_contains(key_hash, dict, &node)){
        vmu_error(context, "Unknown key");
    }

    Value *bucket_key = node->key;
    Value *bucket_value = NULL;

    lzhtable_hash_remove(key_hash, dict, (void **)&bucket_value);

    vmu_destroy_value(bucket_key, context);
    vmu_destroy_value(bucket_value, context);

    return EMPTY_VALUE;
}

Value native_fn_dict_clear(uint8_t argsc, Value *values, Value *target, void *context){
    DictObj *dict_obj = VALUE_TO_DICT(target);
    LZHTable *dict = dict_obj->dict;

    lzhtable_clear_shrink(context, clear_dict, dict);

    return EMPTY_VALUE;
}

Value native_fn_dict_keys(uint8_t argsc, Value *values, Value *target, void *context){
    DictObj *dict_obj = VALUE_TO_DICT(target);
    LZHTable *dict = dict_obj->dict;

    size_t keys_len = dict->n;
    VALIDATE_ARRAY_SIZE(keys_len, context)

    ObjHeader *array_obj = vmu_create_array_obj(keys_len, context);
    ArrayObj *array = OBJ_TO_ARRAY(array_obj);

    LZHTableNode *node = dict->head;

    for (int16_t i = 0; i < array->len; i++){
        LZHTableNode *next = node->next_table_node;
        array->values[i] = *(Value *)node->key;
        node = next;
    }

    return OBJ_VALUE(array_obj);
}

Value native_fn_dict_values(uint8_t argsc, Value *values, Value *target, void *context){
    DictObj *dict_obj = VALUE_TO_DICT(target);
    LZHTable *dict = dict_obj->dict;

    size_t values_len = dict->n;
    VALIDATE_ARRAY_SIZE(values_len, context)

    ObjHeader *array_obj = vmu_create_array_obj(values_len, context);
    ArrayObj *array = OBJ_TO_ARRAY(array_obj);

    LZHTableNode *current = dict->head;

    for (int16_t i = 0; i < array->len; i++){
        LZHTableNode *next = current->next_table_node;
        array->values[i] = *(Value *)current->value;
        current = next;
    }

    return OBJ_VALUE(array_obj);
}

NativeFnInfo *native_dict_get(char *symbol, Allocator *allocator){
    if(!dict_symbols){
        dict_symbols = FACTORY_LZHTABLE(allocator);

        factory_add_native_fn_info("size", 0, native_fn_dict_size, dict_symbols, allocator);
        factory_add_native_fn_info("contains", 1, native_fn_dict_contains, dict_symbols, allocator);
        factory_add_native_fn_info("get", 1, native_fn_dict_get, dict_symbols, allocator);
        factory_add_native_fn_info("put", 2, native_fn_dict_put, dict_symbols, allocator);
        factory_add_native_fn_info("remove", 1, native_fn_dict_remove, dict_symbols, allocator);
        factory_add_native_fn_info("clear", 0, native_fn_dict_clear, dict_symbols, allocator);
        factory_add_native_fn_info("keys", 0, native_fn_dict_keys, dict_symbols, allocator);
        factory_add_native_fn_info("values", 0, native_fn_dict_values, dict_symbols, allocator);
    }

    return (NativeFnInfo *)lzhtable_get((uint8_t *)symbol, strlen(symbol), dict_symbols);
}

#endif
