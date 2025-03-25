#ifndef NATIVE_STR
#define NATIVE_STR

#include "rtypes.h"
#include "memory.h"
#include "factory.h"
#include "vm_utils.h"

static LZHTable *str_symbols = NULL;

Value native_fn_str_insert(uint8_t argsc, Value *values, void *target, VM *vm){
	Str *target_str = (Str *)target;
    Value *at_value = &values[0];
    Value *value_str = &values[1];

    if(!IS_INT(at_value)){
        vmu_error(vm, "Illegal type for argument 'at': must be of type 'int'");
    }

    int64_t at = TO_INT(at_value);

    if(at < 0){
        vmu_error(vm, "Illegal value for argument 'at': indexes must be greater than 0");
    }
    if(at >= (int64_t)(target_str->len + 1)){
        vmu_error(vm, "Illegal value for argument 'at': cannot be greater than %d", target_str->len);
    }

    if(!IS_STR(value_str)){
        vmu_error(vm, "Argument 1: must be of type 'str'");
    }

    Str *str = TO_STR(value_str);
    size_t raw_str_len = target_str->len + str->len;
    char *raw_str = MEMORY_ALLOC(char, raw_str_len + 1, vm->rtallocator);

    memcpy(raw_str, target_str->buff, at);
    memcpy(raw_str + at, str->buff, str->len);
    memcpy(raw_str + at + str->len, target_str->buff + at, target_str->len - (at));

    raw_str[raw_str_len] = '\0';

    Obj *str_obj = vmu_str_obj(&raw_str, vm);

	return OBJ_VALUE(str_obj);
}

Value native_fn_str_remove(uint8_t argsc, Value *values, void *target, VM *vm){
	Str *target_str = (Str *)target;
    Value *from_value = &values[0];
    Value *to_value = &values[1];
    int64_t from = -1;
    int64_t to = -1;

    if(target_str->len == 0){
        vmu_error(vm, "Cannot remove on empty strings");
    }

    VALIDATE_INDEX_ARG("from", from_value, from, target_str->len)
    VALIDATE_INDEX_ARG("to", to_value, to, target_str->len)

    if(from > to){
        vmu_error(vm, "Illegal value for argument 'from': cannot be greater than argument 'to'");
    }

    size_t raw_str_len = target_str->len - (to - from + 1);
    char *raw_str = MEMORY_ALLOC(char, raw_str_len + 1, vm->rtallocator);

    memcpy(raw_str, target_str->buff, from);
    memcpy(raw_str + from, target_str->buff + to + 1, target_str->len - to  - 1);
    raw_str[raw_str_len] = '\0';

    Obj *str_obj = vmu_str_obj(&raw_str, vm);

	return OBJ_VALUE(str_obj);
}

Value native_fn_str_hash(uint8_t argsc, Value *values, void *target, VM *vm){
	Str *target_str = (Str *)target;
	return INT_VALUE((int64_t)target_str->hash);
}

Value native_fn_str_is_runtime(uint8_t argsc, Value *values, void *target, VM *vm){
	Str *target_str = (Str *)target;
	return BOOL_VALUE(target_str->runtime == 1);
}

Value native_fn_str_length(uint8_t argsc, Value *values, void *target, VM *vm){
	Str *target_str = (Str *)target;
	return INT_VALUE(target_str->len);
}

Value native_fn_str_char_at(uint8_t argc, Value *values, void *target, VM *vm){
    Str *target_str = (Str *)target;
    Value *index_value = &values[0];
    int64_t index = -1;

    VALIDATE_INDEX(index_value, index, target_str->len)

    char *raw_str = factory_clone_raw_str_range((size_t)index, 1, target_str->buff, vm->rtallocator);
    Obj *str_obj = vmu_str_obj(&raw_str, vm);

    return OBJ_VALUE(str_obj);
}

Value native_fn_str_sub_str(uint8_t argc, Value *values, void *target, VM *vm){
    Str *target_str = (Str *)target;
    Value *from_value = &values[0];
    Value *to_value = &values[1];
    int64_t from = -1;
    int64_t to = -1;

    VALIDATE_INDEX_NAME(from_value, from, target_str->len, "from")
    VALIDATE_INDEX_NAME(to_value, to, target_str->len, "to")

    if(from > to){
        vmu_error(vm, "Illegal index 'from'(%ld). Must be less or equals to 'to'(%ld)", from, to);
    }

    char *raw_sub_str = factory_clone_raw_str_range(from, to - from + 1, target_str->buff, vm->rtallocator);
    Obj *sub_str_obj = vmu_str_obj(&raw_sub_str, vm);

    return OBJ_VALUE(sub_str_obj);
}

Value native_fn_str_char_code(uint8_t argc, Value *values, void *target, VM *vm){
    Str *str = (Str *)target;
    Value *index_value = &values[0];
    int64_t index = -1;

    VALIDATE_INDEX(index_value, index, str->len)

    int64_t code = (int64_t)str->buff[index];

    return INT_VALUE(code);
}

