#include "vm_utils.h"
#include "bstr.h"
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <dlfcn.h>

static Allocator allocator;

static void *raw_alloc(size_t size, void *ctx){
    return calloc(1, size);
}

static void *raw_realloc(void *ptr, size_t new_size, void *ctx){
    return realloc(ptr, new_size);
}

static void raw_dealloc(void *ptr, void *ctx){
    free(ptr);
}

allocator = {
    .ctx = NULL,
    .alloc = raw_alloc,
    .realloc = raw_realloc,
    .dealloc = raw_dealloc
};

#define CURRENT_FRAME(vm)(&vm->frame_stack[vm->frame_ptr - 1])
#define CURRENT_FN(vm)(CURRENT_FRAME(vm)->fn)
#define CURRENT_LOCATIONS(vm)(CURRENT_FN(vm)->locations)
#define FIND_LOCATION(index, arr)(dynarr_find(&((OPCodeLocation){.offset = index, .line = -1}), compare_locations, arr))
#define FRAME_AT(at, vm)(&vm->frame_stack[at])

//> PRIVATE INTERFACE
// ALLOCATION
void *lzalloc(size_t size);
void *lzrealloc(void *ptr, size_t new_size);
void lzdealloc(void *ptr);
// GARBAGE COLLECTOR
void clean_up_module(Module *module);
void destroy_dict_values(void *key, void *value);
void destroy_globals_values(void *ptr);
void destroy_obj(Obj *obj, VM *vm);
void mark_value(Value *value);
void mark_objs(VM *vm);
void sweep_objs(VM *vm);
void gc(VM *vm);
// ERROR REPORT
int compare_locations(void *a, void *b);
BStr *prepare_stacktrace(unsigned int spaces, VM *vm);
//< PRIVATE INTERFACE
//> PRIVATE IMPLEMENTATION
void *lzalloc(size_t size){
    return raw_alloc(size, allocator.ctx);
}

void *lzrealloc(void *ptr, size_t new_size){
    return raw_realloc(ptr, new_size, allocator.ctx);
}

void lzdealloc(void *ptr){
    raw_dealloc(ptr, allocator.ctx);
}

void clean_up_module(Module *module){
    if(module->shadow){return;}

    SubModule *submodule = module->submodule;
	LZHTable *globals = submodule->globals;
	// Due to the same module can be imported in others modules
	// we need to make sure we already handled its globals in order
	// to not make a double free on them.
	if(globals){
		LZHTableNode *global_node = submodule->globals->head;

		while(global_node){
			LZHTableNode *next = global_node->next_table_node;
			lzdealloc(global_node->value);
			global_node = next;
		}

		submodule->globals = NULL;
	}

	LZHTableNode *symbol_node = submodule->symbols->head;
    
    while (symbol_node){
        LZHTableNode *next = symbol_node->next_table_node;
		ModuleSymbol *symbol = (ModuleSymbol *)symbol_node->value;

        if(symbol->type == MODULE_MSYMTYPE){
            clean_up_module(symbol->value.module);
        }

        symbol_node = next;
    }
}

void destroy_dict_values(void *key, void *value){
    lzdealloc(key);
    lzdealloc(value);
}

void destroy_globals_values(void *ptr){
	lzdealloc(ptr);
}

