#include "vm_utils.h"
#include "memory.h"
#include "bstr.h"
#include "types.h"
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <dlfcn.h>

#define CURRENT_FRAME(vm)(&vm->frame_stack[vm->frame_ptr - 1])
#define CURRENT_FN(vm)(CURRENT_FRAME(vm)->fn)
#define CURRENT_LOCATIONS(vm)(CURRENT_FN(vm)->locations)
#define FIND_LOCATION(index, arr)(dynarr_find(&((OPCodeLocation){.offset = index, .line = -1}), compare_locations, arr))
#define FRAME_AT(at, vm)(&vm->frame_stack[at])

//> PRIVATE INTERFACE
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
			vmu_dealloc(global_node->value);
			global_node = next;
		}

		submodule->globals = NULL;
	}

    DynArr *symbols = submodule->symbols;
    
    for (size_t i = 0; i < DYNARR_LEN(symbols); i++){
        ModuleSymbol symbol = DYNARR_GET_AS(ModuleSymbol, i, symbols);

        if(symbol.type == MODULE_MSYMTYPE){
            clean_up_module(symbol.value.module);
        }
    }
}

void destroy_dict_values(void *key, void *value){
    vmu_dealloc(key);
    vmu_dealloc(value);
}

void destroy_globals_values(void *ptr){
	vmu_dealloc(ptr);
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
			if(!str->core){vmu_dealloc(str->buff);}
            vmu_dealloc(str);
			break;
		}case ARRAY_OTYPE:{
            Array *array = obj->value.array;
            vmu_dealloc(array->values);
            vmu_dealloc(array);
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
			lzhtable_destroy(destroy_dict_values, record->attributes);
			vmu_dealloc(record);
			break;
		}case CLOSURE_OTYPE:{
            Closure *closure = obj->value.closure;
            
            for (int i = 0; i < closure->values_len; i++){
                OutValue closure_value = closure->values[i];
                
                if(!closure_value.linked){
                    vmu_dealloc(closure_value.value);
                }
            }

            vmu_dealloc(closure->values);
            vmu_dealloc(closure);
            
            break;
        }case NATIVE_FN_OTYPE:{
            NativeFn *native_fn = obj->value.native_fn;
            if(!native_fn->unique){vmu_dealloc(native_fn);}
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
            vmu_dealloc(library);

            break;
        }case FOREIGN_FN_OTYPE:{
            vmu_dealloc(obj->value.foreign_fn);
            break;
        }default:{
			assert("Illegal object type");
		}
	}

	vmu_dealloc(obj);

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
			LZHTable *key_values = record->attributes;
			
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
		}case CLOSURE_OTYPE:{
            Closure *closure = obj->value.closure;
            
            for (int i = 0; i < closure->values_len; i++){
                Value *value = closure->values[i].value;
                mark_value(value);
            }
            
            break;
        }case NATIVE_FN_OTYPE:{
            break;
        }case MODULE_OTYPE:{
			Module *module = obj->value.module;
            LZHTable *globals = MODULE_GLOBALS(module);

			LZHTableNode *current = globals->head;
			LZHTableNode *next = NULL;

			GlobalValue *global_value = NULL;
            Value *value = NULL;

			while (current){
				next = current->next_table_node;
				global_value = (GlobalValue *)current->value;
                value = global_value->value;
				
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
    LZHTable *globals = MODULE_GLOBALS(module);
    
    LZHTableNode *current = globals->head;
    LZHTableNode *next = NULL;
    
    GlobalValue *global_value = NULL;
    Value *value = NULL;
    
    while (current){
        next = current->next_table_node;
        
        global_value = (GlobalValue *)current->value;
        value = global_value->value;

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
void vmu_error(VM *vm, char *msg, ...){
	BStr *st = prepare_stacktrace(4, vm);

    va_list args;
	va_start(args, msg);

    fprintf(stderr, "Runtime error: ");
	vfprintf(stderr, msg, args);
    fprintf(stderr, "\n");
	if(st){fprintf(stderr, "%s", (char *)st->buff);}

	va_end(args);
	bstr_destroy(st);
    vmu_clean_up(vm);

    longjmp(vm->err_jmp, 1);
}

//> PUBLIC IMPLEMENTATION
void *vmu_alloc(size_t size){
    return memory_alloc(size);
}

void *vmu_realloc(void *ptr, size_t new_size){
    return memory_realloc(ptr, new_size);
}

void vmu_dealloc(void *ptr){
    memory_dealloc(ptr);
}

uint32_t vmu_hash_obj(Obj *obj){
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

uint32_t vmu_hash_value(Value *value){
    switch (value->type){
        case EMPTY_VTYPE:
        case BOOL_VTYPE:
        case INT_VTYPE:{
            uint32_t hash = lzhtable_hash((uint8_t *)&value->literal.i64, sizeof(int64_t));
            return hash;
        }default:{
            Obj *obj = value->literal.obj;
            return vmu_hash_obj(obj);
        }
    }   
}

int vmu_obj_to_str(Obj *object, BStr *bstr){
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
            snprintf(buff, buff_len, "<record %ld at %p>", record->attributes ? record->attributes->n : 0, record);
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

int vmu_value_to_str(Value *value, BStr *bstr){
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
            return vmu_obj_to_str(value->literal.obj, bstr);
        }default:{
            assert("Illegal value type");
        }
    }

    return 1;
}

void vmu_clean_up(VM *vm){
	Obj *obj = vm->head;

	while(obj){
		Obj *next = obj->next;
		destroy_obj(obj, vm);
		obj = next;
	}

    clean_up_module(vm->module);
}

char *vmu_clone_buff(char *buff, VM *vm){
	size_t buff_len = strlen(buff);
	char *cloned_buff = (char *)vmu_alloc(buff_len + 1);
	
	if(!cloned_buff){return NULL;}

	memcpy(cloned_buff, buff, buff_len);
	cloned_buff[buff_len] = '\0';

	return cloned_buff;
}

char *vmu_join_buff(char *buffa, size_t sza, char *buffb, size_t szb, VM *vm){
    size_t szc = sza + szb;
    char *buff = vmu_alloc(szc + 1);

	  if(!buff){
        vmu_error(vm, "Failed to create buffer: out of memory");
    }

    memcpy(buff, buffa, sza);
    memcpy(buff + sza, buffb, szb);
    buff[szc] = '\0';

    return buff;
}

char *vmu_multiply_buff(char *buff, size_t szbuff, size_t by, VM *vm){
	size_t sz = szbuff * by;
	char *b = vmu_alloc(sz + 1);

	if(!b){
        vmu_error(vm, "Failed to create buffer: out of memory");
    }

	for(size_t i = 0; i < by; i++){
        memcpy(b + (i * szbuff), buff, szbuff);
    }

	b[sz] = '\0';

	return b;
}

Value *vmu_clone_value(Value *value, VM *vm){
    Value *clone = (Value *)vmu_alloc(sizeof(Value));
    if(!clone){return NULL;}
    *clone = *value;
    return clone;
}

Obj *vmu_obj(ObjType type, VM *vm){
    if(vm->objs_size >= 67108864){gc(vm);}

    Obj *obj = vmu_alloc(sizeof(Obj));
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

Obj *vmu_core_str_obj(char *buff, VM *vm){
    size_t buff_len = strlen(buff);
    Str *str = (Str *)vmu_alloc(sizeof(Str));
    Obj *str_obj = vmu_obj(STR_OTYPE, vm);

    if(!str || !str_obj){
        vmu_dealloc(str);
        return NULL;
    }

    str->core = 1;
    str->len = buff_len;
    str->buff = buff;

    str_obj->value.str = str;

    return str_obj;
}

Obj *vmu_uncore_str_obj(char *buff, VM *vm){
    size_t buff_len = strlen(buff);
    Str *str = (Str *)vmu_alloc(sizeof(Str));
    Obj *str_obj = vmu_obj(STR_OTYPE, vm);

    if(!str || !str_obj){
        vmu_dealloc(str);
        return NULL;
    }

    str->core = 0;
    str->len = buff_len;
    str->buff = buff;

    str_obj->value.str = str;

    return str_obj;
}

Obj *vmu_empty_str_obj(Value *out_value, VM *vm){
	char *buff = (char *)vmu_alloc(1);
	Str *str = str = (Str *)vmu_alloc(sizeof(Str));
	Obj *str_obj = vmu_obj(STR_OTYPE, vm);

	if(!buff || !str || !str_obj){
        vmu_dealloc(buff);
        vmu_dealloc(str);
        return NULL;
    }

    str->core = 0;
    str->len = 0;
    str->buff = buff;

    str_obj->value.str = str;
	
	return str_obj;
}

Obj *vmu_clone_str_obj(char *buff, Value *out_value, VM *vm){
    size_t buff_len = strlen(buff);
    char *new_buff = (char *)vmu_alloc(buff_len + 1);
	Str *str = (Str *)vmu_alloc(sizeof(Str));
	Obj *str_obj = vmu_obj(STR_OTYPE, vm);

    if(!new_buff || !str || !str_obj){
        vmu_dealloc(new_buff);
        vmu_dealloc(str);
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

Obj *vmu_range_str_obj(size_t from, size_t to, char *buff, Value *out_value, VM *vm){
	size_t range_buff_len = to - from + 1;
	char *range_buff = (char *)vmu_alloc(range_buff_len + 1);
	Str *str = (Str *)vmu_alloc(sizeof(Str));
	Obj *str_obj = vmu_obj(STR_OTYPE, vm);

    if(!range_buff || !str || !str_obj){
        vmu_dealloc(range_buff);
        vmu_dealloc(str);
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
    Str *str = (Str *)vmu_alloc(sizeof(Str));

    if(!str){return NULL;}

    str->core = 0;
    str->len = buff_len;
    str->buff = buff;

    return str;
}

Str *vm_utils_uncore_alloc_str(char *buff, VM *vm){
    size_t buff_len = strlen(buff);
	char *new_buff = (char *)vmu_alloc(buff_len + 1);
    Str *str = (Str *)vmu_alloc(sizeof(Str));
        
    if(!new_buff || !str){
        vmu_dealloc(new_buff);
        vmu_dealloc(str);
        return NULL;
    }

    memcpy(new_buff, buff, buff_len);
    new_buff[buff_len] = '\0';

    str->core = 0;
    str->len = buff_len;
    str->buff = new_buff;

    return str;
}

Obj *vmu_array_obj(int32_t len, VM *vm){
    Value *values = (Value *)vmu_alloc(len * sizeof(Value));
    Array *array = (Array *)vmu_alloc(sizeof(Array));
    Obj *array_obj = vmu_obj(ARRAY_OTYPE, vm);
    
    if(!values || !array || !array_obj){
        vmu_dealloc(values);
        vmu_dealloc(array);
        return NULL;
    }

    array->len = len;
    array->values = values;
    array_obj->value.array = array;

    return array_obj;
}

Obj *vmu_list_obj(VM *vm){
    DynArr *list = dynarr_create(sizeof(Value), (DynArrAllocator *)memory_allocator());
    Obj *list_obj = vmu_obj(LIST_OTYPE, vm);

    if(!list || !list_obj){
        dynarr_destroy(list);
        return NULL;
    }

    list_obj->value.list = list;

    return list_obj;
}

Obj *vmu_dict_obj(VM *vm){
    LZHTable *dict = lzhtable_create(16, (LZHTableAllocator *)memory_allocator());
    if(!dict){return NULL;}

    Obj *dict_obj = vmu_obj(DICT_OTYPE, vm);
    
    if(!dict_obj){
        lzhtable_destroy(NULL, dict);
        return NULL;
    }

    dict_obj->value.dict = dict;

    return dict_obj;
}

Obj *vmu_record_obj(uint8_t length, VM *vm){
	Record *record = (Record *)vmu_alloc(sizeof(Record));
	Obj *record_obj = vmu_obj(RECORD_OTYPE, vm);

	if(!record || !record_obj){
		vmu_dealloc(record);
		return NULL;
	}

	if(length == 0){
		record->attributes = NULL;
	}else{
		LZHTable *key_values = lzhtable_create(
            (size_t)length,
            (LZHTableAllocator *)memory_allocator()
        );

		if(!key_values){
			vmu_dealloc(record);
			return NULL;
		}

		record->attributes= key_values;
	}

	record_obj->value.record = record;

	return record_obj;
}

Obj *vmu_native_fn_obj(
    int arity,
    char *name,
    void *target,
    RawNativeFn raw_native,
    VM *vm
){
    size_t name_len = strlen(name);
    assert(name_len < NAME_LEN - 1);

    NativeFn *native_fn = (NativeFn *)vmu_alloc(sizeof(NativeFn));
    Obj *native_fn_obj = vmu_obj(NATIVE_FN_OTYPE, vm);

    if(!native_fn || !native_fn_obj){
        vmu_dealloc(native_fn);
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

Obj *vmu_native_lib_obj(void *handler, VM *vm){
    NativeLib *native_lib = (NativeLib *)vmu_alloc(sizeof(NativeLib));
    Obj *native_lib_obj = vmu_obj(NATIVE_LIB_OTYPE, vm);

    if(!native_lib || !native_lib_obj){
        vmu_dealloc(native_lib);
        return NULL;
    }

    native_lib->handler = handler;
    native_lib_obj->value.native_lib = native_lib;

    return native_lib_obj;
}

NativeFn *vmu_native_function(
    int arity,
    char *name,
    void *target,
    RawNativeFn native,
    VM *vm
){
    size_t name_len = strlen(name);
    assert(name_len < NAME_LEN - 1);

    NativeFn *native_fn = (NativeFn *)vmu_alloc(sizeof(NativeFn));
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
//< PUBLIC IMPLEMENTATION