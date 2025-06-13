#ifndef NATIVE_STR
#define NATIVE_STR

#include "memory.h"
#include "factory.h"
#include "vmu.h"
#include "tutils.h"

static LZHTable *str_symbols = NULL;

Value native_fn_str_code(uint8_t argsc, Value *values, Value *target, void *context){
    StrObj *target_str = VALUE_TO_STR(target);

    if(target_str->len != 1){
        vmu_error(context, "Expect string of length 1, but is %d", target_str->len);
    }

    return INT_VALUE((int64_t)target_str->buff[0]);
}

Value native_fn_str_insert(uint8_t argsc, Value *values, Value *target, void *context){
	StrObj *target_str = VALUE_TO_STR(target);
    Value *at_value = &values[0];
    Value *value_str = &values[1];

    sidx_t str_len = target_str->len;

    VALIDATE_VALUE_INT_ARG(at_value, 1, "at", context)
    VALIDATE_VALUE_INT_RANGE_ARG(at_value, 1, "at", 0, str_len, context);
    VALIDATE_VALUE_STR_ARG(value_str, 2, "to_insert", context)

    int64_t at = VALUE_TO_INT(at_value);
    StrObj *str = VALUE_TO_STR(value_str);
    size_t raw_str_len = str_len + str->len;
    char *raw_str = MEMORY_ALLOC(char, raw_str_len + 1, ((VM *)context)->fake_allocator);

    memcpy(raw_str, target_str->buff, at);
    memcpy(raw_str + at, str->buff, str->len);
    memcpy(raw_str + at + str->len, target_str->buff + at, str_len - (at));

    raw_str[raw_str_len] = '\0';

    ObjHeader *str_obj = vmu_create_str_obj(&raw_str, context);

	return OBJ_VALUE(str_obj);
}

Value native_fn_str_remove(uint8_t argsc, Value *values, Value *target, void *context){
	StrObj *target_str = VALUE_TO_STR(target);
    Value *from_value = &values[0];
    Value *to_value = &values[1];
    sidx_t str_len = target_str->len;

    if(str_len == 0){
        vmu_error(context, "Cannot remove from empty string");
    }

    sidx_t max_idx = str_len == 0 ? 0 : str_len - 1;

    VALIDATE_VALUE_INT_ARG(from_value, 1, "from", context)
    VALIDATE_VALUE_INT_ARG(to_value, 2, "to", context)

    int64_t from = VALUE_TO_INT(from_value);
    int64_t to = VALUE_TO_INT(to_value);

    VALIDATE_VALUE_INT_RANGE_ARG(from_value, 1, "from", 0, max_idx, context)
    VALIDATE_VALUE_INT_RANGE_ARG(to_value, 2, "to", from, max_idx, context)

    size_t raw_str_len = target_str->len - (to - from + 1);
    char *raw_str = MEMORY_ALLOC(char, raw_str_len + 1, ((VM *)context)->fake_allocator);

    memcpy(raw_str, target_str->buff, from);
    memcpy(raw_str + from, target_str->buff + to + 1, target_str->len - to  - 1);
    raw_str[raw_str_len] = '\0';

    ObjHeader *str_obj = vmu_create_str_obj(&raw_str, context);

	return OBJ_VALUE(str_obj);
}

Value native_fn_str_hash(uint8_t argsc, Value *values, Value *target, void *context){
	StrObj *target_str = VALUE_TO_STR(target);
	return INT_VALUE((int64_t)target_str->hash);
}

Value native_fn_str_is_runtime(uint8_t argsc, Value *values, Value *target, void *context){
	StrObj *target_str = VALUE_TO_STR(target);
	return BOOL_VALUE(target_str->runtime == 1);
}

Value native_fn_str_size(uint8_t argsc, Value *values, Value *target, void *context){
	StrObj *target_str = VALUE_TO_STR(target);
	return INT_VALUE(target_str->len);
}

Value native_fn_str_char_at(uint8_t argc, Value *values, Value *target, void *context){
    Value *at_value = &values[0];
    StrObj *target_str = VALUE_TO_STR(target);
    sidx_t str_len = target_str->len;

    if(str_len == 0){
        vmu_error(context, "Cannot get char from empty string");
    }

    VALIDATE_VALUE_INT_ARG(at_value, 1, "at", context)
    VALIDATE_VALUE_INT_RANGE_ARG(at_value, 1, "at", 0, str_len - 1, context)

    int64_t index = VALUE_TO_INT(at_value);
    char *raw_str = factory_clone_raw_str_range((size_t)index, 1, target_str->buff, ((VM *)context)->fake_allocator);
    ObjHeader *str_obj = vmu_create_str_obj(&raw_str, context);

    return OBJ_VALUE(str_obj);
}