void destroy_obj(Obj *obj, VM *vm){
    Obj *prev = obj->prev;
    Obj *next = obj->next;

    if(prev) prev->next = next;
	if(next) next->prev = prev;

	if(vm->head == obj){vm->head = next;}
	if(vm->tail == obj){vm->tail = prev;}

	switch(obj->type){
		case STR_OTYPE:{
			Str *str = obj->value.str;
			if(!str->core){lzdealloc(str->buff);}
            lzdealloc(str);
			break;
		}case ARRAY_OTYPE:{
            Array *array = obj->value.array;
            lzdealloc(array->values);
            lzdealloc(array);
            break;
        }case LIST_OTYPE:{
			DynArr *list = obj->value.list;
			dynarr_destroy(list);
			break;
		}case DICT_OTYPE:{
            LZHTable *table = obj->value.dict;
            lzhtable_destroy(destroy_dict_values, table);
            break;
        }case RECORD_OTYPE:{
			Record *record = obj->value.record;
			lzhtable_destroy(destroy_dict_values, record->key_values);
			lzdealloc(record);
			break;
		}case NATIVE_FN_OTYPE:{
            NativeFn *native_fn = obj->value.native_fn;
            if(!native_fn->unique){lzdealloc(native_fn);}
            break;
        }case MODULE_OTYPE:{
            break;
        }case NATIVE_LIB_OTYPE:{
            NativeLib *library = obj->value.native_lib;
            void *handler = library->handler;
            void (*znative_deinit)(void) = dlsym(handler, "znative_deinit");
            
            if(znative_deinit){
                znative_deinit();
            }else{
                fprintf(stderr, "Failed to deinit native library: not function present");
            }

            dlclose(handler);
            lzdealloc(library);

            break;
        }case FOREIGN_FN_OTYPE:{
            lzdealloc(obj->value.foreign_fn);
            break;
        }default:{
			assert("Illegal object type");
		}
	}

	lzdealloc(obj);
	vm->objs_size -= sizeof(Obj);
}

void mark_value(Value *value){
	if(value-> type != OBJ_VTYPE){return;}
	
	Obj *obj = value->literal.obj;
	
    if(obj->marked){return;}
    obj->marked = 1;
	
	switch(obj->type){
		case STR_OTYPE:{
			break;
		}case ARRAY_OTYPE:{
            Array *array = obj->value.array;
            Value *values = array->values;
            Value *value = NULL;

            for (int16_t i = 0; i < array->len; i++){
                value = &values[i];
                if(value->type != OBJ_VTYPE){continue;}
                mark_value(value);
            }
            
            break;
        }case LIST_OTYPE:{
			DynArr *list = obj->value.list;
			Value *value = NULL;
			
			for(size_t i = 0; i < DYNARR_LEN(list); i++){
				value = (Value *)DYNARR_GET(i, list);
				if(value->type != OBJ_VTYPE){continue;}
				mark_value(value);
			}
			
			break;
		}case DICT_OTYPE:{
            LZHTable *dict = obj->value.dict;
            LZHTableNode *current = dict->head;
			LZHTableNode *next = NULL;
			Value *value = NULL;
			
			while (current){
				next = current->next_table_node;
				value = (Value *)current->value;
				
				if(value->type != OBJ_VTYPE){
					current = next;
					continue;
				}
				
				mark_value(value);
				current = next;
			}
			
            break;
        }case RECORD_OTYPE:{
			Record *record = obj->value.record;
			LZHTable *key_values = record->key_values;
			
			if(!key_values){break;}
			
			LZHTableNode *current = key_values->head;
			LZHTableNode *next = NULL;
			Value *value = NULL;
			
			while (current){
				next = current->next_table_node;
				value = (Value *)current->value;
				
				if(value->type != OBJ_VTYPE){
					current = next;
					continue;
				}
				
				mark_value(value);
				current = next;
			}
			
			break;
		}case NATIVE_FN_OTYPE:{
            break;
        }case MODULE_OTYPE:{
			Module *module = obj->value.module;
			LZHTableNode *current = MODULE_GLOBALS(module)->head;
			LZHTableNode *next = NULL;
			Value *value = NULL;
			
			while (current){
				next = current->next_table_node;
				value = (Value *)current->value;
				
				if(value->type != OBJ_VTYPE){
					current = next;
					continue;
				}
				
				mark_value(value);
				current = next;
			}
    
            break;
        }default:{
			assert("Illegal object type");
		}
	}
}

