#ifndef NATIVE_STR
#define NATIVE_STR

#include "types.h"
#include "value.h"
#include "memory.h"
#include "vm_utils.h"

static LZHTable *str_symbols = NULL;

Value native_fn_str_length(uint8_t argsc, Value *values, void *target, VM *vm){
		Str *str = (Str *)target;
		return INT_VALUE(str->len);
}

Value native_fn_str_char_at(uint8_t argc, Value *values, void *target, VM *vm){
    Str *str = (Str *)target;
    Value *index_value = &values[0];
    int64_t index = -1;

    VALIDATE_INDEX(index_value, index, str->len)
    
    Value value = {0};

    if(!vm_utils_range_str_obj(index, index, str->buff, &value, vm))
		vm_utils_error(vm, "Out of memory");
    
    return value;
}

Value native_fn_str_sub_str(uint8_t argc, Value *values, void *target, VM *vm){
    Str *str = (Str *)target;
    Value *from_value = &values[0];
    Value *to_value = &values[1];
    int64_t from = -1;
    int64_t to = -1;

    VALIDATE_INDEX_NAME(from_value, from, str->len, "from")
    VALIDATE_INDEX_NAME(to_value, to, str->len, "to")

    if(from > to) 
        vm_utils_error(vm, "Illegal index 'from'(%ld). Must be less or equals to 'to'(%ld)", from, to);

    Value value = {0};

    if(!vm_utils_range_str_obj(from, to, str->buff, &value, vm))
		vm_utils_error(vm, "Out of memory");

    return value;
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
    Str *str = (Str *)target;
    Value *by_value = &values[0];
    Str *by = NULL;

    if(!IS_STR(by_value))
        vm_utils_error(vm, "Expect string as 'by' to split");
    
    by = TO_STR(by_value);

    if(by->len != 1)
		vm_utils_error(vm, "Expect string of length 1 as 'by' to split");

    Obj *list_obj = vm_utils_list_obj(vm);
    if(!list_obj) vm_utils_error(vm, "Out of memory");

    DynArr *list = list_obj->value.list;
	char coincidence = 0;
    size_t from = 0;
    size_t to = 0;
    size_t i = 0;

    while (i < str->len){
        char c = str->buff[i++];
        
        if(c == by->buff[0]){
            to = i - 1;
			size_t len = to - from;

			if(!coincidence) coincidence = 1;
			
			Value str_value = {0};

			if(len == 0){
                if(!vm_utils_empty_str_obj(&str_value, vm))
                    vm_utils_error(vm, "Out of memory");
            }else{
                if(!vm_utils_range_str_obj(from, to - 1, str->buff, &str_value, vm))
                    vm_utils_error(vm, "Out of memory");
            }

			if(dynarr_insert(&str_value, list))
                vm_utils_error(vm, "Out of memory");

            from = i;
        }
    }

	if(coincidence){
		Value str_value = {0};

    	if(!vm_utils_range_str_obj(from, i - 1, str->buff, &str_value, vm))
            vm_utils_error(vm, "Out of memory");

		if(dynarr_insert(&str_value, list))
            vm_utils_error(vm, "Out of memory");
	}

    return OBJ_VALUE(list_obj);
}

Value native_fn_str_lstrip(uint8_t argsc, Value *values, void *target, VM *vm){
	Str *str = (Str *)target;

	if(str->len == 0)
		vm_utils_error(vm, "Expect a not empty string");

	size_t from = 0;

	while(1){
		char cs = str->buff[from++];
		if(cs != ' ' && cs != '\t') break;
		if(from >= str->len) break;
	}

	Value value = {0};

	if(!vm_utils_range_str_obj(from - 1, str->len - 1, str->buff, &value, vm))
		vm_utils_error(vm, "Out of memory");

	return value;
}

Value native_fn_str_rstrip(uint8_t argsc, Value *values, void *target, VM *vm){
	Str *str = (Str *)target;

	if(str->len == 0)
		vm_utils_error(vm, "Expect a not empty string");

	size_t to = str->len;
	
	while(1){
		char cs = str->buff[--to];
		if(to == 0) break;
		if(cs != ' ' && cs != '\t') break;
	}

	Value value = {0};

	if(!vm_utils_range_str_obj(0, to, str->buff, &value, vm))
		vm_utils_error(vm, "Out of memory");

	return value;
}

Value native_fn_str_strip(uint8_t argsc, Value *values, void *target, VM *vm){
	Str *str = (Str *)target;

	if(str->len == 0)
		vm_utils_error(vm, "Expect a not empty string");

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

	Value value = {0};

	if(!vm_utils_range_str_obj(from, to, str->buff, &value, vm))
		vm_utils_error(vm, "Out of memory");

	return value;
}