Value native_fn_str_sub_str(uint8_t argc, Value *values, Value *target, void *context){
    StrObj *target_str = VALUE_TO_STR(target);
    Value *from_value = &values[0];
    Value *to_value = &values[1];
    sidx_t str_len = target_str->len;

    if(str_len == 0){
        vmu_error(context, "Cannot make substring from empty string");
    }

    VALIDATE_VALUE_INT_ARG(from_value, 1, "from", context)
    VALIDATE_VALUE_INT_ARG(to_value, 2, "to", context)

    int64_t from = VALUE_TO_INT(from_value);
    int64_t to = VALUE_TO_INT(to_value);

    VALIDATE_VALUE_INT_RANGE_ARG(from_value, 1, "from", 0, str_len - 1, context)
    VALIDATE_VALUE_INT_RANGE_ARG(to_value, 2, "to", from, str_len - 1, context)

    char *raw_sub_str = factory_clone_raw_str_range(from, to - from + 1, target_str->buff, ((VM *)context)->fake_allocator);
    ObjHeader *sub_str_obj = vmu_create_str_obj(&raw_sub_str, context);

    return OBJ_VALUE(sub_str_obj);
}

Value native_fn_str_split(uint8_t argc, Value *values, Value *target, void *context){
    StrObj *target_str = VALUE_TO_STR(target);
    Value *by_value = &values[0];

    VALIDATE_VALUE_STR_ARG(by_value, 1, "by", context)

    StrObj *by_str = VALUE_TO_STR(by_value);

    if(by_str->len != 1){
        vmu_error(context, "Illegal size or argument 1: expect 'by' of size 1");
    }

    char *str_buff = target_str->buff;
    sidx_t str_len = target_str->len;
    char *by_buff = by_str->buff;

    DynArr *list = FACTORY_DYNARR(sizeof(Value), ((VM *)context)->fake_allocator);

	char coincidence = 0;
    size_t from = 0;
    size_t to = 0;
    sidx_t i = 0;

    while (i < str_len){
        char c = str_buff[i++];

        if(c == by_buff[0]){
            to = i - 1;
			size_t len = to - from;

			if(!coincidence){
                coincidence = 1;
            }

            char *raw_str = factory_clone_raw_str_range(from, len, str_buff, ((VM *)context)->fake_allocator);
            ObjHeader *str_obj = vmu_create_str_obj(&raw_str, context);

            dynarr_insert(&OBJ_VALUE(str_obj), list);

            from = i;
        }
    }

	if(coincidence){
        char *raw_str = factory_clone_raw_str_range(from, i - 1, str_buff, ((VM *)context)->fake_allocator);
		ObjHeader *str_obj = vmu_create_str_obj(&raw_str, context);

        dynarr_insert(&OBJ_VALUE(str_obj), list);
	}

    size_t list_size = DYNARR_LEN(list);

    if(list_size > (size_t)ARRAY_LENGTH_MAX){
        vmu_error(context, "Cannot create array from count of split");
    }

    aidx_t arr_size = (aidx_t)list_size;
    ObjHeader *arr_obj = vmu_create_array_obj(arr_size, context);
    ArrayObj *arr = OBJ_TO_ARRAY(arr_obj);
    Value *arr_values = arr->values;

    for (aidx_t i = 0; i < arr_size; i++){
        arr_values[i] = DYNARR_GET_AS(Value, i, list);
    }

    dynarr_destroy(list);

    return OBJ_VALUE(arr_obj);
}

Value native_fn_str_lstrip(uint8_t argsc, Value *values, Value *target, void *context){
	StrObj *target_str = VALUE_TO_STR(target);

    sidx_t str_len = target_str->len;
    char *str_buff = target_str->buff;

    sidx_t ptr = 0;

    while (ptr < str_len){
        char c = str_buff[ptr];

		if(c != ' ' && c != '\t'){
            break;
        }

        ptr++;
    }

    size_t from = ptr == str_len ? 0 : ptr;
    size_t len = ptr == str_len ? 0 : str_len - ptr + 1;

    char *strip_raw_str = factory_clone_raw_str_range(from, len, str_buff, ((VM *)context)->fake_allocator);
    ObjHeader *strip_str_obj = vmu_create_str_obj(&strip_raw_str, context);

	return OBJ_VALUE(strip_str_obj);
}