void mark_objs(VM *vm){
    //> MARKING GLOBALS
    Module *module = vm->module;
    LZHTableNode *current = MODULE_GLOBALS(module)->head;
    LZHTableNode *next = NULL;
    Value *value = NULL;
    
    while (current){
        next = current->next_table_node;
        value = (Value *)current->value;
        mark_value(value);
        current = next;
    }
    //< MARKING GLOBALS
    //> MARKING LOCALS
    Frame *frame = NULL;
    Value *frame_value = NULL;
    
    for (int frame_ptr = 0; frame_ptr < vm->frame_ptr; frame_ptr++){
        frame = &vm->frame_stack[frame_ptr];
        
        for (size_t local_ptr = 0; local_ptr < LOCALS_LENGTH; local_ptr++){
            frame_value = &frame->locals[local_ptr];
            mark_value(frame_value);
        }
    }
    //< MARKING LOCALS
    //> MARKING STACK
    Value *stack_value = NULL;
    
    for(int i = 0; i < vm->stack_ptr; i++){
		stack_value = &vm->stack[i];
		mark_value(stack_value);
	}
    //< MARKING STACK
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
        
        if(obj == vm->head){
            vm->head = next;
        }if(obj == vm->tail){
            vm->tail = prev;
        }if(prev){
            prev->next = next;
        }
            
        destroy_obj(obj, vm);

        obj = next;
    }    
}

void gc(VM *vm){
    mark_objs(vm);
    sweep_objs(vm);
}

int compare_locations(void *a, void *b){
	OPCodeLocation *al = (OPCodeLocation *)a;
	OPCodeLocation *bl = (OPCodeLocation *)b;

	if(al->offset < bl->offset){return -1;}
	else if(al->offset > bl->offset){return 1;}
	else{return 0;}
}

BStr *prepare_stacktrace(unsigned int spaces, VM *vm){
	BStr *st = bstr_create_empty(NULL);

	for(int i = vm->frame_ptr - 1; i >= 0; i--){
		Frame *frame = FRAME_AT(i, vm);
		Fn *fn = frame->fn;
		DynArr *locations = fn->locations;
		int index = FIND_LOCATION(frame->last_offset, locations);
		OPCodeLocation *location = index == -1 ? NULL : (OPCodeLocation *)DYNARR_GET(index, locations);

		if(location){
			bstr_append_args(
				st,
				"%*sin file: '%s' at %s:%d\n",
                spaces,
                "",
                location->filepath,
				frame->fn->name,
				location->line
			);
		}else{
			bstr_append_args(st, "inside function '%s'\n", fn->name);
        }

	}

	return st;
}
//< Private Implementation
void vm_utils_error(VM *vm, char *msg, ...){
	BStr *st = prepare_stacktrace(4, vm);

    va_list args;
	va_start(args, msg);

    fprintf(stderr, "Runtime error: ");
	vfprintf(stderr, msg, args);
    fprintf(stderr, "\n");
	if(st){fprintf(stderr, "%s", (char *)st->buff);}

	va_end(args);
	bstr_destroy(st);
    vm_utils_clean_up(vm);

    longjmp(vm->err_jmp, 1);
}

Allocator *vm_utils_allocator(){
    return &allocator;
}

uint32_t vm_utils_hash_obj(Obj *obj){
    switch (obj->type){
        case STR_OTYPE :{
            Str *str = obj->value.str;
            uint32_t hash = lzhtable_hash((uint8_t *)str->buff, str->len);
            return hash;
        }default:{
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
        }default:{
            Obj *obj = value->literal.obj;
            return vm_utils_hash_obj(obj);
        }
    }   
}

