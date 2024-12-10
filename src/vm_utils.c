#include "vm_utils.h"
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

//> Private Interface
void destroy_dict_values(void *key, void *value);
void destroy_globals_values(void *ptr);
void destroy_obj(Obj *obj, VM *vm);
void mark_objs(VM *vm);
void sweep_objs(VM *vm);
void gc(VM *vm);
//< Private Interface

//> Private Implementation
void destroy_dict_values(void *key, void *value){
    free(key);
    free(value);
}

void destroy_globals_values(void *ptr){
	free(ptr);
}

void destroy_obj(Obj *obj, VM *vm){
    Obj *prev = obj->prev;
    Obj *next = obj->next;

    if(prev) prev->next = next;
	if(next) next->prev = prev;

	if(vm->head == obj) vm->head = next;
	if(vm->tail == obj) vm->tail = prev;

	switch(obj->type){
		case STR_OTYPE:{
			Str *str = obj->value.str;
			if(!str->core) free(str->buff);
            free(str);
			break;
		}
		case LIST_OTYPE:{
			DynArr *list = obj->value.list;
			dynarr_destroy(list);
			break;
		}
        case DICT_OTYPE:{
            LZHTable *table = obj->value.dict;
            lzhtable_destroy(destroy_dict_values, table);
            break;
        }
        case NATIVE_FN_OTYPE:{
            NativeFn *native_fn = obj->value.native_fn;
            if(!native_fn->unique) free(native_fn);
            break;
        }
        case MODULE_OTYPE:{
            break;
        }
		default:{
			assert("Illegal object type");
		}
	}

	free(obj);
}

void mark_objs(VM *vm){
    // globals
    Frame *frame = &vm->frame_stack[vm->frame_ptr - 1];
    LZHTableNode *node = frame->fn->module->globals->head;
    
    while (node){
        LZHTableNode *next = node->next_table_node;
        Value *value = (Value *)node->value;
        if(value->type != OBJ_VTYPE) continue;
        
        Obj *obj = value->literal.obj;
        if(obj->marked) continue;
        
        obj->marked = 1;
        node = next;
    }
    
    // locals
    for (size_t frame_ptr = 0; frame_ptr < vm->frame_ptr; frame_ptr++){
        Frame *frame = &vm->frame_stack[frame_ptr];
        
        for (size_t local_ptr = 0; local_ptr < LOCALS_LENGTH; local_ptr++){
            Value *value = &frame->locals[local_ptr];
            if(value->type != OBJ_VTYPE) continue;
            
            Obj *obj = value->literal.obj;
            if(obj->marked) continue;
            
            obj->marked = 1;
        }
    }
}

void sweep_objs(VM *vm){
    Obj *obj = vm->head;
    Obj *prev = NULL;

    while (obj){
        Obj *next = obj->next;

        if(obj->marked){
            obj->marked = 0;
            prev = obj;
            obj = next;
            continue;
        }
        
        if(obj == vm->head) 
            vm->head = next;
        if(obj == vm->tail)
            vm->tail = prev;
        if(prev)
            prev->next = next;
            
        destroy_obj(obj, vm);

        obj = next;
    }    
}

void gc(VM *vm){
    mark_objs(vm);
    sweep_objs(vm);
}
//< Private Implementation

void vm_utils_error(VM *vm, char *msg, ...){
    va_list args;
	va_start(args, msg);

	fprintf(stderr, "Runtime error:\n\t");
	vfprintf(stderr, msg, args);
    fprintf(stderr, "\n");

	va_end(args);
    vm_utils_clean_up(vm);

    longjmp(vm->err_jmp, 1);
}

int vm_utils_is_i64(Value *value, int64_t *i64){
    if(value->type != INT_VTYPE) return 0;
    if(i64) *i64 = value->literal.i64;
    return 1;
}

int vm_utils_is_str(Value *value, Str **str){
    if(value->type != OBJ_VTYPE) return 0;
    
    Obj *obj = value->literal.obj;

    if(obj->type == STR_OTYPE){
        Str *ostr = obj->value.str;
        if(str) *str = ostr;
    
        return 1;
    }

    return 0;
}

int vm_utils_is_list(Value *value, DynArr **list){
	if(value->type != OBJ_VTYPE) return 0;
	
	Obj *obj = value->literal.obj;

	if(obj->type == LIST_OTYPE){
		DynArr *olist = obj->value.list;
		if(list) *list = olist;
		return 1;
	}

	return 0;
}

int vm_utils_is_dict(Value *value, LZHTable **dict){
	if(value->type != OBJ_VTYPE) return 0;
	
	Obj *obj = value->literal.obj;

	if(obj->type == DICT_OTYPE){
		LZHTable *odict = obj->value.dict;
		if(dict) *dict = odict;
		return 1;
	}

	return 0;
}