Value native_fn_str_rstrip(uint8_t argsc, Value *values, Value *target, void *context){
	StrObj *target_str = VALUE_TO_STR(target);

    sidx_t str_len = target_str->len;
    char *str_buff = target_str->buff;

    sidx_t ptr = str_len - 1;

	while(ptr >= 0){
		char c = str_buff[ptr];

		if(c != ' ' && c != '\t'){
            break;
        }

        ptr--;
	}

    size_t len = str_len == 0 ? 0 : ptr + 1;

	char *strip_raw_str = factory_clone_raw_str_range(0, len, str_buff, ((VM *)context)->fake_allocator);
    ObjHeader *strip_str_obj = vmu_create_str_obj(&strip_raw_str, context);

    return OBJ_VALUE(strip_str_obj);
}

Value native_fn_str_strip(uint8_t argsc, Value *values, Value *target, void *context){
	StrObj *target_str = VALUE_TO_STR(target);

    sidx_t str_len = target_str->len;
    char *str_buff = target_str->buff;

    char left_set = 1;
    char right_set = 1;
    sidx_t left_ptr = 0;
	sidx_t right_ptr = str_len == 0 ? 0 : str_len - 1;

    while(left_ptr < str_len || right_ptr >= 0){
        if(left_set && left_ptr < str_len){
            char a = str_buff[left_ptr];

            if(a == ' ' || a == '\t'){
                left_ptr++;
            }else{
                left_set = 0;
            }
        }
        if(right_set && right_ptr >= 0){
            char b = str_buff[right_ptr];

            if(b == ' ' || b == '\t'){
                right_ptr--;
            }else{
                right_set = 0;
            }
        }

        if(left_ptr > right_ptr){
            break;
        }
        if(!left_set && !right_set){
            break;
        }
    }

    sidx_t len = left_ptr > right_ptr ? 0 : right_ptr - left_ptr + 1;
    sidx_t from = left_ptr > right_ptr ? 0 : left_ptr;

    char *strip_raw_str = factory_clone_raw_str_range(from, len, str_buff, ((VM *)context)->fake_allocator);
    ObjHeader *strip_str_obj = vmu_create_str_obj(&strip_raw_str, context);

    return OBJ_VALUE(strip_str_obj);
}

Value native_fn_str_lower(uint8_t argsc, Value *values, Value *target, void *context){
	StrObj *target_str = VALUE_TO_STR(target);

    char *lower_raw_str = factory_clone_raw_str(target_str->buff, ((VM *)context)->fake_allocator);
    sidx_t lower_len = target_str->len;

	for(sidx_t i = 0; i < lower_len; i++){
		char c = lower_raw_str[i];
        lower_raw_str[i] = c >= 'A' && c <= 'Z' ? c - 65 + 97 : c;
	}

    ObjHeader *lower_str_obj = vmu_create_str_obj(&lower_raw_str, context);

	return OBJ_VALUE(lower_str_obj);
}

Value native_fn_str_upper(uint8_t argsc, Value *values, Value *target, void *context){
	StrObj *target_str = VALUE_TO_STR(target);

	char *upper_raw_str = factory_clone_raw_str(target_str->buff, ((VM *)context)->fake_allocator);
    sidx_t upper_len = target_str->len;

	for(sidx_t i = 0; i < upper_len; i++){
		char c = upper_raw_str[i];
        upper_raw_str[i] = c >= 'a' && c <= 'z' ? c - 97 + 65 : c;
	}

    ObjHeader *upper_str_obj = vmu_create_str_obj(&upper_raw_str, context);

	return OBJ_VALUE(upper_str_obj);
}