int vm_utils_obj_to_str(Obj *object, BStr *bstr){
    switch (object->type){
        case STR_OTYPE:{
            Str *str = object->value.str;
            return bstr_append(str->buff, bstr);
        }case ARRAY_OTYPE:{
            size_t buff_len = 1024;
            char buff[buff_len];
			Array *array = object->value.array;
            snprintf(buff, buff_len, "<array %d at %p>", array->len, array);
            return bstr_append(buff, bstr);
		}case LIST_OTYPE:{
            size_t buff_len = 1024;
            char buff[buff_len];
			DynArr *list = object->value.list;
            snprintf(buff, buff_len, "<list %ld at %p>", list->used, list);
            return bstr_append(buff, bstr);
		}case DICT_OTYPE:{
            size_t buff_len = 1024;
            char buff[buff_len];
            LZHTable *table = object->value.dict;
            snprintf(buff, buff_len, "<dict %ld at %p>", table->n, table);
			return bstr_append(buff, bstr);
        }case RECORD_OTYPE:{
            size_t buff_len = 1024;
            char buff[buff_len];
			Record *record = object->value.record;
            snprintf(buff, buff_len, "<record %ld at %p>", record->key_values ? record->key_values->n : 0, record);
			return bstr_append(buff, bstr);
		}case FN_OTYPE:{
            size_t buff_len = 1024;
            char buff[buff_len];
            Fn *fn = (Fn *)object->value.fn;
            snprintf(buff, buff_len, "<function '%s' - %d at %p>", fn->name, (uint8_t)(fn->params ? fn->params->used : 0), fn);
			return bstr_append(buff, bstr);
        }case NATIVE_FN_OTYPE:{
            size_t buff_len = 1024;
            char buff[buff_len];
            NativeFn *native_fn = object->value.native_fn;
            snprintf(buff, buff_len, "<native function '%s' - %d at %p>", native_fn->name, native_fn->arity, native_fn);
			return bstr_append(buff, bstr);
        }case NATIVE_MODULE_OTYPE:{
			size_t buff_len = 1024;
            char buff[buff_len];
            NativeModule *module = object->value.native_module;
            snprintf(buff, buff_len, "<native module '%s' at %p>", module->name, module);
			return bstr_append(buff, bstr);
		}case MODULE_OTYPE:{
            size_t buff_len = 1024;
            char buff[buff_len];
            Module *module = object->value.module;
            snprintf(buff, buff_len, "<module '%s' from '%s' at %p>", module->name, module->pathname, module);
			return bstr_append(buff, bstr);
        }case NATIVE_LIB_OTYPE:{
            size_t buff_len = 1024;
            char buff[buff_len];
            NativeLib *module = object->value.native_lib;
            snprintf(buff, buff_len, "<native library %p at %p>", module->handler, module);
			return bstr_append(buff, bstr);
        }case FOREIGN_FN_OTYPE:{
            size_t buff_len = 1024;
            char buff[buff_len];
            ForeignFn *foreign = object->value.foreign_fn;
            snprintf(buff, buff_len, "<foreign function at %p>", foreign);
			return bstr_append(buff, bstr);
        }default:{
            assert("Illegal object type");
        }
    }

    return 1;
}