Value native_fn_str_split(uint8_t argc, Value *values, void *target, VM *vm){
    Str *target_str = (Str *)target;
    Value *by_value = &values[0];
    Str *by_str = NULL;

    if(!IS_STR(by_value)){
        vmu_error(vm, "Expect string as 'by' to split");
    }

    by_str = TO_STR(by_value);

    if(by_str->len != 1){
        vmu_error(vm, "Expect string of length 1 as 'by' to split");
    }

    Obj *list_obj = vmu_list_obj(vm);
    DynArr *list = list_obj->content.list;
	char coincidence = 0;
    size_t from = 0;
    size_t to = 0;
    size_t i = 0;

    while (i < target_str->len){
        char c = target_str->buff[i++];

        if(c == by_str->buff[0]){
            to = i - 1;
			size_t len = to - from;

			if(!coincidence){
                coincidence = 1;
            }

            char *raw_str = factory_clone_raw_str_range(from, len, target_str->buff, vm->rtallocator);
            Obj *str_obj = vmu_str_obj(&raw_str, vm);

            dynarr_insert(&OBJ_VALUE(str_obj), list);

            from = i;
        }
    }

	if(coincidence){
        char *raw_str = factory_clone_raw_str_range(from, i - 1, target_str->buff, vm->rtallocator);
		Obj *str_obj = vmu_str_obj(&raw_str, vm);

        dynarr_insert(&OBJ_VALUE(str_obj), list);
	}

    return OBJ_VALUE(list_obj);
}

Value native_fn_str_lstrip(uint8_t argsc, Value *values, void *target, VM *vm){
	Str *target_str = (Str *)target;

	if(target_str->len == 0){
        vmu_error(vm, "Expect a not empty string");
    }

	size_t from = 0;
    size_t len = target_str->len;
    char *raw_str = target_str->buff;

	while(1){
		char c = raw_str[from++];

		if(c != ' ' && c != '\t'){break;}
		if(from >= len){break;}
	}

    char *strip_raw_str = factory_clone_raw_str_range(from - 1, len - from + 1, target_str->buff, vm->rtallocator);
    Obj *strip_str_obj = vmu_str_obj(&strip_raw_str, vm);

	return OBJ_VALUE(strip_str_obj);
}

Value native_fn_str_rstrip(uint8_t argsc, Value *values, void *target, VM *vm){
	Str *target_str = (Str *)target;

	if(target_str->len == 0){
        vmu_error(vm, "Expect a not empty string");
    }

	size_t to = target_str->len;
    char *raw_str = target_str->buff;

	while(1){
		char cs = raw_str[--to];

		if(to == 0){break;}
		if(cs != ' ' && cs != '\t'){break;}
	}

	char *strip_raw_str = factory_clone_raw_str_range(0, to + 1, raw_str, vm->rtallocator);
    Obj *strip_str_obj = vmu_str_obj(&strip_raw_str, vm);

    return OBJ_VALUE(strip_str_obj);
}

Value native_fn_str_strip(uint8_t argsc, Value *values, void *target, VM *vm){
	Str *str = (Str *)target;

	if(str->len == 0){
        vmu_error(vm, "Expect a not empty string");
    }

	char from_set = 0;
	size_t from = 0;
	char to_set = 0;
	size_t to = str->len - 1;

	for(size_t i = 0, o = str->len - 1; i < str->len; i++, o--){
		char cs = str->buff[i];
		char ce = str->buff[o];

		if((cs != ' ' && cs != '\t') && !from_set){
			from = i;
			from_set = 1;
		}

		if((ce != ' ' && ce != '\t') && !to_set){
			to = o;
			to_set = 1;
		}

		if(from_set && to_set) break;
		if(from == to) break;
	}

    char *strip_raw_str = factory_clone_raw_str_range(from, to - from + 1, str->buff, vm->rtallocator);
    Obj *strip_str_obj = vmu_str_obj(&strip_raw_str, vm);

    return OBJ_VALUE(strip_str_obj);
}

Value native_fn_str_lower(uint8_t argsc, Value *values, void *target, VM *vm){
	Str *target_str = (Str *)target;
    char *lower_raw_str = factory_clone_raw_str(target_str->buff, vm->rtallocator);
    Obj *lower_str_obj = vmu_str_obj(&lower_raw_str, vm);
    Str *lower_str = lower_str_obj->content.str;

	for(size_t i = 0; i < lower_str->len; i++){
		char c = lower_str->buff[i];
		if(c < 'A' || c > 'Z') continue;
		lower_str->buff[i] = c - 65 + 97;
	}

	return OBJ_VALUE(lower_str_obj);
}

Value native_fn_str_upper(uint8_t argsc, Value *values, void *target, VM *vm){
	Str *target_str = (Str *)target;
	char *upper_raw_str = factory_clone_raw_str(target_str->buff, vm->rtallocator);
    Obj *upper_str_obj = vmu_str_obj(&upper_raw_str, vm);
    Str *upper_str = upper_str_obj->content.str;

	for(size_t i = 0; i < upper_str->len; i++){
		char c = upper_str->buff[i];
		if(c < 'a' || c > 'z') continue;
		upper_str->buff[i] = c - 97 + 65;
	}

	return OBJ_VALUE(upper_str_obj);
}