Value native_fn_str_lower(uint8_t argsc, Value *values, void *target, VM *vm){
	Str *str = (Str *)target;
	Value value = {0};

	if(!vm_utils_clone_str_obj(str->buff, &value, vm))
		vm_utils_error(vm, "Out of memory");

	Str *out_str = TO_STR(&value);

	for(size_t i = 0; i < out_str->len; i++){
		char c = out_str->buff[i];
		if(c < 'A' || c > 'Z') continue;
		out_str->buff[i] = c - 65 + 97;
	}

	return value;
}

Value native_fn_str_upper(uint8_t argsc, Value *values, void *target, VM *vm){
	Str *str = (Str *)target;
	Value value = {0};

	if(!vm_utils_clone_str_obj(str->buff, &value, vm))
		vm_utils_error(vm, "Out of memory");

	Str *out_str = value.literal.obj->value.str;

	for(size_t i = 0; i < out_str->len; i++){
		char c = out_str->buff[i];
		if(c < 'a' || c > 'z') continue;
		out_str->buff[i] = c - 97 + 65;
	}

	return value;
}

Value native_fn_str_title(uint8_t argsc, Value *values, void *target, VM *vm){
    Str *str = (Str *)target;
    Value value = {0};

    if(!vm_utils_clone_str_obj(str->buff, &value, vm))
		vm_utils_error(vm, "Out of memory");

    Str *out_str = value.literal.obj->value.str;

    for (size_t i = 0; i < out_str->len; i++){
        char c = out_str->buff[i];
        char before = i == 0 ? '\0' : out_str->buff[i - 1];

        if(c < 'a' || c > 'z') continue;
        
        if(before == ' ' || before == '\t' || i == 0)
            out_str->buff[i] = c - 97 + 65;
    }
    
    return value;
}

Value native_fn_str_cmp(uint8_t argsc, Value *values, void *target, VM *vm){
	Str *s0 = (Str *)target;
	Value *vs1 = &values[0];
	Str *s1 = NULL;

	if(!IS_STR(vs1))
		vm_utils_error(vm, "Expect a string, but got something else");

	s1 = TO_STR(vs1);

	return INT_VALUE((int64_t)strcmp(s0->buff, s1->buff));
}

Value native_fn_str_cmp_ic(uint8_t argsc, Value *values, void *target, VM *vm){
	Str *s0 = (Str *)target;
	Value *vs1 = &values[0];
	Str *s1 = NULL;

	if(!IS_STR(vs1))
		vm_utils_error(vm, "Expect a string, but got something else");

	s1 = TO_STR(vs1);

	if(s0->len < s1->len) return INT_VALUE(-1);
	else if(s0->len > s1->len) return INT_VALUE(1);

	for(size_t i = 0; i < s0->len; i++){
		char ca = s0->buff[i];
		char cb = s1->buff[i];
		
		if(ca >= 'A' && ca <= 'Z') ca = ca - 65 + 97;
		if(cb >= 'A' && cb <= 'Z') cb = cb - 65 + 97;

		if(ca == cb) continue;

		if(ca > cb) return INT_VALUE(1);
		else if(ca < cb) return INT_VALUE(-1);
	}

	return INT_VALUE(0);
}

Obj *native_str_get(char *symbol, void *target, VM *vm){
    if(!str_symbols){
        str_symbols = runtime_lzhtable();
        runtime_add_native_fn_info("length", 0, native_fn_str_length, str_symbols);
        runtime_add_native_fn_info("char_at", 1, native_fn_str_char_at, str_symbols);
        runtime_add_native_fn_info("sub_str", 2, native_fn_str_sub_str, str_symbols);
        runtime_add_native_fn_info("char_code", 1, native_fn_str_char_code, str_symbols);
        runtime_add_native_fn_info("split", 1, native_fn_str_split, str_symbols);
        runtime_add_native_fn_info("lstrip", 0, native_fn_str_lstrip, str_symbols);
        runtime_add_native_fn_info("rstrip", 0, native_fn_str_rstrip, str_symbols);
        runtime_add_native_fn_info("strip", 0, native_fn_str_strip, str_symbols);
        runtime_add_native_fn_info("lower", 0, native_fn_str_lower, str_symbols);
        runtime_add_native_fn_info("upper", 0, native_fn_str_upper, str_symbols);
        runtime_add_native_fn_info("title", 0, native_fn_str_title, str_symbols);
        runtime_add_native_fn_info("cmp", 1, native_fn_str_cmp, str_symbols);
        runtime_add_native_fn_info("cmp_ic", 1, native_fn_str_cmp_ic, str_symbols);
    }

    size_t key_size = strlen(symbol);
    NativeFnInfo *native_fn_info = (NativeFnInfo *)lzhtable_get((uint8_t *)symbol, key_size, str_symbols);
    
    if(native_fn_info){
        Obj *native_obj_fn = vm_utils_native_fn_obj(
            native_fn_info->arity,
            symbol,
            target,
            native_fn_info->raw_native,
            vm
        );

        if(!native_obj_fn){
            vm_utils_error(vm, "Out of memory");
        }

        return native_obj_fn;
    }

    return NULL;
}

#endif