int vm_utils_value_to_str(Value *value, BStr *bstr){
    switch (value->type){
        case EMPTY_VTYPE:{
            return bstr_append("empty", bstr);
        }case BOOL_VTYPE:{
            uint8_t bool = value->literal.bool;

            if(bool){
                return bstr_append("true", bstr);
            }else{
                return bstr_append("false", bstr);
            }
        }case INT_VTYPE:{
            size_t buff_len = 1024;
            char buff[buff_len];
            snprintf(buff, buff_len, "%ld", value->literal.i64);
            return bstr_append(buff, bstr);
        }case FLOAT_VTYPE:{
            size_t buff_len = 1024;
            char buff[buff_len];
            snprintf(buff, buff_len, "%.8f", value->literal.fvalue);
            return bstr_append(buff, bstr);
		}case OBJ_VTYPE:{
            return vm_utils_obj_to_str(value->literal.obj, bstr);
        }default:{
            assert("Illegal value type");
        }
    }

    return 1;
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

char *vm_utils_clone_buff(char *buff, VM *vm){
	size_t buff_len = strlen(buff);
	char *cloned_buff = (char *)lzalloc(buff_len + 1);
	
	if(!cloned_buff){return NULL;}

	memcpy(cloned_buff, buff, buff_len);
	cloned_buff[buff_len] = '\0';

	return cloned_buff;
}

Value *vm_utils_clone_value(Value *value, VM *vm){
    Value *clone = (Value *)lzalloc(sizeof(Value));
    if(!clone){return NULL;}
    *clone = *value;
    return clone;
}

Obj *vm_utils_obj(ObjType type, VM *vm){
    if(vm->objs_size >= 67108864){gc(vm);}

    Obj *obj = lzalloc(sizeof(Obj));
    if(!obj){return NULL;}

    memset(obj, 0, sizeof(Obj));
    obj->type = type;
    
    if(vm->tail){
        obj->prev = vm->tail;
        vm->tail->next = obj;
    }else{
		vm->head = obj;
	}

    vm->tail = obj;
    vm->objs_size += sizeof(Obj);

    return obj;
}

Obj *vm_utils_core_str_obj(char *buff, VM *vm){
    size_t buff_len = strlen(buff);
    Str *str = (Str *)lzalloc(sizeof(Str));
    Obj *str_obj = vm_utils_obj(STR_OTYPE, vm);

    if(!str || !str_obj){
        lzdealloc(str);
        return NULL;
    }

    str->core = 1;
    str->len = buff_len;
    str->buff = buff;

    str_obj->value.str = str;

    return str_obj;
}

Obj *vm_utils_uncore_str_obj(char *buff, VM *vm){
    size_t buff_len = strlen(buff);
    Str *str = (Str *)lzalloc(sizeof(Str));
    Obj *str_obj = vm_utils_obj(STR_OTYPE, vm);

    if(!str || !str_obj){
        lzdealloc(str);
        return NULL;
    }

    str->core = 0;
    str->len = buff_len;
    str->buff = buff;

    str_obj->value.str = str;

    return str_obj;
}

Obj *vm_utils_empty_str_obj(Value *out_value, VM *vm){
	char *buff = (char *)lzalloc(1);
	Str *str = str = (Str *)lzalloc(sizeof(Str));
	Obj *str_obj = vm_utils_obj(STR_OTYPE, vm);

	if(!buff || !str || !str_obj){
        lzdealloc(buff);
        lzdealloc(str);
        return NULL;
    }

    str->core = 0;
    str->len = 0;
    str->buff = buff;

    str_obj->value.str = str;
	
	return str_obj;
}

Obj *vm_utils_clone_str_obj(char *buff, Value *out_value, VM *vm){
    size_t buff_len = strlen(buff);
    char *new_buff = (char *)lzalloc(buff_len + 1);
	Str *str = (Str *)lzalloc(sizeof(Str));
	Obj *str_obj = vm_utils_obj(STR_OTYPE, vm);

    if(!new_buff || !str || !str_obj){
        lzdealloc(new_buff);
        lzdealloc(str);
        return NULL;
    }

    memcpy(new_buff, buff, buff_len);
    new_buff[buff_len] = '\0';

    str->core = 0;
    str->len = buff_len;
    str->buff = new_buff;

    str_obj->value.str = str;

    if(out_value){
        *out_value = OBJ_VALUE(str_obj);
    }

    return str_obj;
}

Obj *vm_utils_range_str_obj(size_t from, size_t to, char *buff, Value *out_value, VM *vm){
	size_t range_buff_len = to - from + 1;
	char *range_buff = (char *)lzalloc(range_buff_len + 1);
	Str *str = (Str *)lzalloc(sizeof(Str));
	Obj *str_obj = vm_utils_obj(STR_OTYPE, vm);

    if(!range_buff || !str || !str_obj){
        lzdealloc(range_buff);
        lzdealloc(str);
        return NULL;
    }

	memcpy(range_buff, buff + from, range_buff_len);
	range_buff[range_buff_len] = '\0';

    str->core = 0;
    str->len = range_buff_len;
    str->buff = range_buff;

	str_obj->value.str = str;

    if(out_value){
        *out_value = OBJ_VALUE(str_obj);
    }

    return str_obj;
}

Str *vm_utils_uncore_nalloc_str(char *buff, VM *vm){
    size_t buff_len = strlen(buff);
    Str *str = (Str *)lzalloc(sizeof(Str));

    if(!str){return NULL;}

    str->core = 0;
    str->len = buff_len;
    str->buff = buff;

    return str;
}

Str *vm_utils_uncore_alloc_str(char *buff, VM *vm){
    size_t buff_len = strlen(buff);
	char *new_buff = (char *)lzalloc(buff_len + 1);
    Str *str = (Str *)lzalloc(sizeof(Str));
        
    if(!new_buff || !str){
        lzdealloc(new_buff);
        lzdealloc(str);
        return NULL;
    }

    memcpy(new_buff, buff, buff_len);
    new_buff[buff_len] = '\0';

    str->core = 0;
    str->len = buff_len;
    str->buff = new_buff;

    return str;
}

Obj *vm_utils_array_obj(int32_t len, VM *vm){
    Value *values = (Value *)lzalloc(len * sizeof(Value));
    Array *array = (Array *)lzalloc(sizeof(Array));
    Obj *array_obj = vm_utils_obj(ARRAY_OTYPE, vm);
    
    if(!values || !array || !array_obj){
        lzdealloc(values);
        lzdealloc(array);
        return NULL;
    }

    array->len = len;
    array->values = values;
    array_obj->value.array = array;

    return array_obj;
}

Obj *vm_utils_list_obj(VM *vm){
    DynArr *list = dynarr_create(sizeof(Value), NULL);
    Obj *list_obj = vm_utils_obj(LIST_OTYPE, vm);

    if(!list || !list_obj){
        dynarr_destroy(list);
        return NULL;
    }

    list_obj->value.list = list;

    return list_obj;
}

Obj *vm_utils_dict_obj(VM *vm){
    LZHTable *dict = lzhtable_create(17, NULL);
    if(!dict){return NULL;}

    Obj *dict_obj = vm_utils_obj(DICT_OTYPE, vm);
    
    if(!dict_obj){
        lzhtable_destroy(NULL, dict);
        return NULL;
    }

    dict_obj->value.dict = dict;

    return dict_obj;
}

Obj *vm_utils_record_obj(char empty, VM *vm){
	Record *record = (Record *)lzalloc(sizeof(record));
	Obj *record_obj = vm_utils_obj(RECORD_OTYPE, vm);

	if(!record || !record_obj){
		lzdealloc(record);
		return NULL;
	}

	if(empty){
		record->key_values = NULL;
	}else{
		LZHTable *key_values = lzhtable_create(17, NULL);

		if(!key_values){
			lzdealloc(record);
			return NULL;
		}

		record->key_values= key_values;
	}

	record_obj->value.record = record;

	return record_obj;
}

Obj *vm_utils_native_fn_obj(
    int arity,
    char *name,
    void *target,
    RawNativeFn raw_native,
    VM *vm
){
    size_t name_len = strlen(name);
    assert(name_len < NAME_LEN - 1);

    NativeFn *native_fn = (NativeFn *)lzalloc(sizeof(NativeFn));
    Obj *native_fn_obj = vm_utils_obj(NATIVE_FN_OTYPE, vm);

    if(!native_fn || !native_fn_obj){
        lzdealloc(native_fn);
        return NULL;
    }

    native_fn->unique = 0;
    native_fn->arity = arity;
    memcpy(native_fn->name, name, name_len);
    native_fn->name[name_len] = '\0';
    native_fn->name_len = name_len;
    native_fn->target = target;
    native_fn->raw_fn = raw_native;

    native_fn_obj->value.native_fn = native_fn;

    return native_fn_obj;
}

Obj *vm_utils_native_lib_obj(void *handler, VM *vm){
    NativeLib *native_lib = (NativeLib *)lzalloc(sizeof(NativeLib));
    Obj *native_lib_obj = vm_utils_obj(NATIVE_LIB_OTYPE, vm);

    if(!native_lib || !native_lib_obj){
        lzdealloc(native_lib);
        return NULL;
    }

    native_lib->handler = handler;
    native_lib_obj->value.native_lib = native_lib;

    return native_lib_obj;
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

    NativeFn *native_fn = (NativeFn *)lzalloc(sizeof(NativeFn));
    if(!native_fn){return NULL;}

    native_fn->unique = 0;
    native_fn->arity = arity;
    memcpy(native_fn->name, name, name_len);
    native_fn->name[name_len] = '\0';
    native_fn->name_len = name_len;
    native_fn->target = target;
    native_fn->raw_fn = native;

    return native_fn;
}