int vm_utils_is_function(Value *value, Fn **out_fn){
	if(value->type != OBJ_VTYPE) return 0;
	
	Obj *obj = value->literal.obj;

	if(obj->type == FN_OTYPE){
		Fn *ofn = obj->value.fn;
		if(out_fn) *out_fn = ofn;
		return 1;
	}

	return 0;
}

int vm_utils_is_module(Value *value, Module **out_module){
	if(value->type != OBJ_VTYPE) return 0;
	
	Obj *obj = value->literal.obj;

	if(obj->type == MODULE_OTYPE){
		Module *module_obj = obj->value.module;
		if(out_module) *out_module = module_obj;
		return 1;
	}

	return 0;
}

int vm_utils_is_native_function(Value *value, NativeFn **out_native_fn){
	if(value->type != OBJ_VTYPE) return 0;
	
	Obj *obj = value->literal.obj;

	if(obj->type == NATIVE_FN_OTYPE){
		NativeFn *ofn = obj->value.native_fn;
		if(out_native_fn) *out_native_fn = ofn;
		return 1;
	}

	return 0;
}

Obj *vm_utils_empty_str_obj(Value *out_value, VM *vm){
	char *buff = NULL;
	Str *str = NULL;
	Obj *str_obj = NULL;

	buff = (char *)malloc(1);
	if(!buff) goto FAIL;
	buff[0] = '\0';

	str = vm_utils_uncore_alloc_str(buff, vm);
	if(!str) goto FAIL;

	str_obj = vm_utils_obj(STR_OTYPE, vm);
	if(!str_obj) goto FAIL;

	str_obj->value.str = str;

	goto OK;

FAIL:
	free(buff);
	free(str);
	free(str_obj);
	return NULL;

OK:
	*out_value = (Value){.type = OBJ_VTYPE, .literal.obj = str_obj};
	return str_obj;
}

Obj *vm_utils_clone_str_obj(char *buff, Value *out_value, VM *vm){
	Str *str = NULL;
	Obj *str_obj = NULL;

	str = vm_utils_uncore_alloc_str(buff, vm);
	if(!str) goto FAIL;

	str_obj = vm_utils_obj(STR_OTYPE, vm);
	if(!str_obj) goto FAIL;

	str_obj->value.str = str;

	goto OK;

FAIL:
	free(buff);
	free(str);
	free(str_obj);
	return NULL;

OK:
	*out_value = (Value){.type = OBJ_VTYPE, .literal.obj = str_obj};
	return str_obj;

}

Obj *vm_utils_range_str_obj(size_t from, size_t to, char *buff, Value *out_value, VM *vm){
	size_t range_buff_len = to - from + 1;
	char *range_buff = NULL;
	Str *str = NULL;
	Obj *str_obj = NULL;
	
    range_buff = (char *)malloc(range_buff_len + 1);
    if(!range_buff) goto FAIL;

	memcpy(range_buff, buff + from, range_buff_len);
	range_buff[range_buff_len] = '\0';

	str = vm_utils_uncore_str(range_buff, vm);
	if(!str) goto FAIL;

	str_obj = vm_utils_obj(STR_OTYPE, vm);
	if(!str_obj) goto FAIL;

	str_obj->value.str = str;

	goto OK;

FAIL:
    free(range_buff);
	free(str);
	free(str_obj);
	return NULL;

OK:
	*out_value = (Value){.type = OBJ_VTYPE, .literal.obj = str_obj};
	return str_obj;
}

Value *vm_utils_clone_value(Value *value, VM *vm){
    Value *clone = (Value *)malloc(sizeof(Value));
    if(!clone) return NULL;
    memcpy(clone, value, sizeof(Value));
    return clone;
}

Str *vm_utils_core_str(char *buff, uint32_t hash, VM *vm){
    size_t buff_len = strlen(buff);
    Str *str = (Str *)malloc(sizeof(Str));
    
    if(!str) return NULL;

    str->core = 1;
    str->len = buff_len;
    str->buff = buff;

    return str;
}

Str *vm_utils_uncore_str(char *buff, VM *vm){
    size_t buff_len = strlen(buff);
    Str *str = (Str *)malloc(sizeof(Str));

    if(!str) return NULL;

    str->core = 0;
    str->len = buff_len;
    str->buff = buff;

    return str;
}