Value native_fn_str_title(uint8_t argsc, Value *values, Value *target, void *context){
    StrObj *target_str = VALUE_TO_STR(target);

    char *title_raw_str = factory_clone_raw_str(target_str->buff, ((VM *)context)->fake_allocator);
    sidx_t title_len = target_str->len;

    char state = 0;

    for(sidx_t i = 0; i < title_len; i++){
        char c = title_raw_str[i];

        if(state){
            if((c >= 'a' && c <= 'z')){
                title_raw_str[i] = c - 97 + 65;
                state = 0;
                continue;
            }
            if(c >= 'A' && c <= 'Z'){
                state = 0;
            }
        }else{
            if((c < 'a' || c > 'z') && (c < 'A' || c > 'Z')){
                state = 1;
                continue;
            }
            if(c >= 'A' && c <= 'Z'){
                title_raw_str[i] = c - 65 + 97;
            }
        }
    }

    ObjHeader *title_str_obj = vmu_create_str_obj(&title_raw_str, context);

    return OBJ_VALUE(title_str_obj);
}

Value native_fn_str_cmp(uint8_t argsc, Value *values, Value *target, void *context){
	StrObj *target_str = VALUE_TO_STR(target);
	Value *cmp_value = &values[0];

	if(!IS_VALUE_STR(cmp_value)){
        vmu_error(context, "Expect a string, but got something else");
    }

	StrObj *cmp_str = VALUE_TO_STR(cmp_value);

	return INT_VALUE((int64_t)strcmp(target_str->buff, cmp_str->buff));
}

Value native_fn_str_cmp_ic(uint8_t argsc, Value *values, Value *target, void *context){
	StrObj *target_str = VALUE_TO_STR(target);
	Value *cmp_value = &values[0];
	StrObj *cmp_str = NULL;

	if(!IS_VALUE_STR(cmp_value)){
        vmu_error(context, "Expect a string, but got something else");
    }

	cmp_str = VALUE_TO_STR(cmp_value);

	if(target_str->len < cmp_str->len){
        return INT_VALUE(-1);
    }else if(target_str->len > cmp_str->len){
        return INT_VALUE(1);
    }

	for(sidx_t i = 0; i < target_str->len; i++){
		char ca = target_str->buff[i];
		char cb = cmp_str->buff[i];

		if(ca >= 'A' && ca <= 'Z'){
            ca = ca - 65 + 97;
        }
		if(cb >= 'A' && cb <= 'Z'){
            cb = cb - 65 + 97;
        }

		if(ca == cb){
            continue;
        }

		if(ca > cb){
            return INT_VALUE(1);
        }else if(ca < cb){
            return INT_VALUE(-1);
        }
	}

	return INT_VALUE(0);
}

NativeFnInfo *native_str_get(char *symbol, Allocator *allocator){
    if(!str_symbols){
        str_symbols = FACTORY_LZHTABLE(allocator);

        factory_add_native_fn_info("code", 0, native_fn_str_code, str_symbols, allocator);
        factory_add_native_fn_info("insert", 2, native_fn_str_insert, str_symbols, allocator);
        factory_add_native_fn_info("remove", 2, native_fn_str_remove, str_symbols, allocator);
        factory_add_native_fn_info("hash", 0, native_fn_str_hash, str_symbols, allocator);
        factory_add_native_fn_info("is_runtime", 0, native_fn_str_is_runtime, str_symbols, allocator);
        factory_add_native_fn_info("size", 0, native_fn_str_size, str_symbols, allocator);
        factory_add_native_fn_info("char_at", 1, native_fn_str_char_at, str_symbols, allocator);
        factory_add_native_fn_info("sub_str", 2, native_fn_str_sub_str, str_symbols, allocator);
        factory_add_native_fn_info("split", 1, native_fn_str_split, str_symbols, allocator);
        factory_add_native_fn_info("lstrip", 0, native_fn_str_lstrip, str_symbols, allocator);
        factory_add_native_fn_info("rstrip", 0, native_fn_str_rstrip, str_symbols, allocator);
        factory_add_native_fn_info("strip", 0, native_fn_str_strip, str_symbols, allocator);
        factory_add_native_fn_info("lower", 0, native_fn_str_lower, str_symbols, allocator);
        factory_add_native_fn_info("upper", 0, native_fn_str_upper, str_symbols, allocator);
        factory_add_native_fn_info("title", 0, native_fn_str_title, str_symbols, allocator);
        factory_add_native_fn_info("cmp", 1, native_fn_str_cmp, str_symbols, allocator);
        factory_add_native_fn_info("cmp_ic", 1, native_fn_str_cmp_ic, str_symbols, allocator);
    }

    return (NativeFnInfo *)lzhtable_get((uint8_t *)symbol, strlen(symbol), str_symbols);
}

#endif