Value native_fn_str_title(uint8_t argsc, Value *values, void *target, VM *vm){
    Str *target_str = (Str *)target;
    char *title_raw_str = factory_clone_raw_str(target_str->buff, vm->rtallocator);
    Obj *title_str_obj = vmu_str_obj(&title_raw_str, vm);
    Str *title_str = title_str_obj->content.str;

    for (size_t i = 0; i < title_str->len; i++){
        char c = title_str->buff[i];
        char before = i == 0 ? '\0' : title_str->buff[i - 1];

        if(c < 'a' || c > 'z'){continue;}

        if(before == ' ' || before == '\t' || i == 0){
            title_str->buff[i] = c - 97 + 65;
        }
    }

    return OBJ_VALUE(title_str_obj);
}

Value native_fn_str_cmp(uint8_t argsc, Value *values, void *target, VM *vm){
	Str *target_str = (Str *)target;
	Value *cmp_value = &values[0];

	if(!IS_STR(cmp_value)){
        vmu_error(vm, "Expect a string, but got something else");
    }

	Str *cmp_str = TO_STR(cmp_value);

	return INT_VALUE((int64_t)strcmp(target_str->buff, cmp_str->buff));
}

Value native_fn_str_cmp_ic(uint8_t argsc, Value *values, void *target, VM *vm){
	Str *target_str = (Str *)target;
	Value *cmp_value = &values[0];
	Str *cmp_str = NULL;

	if(!IS_STR(cmp_value)){
        vmu_error(vm, "Expect a string, but got something else");
    }

	cmp_str = TO_STR(cmp_value);

	if(target_str->len < cmp_str->len){return INT_VALUE(-1);}
	else if(target_str->len > cmp_str->len){return INT_VALUE(1);}

	for(size_t i = 0; i < target_str->len; i++){
		char ca = target_str->buff[i];
		char cb = cmp_str->buff[i];

		if(ca >= 'A' && ca <= 'Z'){ca = ca - 65 + 97;}
		if(cb >= 'A' && cb <= 'Z'){cb = cb - 65 + 97;}

		if(ca == cb){continue;}

		if(ca > cb){return INT_VALUE(1);}
		else if(ca < cb){return INT_VALUE(-1);}
	}

	return INT_VALUE(0);
}

Obj *native_str_get(char *symbol, void *target, VM *vm){
    if(!str_symbols){
        str_symbols = FACTORY_LZHTABLE(vm->rtallocator);
        factory_add_native_fn_info("insert", 2, native_fn_str_insert, str_symbols, vm->rtallocator);
        factory_add_native_fn_info("remove", 2, native_fn_str_remove, str_symbols, vm->rtallocator);
        factory_add_native_fn_info("hash", 0, native_fn_str_hash, str_symbols, vm->rtallocator);
        factory_add_native_fn_info("is_runtime", 0, native_fn_str_is_runtime, str_symbols, vm->rtallocator);
        factory_add_native_fn_info("length", 0, native_fn_str_length, str_symbols, vm->rtallocator);
        factory_add_native_fn_info("char_at", 1, native_fn_str_char_at, str_symbols, vm->rtallocator);
        factory_add_native_fn_info("sub_str", 2, native_fn_str_sub_str, str_symbols, vm->rtallocator);
        factory_add_native_fn_info("char_code", 1, native_fn_str_char_code, str_symbols, vm->rtallocator);
        factory_add_native_fn_info("split", 1, native_fn_str_split, str_symbols, vm->rtallocator);
        factory_add_native_fn_info("lstrip", 0, native_fn_str_lstrip, str_symbols, vm->rtallocator);
        factory_add_native_fn_info("rstrip", 0, native_fn_str_rstrip, str_symbols, vm->rtallocator);
        factory_add_native_fn_info("strip", 0, native_fn_str_strip, str_symbols, vm->rtallocator);
        factory_add_native_fn_info("lower", 0, native_fn_str_lower, str_symbols, vm->rtallocator);
        factory_add_native_fn_info("upper", 0, native_fn_str_upper, str_symbols, vm->rtallocator);
        factory_add_native_fn_info("title", 0, native_fn_str_title, str_symbols, vm->rtallocator);
        factory_add_native_fn_info("cmp", 1, native_fn_str_cmp, str_symbols, vm->rtallocator);
        factory_add_native_fn_info("cmp_ic", 1, native_fn_str_cmp_ic, str_symbols, vm->rtallocator);
    }

    size_t key_size = strlen(symbol);
    NativeFnInfo *native_fn_info = (NativeFnInfo *)lzhtable_get((uint8_t *)symbol, key_size, str_symbols);

    if(native_fn_info){
        Obj *native_obj_fn = vmu_native_fn_obj(
            native_fn_info->arity,
            symbol,
            target,
            native_fn_info->raw_native,
            vm
        );

        if(!native_obj_fn){
            vmu_error(vm, "Out of memory");
        }

        return native_obj_fn;
    }

    return NULL;
}

#endif