Str *vm_utils_uncore_alloc_str(char *buff, VM *vm){
    size_t buff_len = strlen(buff);
	char *new_buff = (char *)malloc(buff_len + 1);
        
    if(!new_buff) return NULL;
	memcpy(new_buff, buff, buff_len);
	new_buff[buff_len] = '\0';

    Str *str = (Str *)malloc(sizeof(Str));

    if(!str){
		free(new_buff);
		return NULL;
	}

    str->core = 0;
    str->len = buff_len;
    str->buff = new_buff;

    return str;
}

Obj *vm_utils_obj(ObjType type, VM *vm){
    if(vm->objs_size >= 67108864)
        gc(vm);

    Obj *obj = malloc(sizeof(Obj));
    if(!obj) return NULL;

    memset(obj, 0, sizeof(Obj));
    obj->type = type;
    
    if(vm->tail){
        obj->prev = vm->tail;
        vm->tail->next = obj;
    }else vm->head = obj;

    vm->tail = obj;
    vm->objs_size += sizeof(Obj);

    return obj;
}

NativeFn *vm_utils_native_function(
    int arity,
    char *name,
    void *target,
    RawNativeFn native,
    VM *vm
){
    size_t name_len = strlen(name);
    assert(name_len < NAME_LEN - 1);

    NativeFn *native_fn = (NativeFn *)malloc(sizeof(NativeFn));
    if(!native_fn) return NULL;

    native_fn->unique = 0;
    native_fn->arity = arity;
    memcpy(native_fn->name, name, name_len);
    native_fn->name[name_len] = '\0';
    native_fn->name_len = name_len;
    native_fn->target = target;
    native_fn->native = native;

    return native_fn;
}

DynArr *vm_utils_dyarr(VM *vm){
    DynArr *dynarr = dynarr_create(sizeof(Value), NULL);
    if(!dynarr) return NULL;
    return dynarr;
}

Obj *vm_utils_list_obj(VM *vm){
    DynArr *list = dynarr_create(sizeof(Value), NULL);
    if(!list) return NULL;

    Obj *list_obj = vm_utils_obj(LIST_OTYPE, vm);
    
    if(!list_obj){
        dynarr_destroy(list);
        return NULL;
    }

    list_obj->value.list = list;

    return list_obj;
}

LZHTable *vm_utils_dict(VM *vm){
    LZHTable *table = lzhtable_create(17, NULL);
    if(!table) return NULL;
    return table;
}

Obj *vm_utils_dict_obj(VM *vm){
    LZHTable *dict = lzhtable_create(17, NULL);
    if(!dict) return NULL;

    Obj *dict_obj = vm_utils_obj(DICT_OTYPE, vm);
    
    if(!dict_obj){
        lzhtable_destroy(NULL, dict);
        return NULL;
    }

    dict_obj->value.dict = dict;

    return dict_obj;
}

void *assert_ptr(void *ptr, VM *vm){
    if(ptr) return ptr;
    vm_utils_error(vm, "Out of memory");
    return NULL;
}

uint32_t vm_utils_hash_obj(Obj *obj){
    switch (obj->type){
        case STR_OTYPE :{
            Str *str = obj->value.str;
            uint32_t hash = lzhtable_hash((uint8_t *)str->buff, str->len);
            return hash;
        }
        default:{
            uint32_t hash = lzhtable_hash((uint8_t *)&obj->value, sizeof(obj->value));
            return hash;
        }
    }
}

uint32_t vm_utils_hash_value(Value *value){
    switch (value->type){
        case EMPTY_VTYPE:
        case BOOL_VTYPE:
        case INT_VTYPE:{
            uint32_t hash = lzhtable_hash((uint8_t *)&value->literal.i64, sizeof(int64_t));
            return hash;
        }
        default:{
            Obj *obj = value->literal.obj;
            return vm_utils_hash_obj(obj);
        }
    }   
}

void clean_up_module(Module *module){
	LZHTable *globals = module->globals;
	// Due to the same module can be imported in others modules
	// we need to make sure we already handled its globals in order
	// to no make a double free on them.
	if(globals){
		LZHTableNode *global_node = module->globals->head;

		while(global_node){
			LZHTableNode *next = global_node->next_table_node;
			free(global_node->value);
			global_node = next;
		}

		module->globals = NULL;
	}

	LZHTableNode *symbol_node = module->symbols->head;
    
    while (symbol_node){
        LZHTableNode *next = symbol_node->next_table_node;
		ModuleSymbol *symbol = (ModuleSymbol *)symbol_node->value;

        if(symbol->type == MODULE_MSYMTYPE)
			clean_up_module(symbol->value.module);

        symbol_node = next;
    }

}

void vm_utils_clean_up(VM *vm){
	Obj *obj = vm->head;

	while(obj){
		Obj *next = obj->next;
		destroy_obj(obj, vm);
		obj = next;
	}

    clean_up_module(vm->module);
}
