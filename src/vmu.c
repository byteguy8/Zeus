#include "vmu.h"
#include "memory.h"
#include "factory.h"
#include "bstr.h"
#include "array.h"
#include "list.h"
#include "fn.h"
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <dlfcn.h>

#define CURRENT_FN(vm)(CURRENT_FRAME(vm)->fn)
#define CURRENT_LOCATIONS(vm)(CURRENT_FN(vm)->locations)
#define FIND_LOCATION(index, arr)(dynarr_find(&((OPCodeLocation){.offset = index, .line = -1}), compare_locations, arr))
#define FRAME_AT(at, vm)(&vm->frame_stack[at])

// GARBAGE COLLECTOR
void clean_up_module(Module *module, VM *vm);
void destroy_obj(Obj *obj, VM *vm);
void sweep_objs(VM *vm);
void mark_globals(LZHTable *globals, VM *vm);
void mark_obj(Obj *obj, VM *vm);
void mark_roots(VM *vm);
int compare_locations(void *a, void *b);
BStr *prepare_stacktrace(unsigned int spaces, VM *vm);
//< PRIVATE INTERFACE
//> PRIVATE IMPLEMENTATION
void clean_up_record(void *key, void *value, void *extra){
    VM *vm = (VM *)extra;
    factory_destroy_raw_str(key, vm->fake_allocator);
    vmu_destroy_value(value, vm);
}

void clean_up_table(void *value, void *key, void *vm){
    vmu_destroy_value(key, vm);
    vmu_destroy_value(value, vm);
}

void clean_up_module(Module *module, VM *vm){
    if(module->shadow){return;}

    SubModule *submodule = module->submodule;
	LZHTable *globals = submodule->globals;
	// Due to the same module can be imported in others modules
	// we need to make sure we already handled its globals in order
	// to not make a double free on them.
	if(globals){
		LZHTableNode *global_node = submodule->globals->head;
        LZHTableNode *next = NULL;
        GlobalValue *global_value = NULL;

		while(global_node){
			next = global_node->next_table_node;
            global_value = (GlobalValue *)(global_node->value);

            vmu_destroy_value(global_value->value, vm);
            vmu_destroy_global_value(global_value, vm);

            global_node = next;
		}

		submodule->globals = NULL;
	}

    DynArr *symbols = submodule->symbols;

    for (size_t i = 0; i < DYNARR_LEN(symbols); i++){
        SubModuleSymbol symbol = DYNARR_GET_AS(SubModuleSymbol, i, symbols);

        if(symbol.type == MODULE_MSYMTYPE){
            clean_up_module(symbol.value, vm);
        }
    }
}

void destroy_obj(Obj *obj, VM *vm){
    assert(!obj->marked && "Must not destroy market objects");

	switch(obj->type){
		case STR_OTYPE:{
            factory_destroy_str(OBJ_TO_STR(obj), vm->fake_allocator);
			break;
		}case ARRAY_OTYPE:{
            factory_destroy_array(OBJ_TO_ARRAY(obj), vm->fake_allocator);
            break;
        }case LIST_OTYPE:{
			dynarr_destroy(OBJ_TO_LIST(obj));
			break;
		}case DICT_OTYPE:{
            lzhtable_destroy(vm, clean_up_table, OBJ_TO_DICT(obj));
            break;
        }case RECORD_OTYPE:{
            factory_destroy_record(vm, clean_up_record, OBJ_TO_RECORD(obj), vm->fake_allocator);
			break;
		}case NATIVE_FN_OTYPE:{
            NativeFn *native_fn = OBJ_TO_NATIVE_FN(obj);

            if(!native_fn->core){
                factory_destroy_native_fn(native_fn, vm->fake_allocator);
            }

            break;
        }case FN_OTYPE:{
            // No work to be done
            break;
        }case CLOSURE_OTYPE:{
            Closure *closure = OBJ_TO_CLOSURE(obj);
            MetaClosure *meta = closure->meta;
            OutValue *out_value = NULL;

            for (int i = 0; i < meta->values_len; i++){
                out_value = &closure->out_values[i];

                if(!out_value->linked){
                    vmu_destroy_value(out_value->value, vm);
                }
            }

            factory_destroy_out_values(meta->values_len, closure->out_values, vm->fake_allocator);
            factory_destroy_closure(closure, vm->fake_allocator);

            break;
        }case NATIVE_MODULE_OTYPE:{
            // No work to be done
            break;
        }case MODULE_OTYPE:{
            // No work to be done
            break;
        }default:{
			assert("Illegal object type");
		}
	}

    vmu_destroy_obj(obj, vm);
}

void sweep_objs(VM *vm){
    Obj *current = vm->red_head;
    Obj *next = NULL;

    while (current){
        next = current->next;
        vmu_remove_obj(current, (Obj **)&vm->red_head, (Obj **)&vm->red_tail);
        destroy_obj(current, vm);
        current = next;
    }
}

void mark_globals(LZHTable *globals, VM *vm){
    LZHTableNode *current = globals->head;
    LZHTableNode *next = NULL;

    GlobalValue *global_value = NULL;
    Value *value = NULL;

    while (current){
        next = current->next_table_node;
        global_value = (GlobalValue *)current->value;
        value = global_value->value;

        if(IS_VALUE_OBJ(value) && !IS_VALUE_MARKED(VALUE_TO_OBJ(value))){
            mark_obj(VALUE_TO_OBJ(value), vm);
        }

        current = next;
    }
}

void mark_obj(Obj *obj, VM *vm){
    if(obj->marked){
        return;
    }

    obj->marked = 1;

	switch(obj->type){
		case ARRAY_OTYPE:{
            Array *array = OBJ_TO_ARRAY(obj);
            Value *values = array->values;
            Value *value = NULL;

            for (aidx_t i = 0; i < array->len; i++){
                value = &values[i];

                if(IS_VALUE_OBJ(value) && !IS_VALUE_MARKED(VALUE_TO_OBJ(value))){
                    mark_obj(VALUE_TO_OBJ(value), vm);
                }
            }

            break;
        }case LIST_OTYPE:{
			DynArr *list = OBJ_TO_LIST(obj);
			Value *value = NULL;

			for(lidx_t i = 0; i < (lidx_t)DYNARR_LEN(list); i++){
				value = (Value *)DYNARR_GET(i, list);

                if(IS_VALUE_OBJ(value) && !IS_VALUE_MARKED(VALUE_TO_OBJ(value))){
                    mark_obj(VALUE_TO_OBJ(value), vm);
                }
			}

			break;
		}case DICT_OTYPE:{
            LZHTable *dict = OBJ_TO_DICT(obj);
            LZHTableNode *current = dict->head;
			LZHTableNode *next = NULL;
			Value *key_value = NULL;
            Value *value = NULL;

			while (current){
				next = current->next_table_node;
                key_value = (Value *)current->key;
				value = (Value *)current->value;

                if(IS_VALUE_OBJ(key_value) && !IS_VALUE_MARKED(VALUE_TO_OBJ(key_value))){
                    mark_obj(VALUE_TO_OBJ(key_value), vm);
                }
                if(IS_VALUE_OBJ(value) && !IS_VALUE_MARKED(VALUE_TO_OBJ(value))){
                    mark_obj(VALUE_TO_OBJ(value), vm);
                }

				current = next;
			}

            break;
        }case RECORD_OTYPE:{
			Record *record = OBJ_TO_RECORD(obj);
			LZHTable *key_values = record->attributes;

			if(!key_values){
                break;
            }

			LZHTableNode *current = key_values->head;
			LZHTableNode *next = NULL;
			Value *value = NULL;

			while (current){
				next = current->next_table_node;
				value = (Value *)current->value;

				if(IS_VALUE_OBJ(value) && !IS_VALUE_MARKED(VALUE_TO_OBJ(value))){
                    mark_obj(VALUE_TO_OBJ(value), vm);
                }

				current = next;
			}

			break;
		}case NATIVE_FN_OTYPE:{
            NativeFn *native_fn = OBJ_TO_NATIVE_FN(obj);

            if(IS_VALUE_OBJ(&native_fn->target) && !IS_VALUE_MARKED(VALUE_TO_OBJ(&native_fn->target))){
                mark_obj(VALUE_TO_OBJ(&native_fn->target), vm);
            }

            break;
        }case FN_OTYPE:{
            // No work to be done
            break;
        }case CLOSURE_OTYPE:{
            Closure *closure = OBJ_TO_CLOSURE(obj);
            MetaClosure *meta = closure->meta;

            for (int i = 0; i < meta->values_len; i++){
                Value *value = closure->out_values[i].value;

                if(IS_VALUE_OBJ(value) && !IS_VALUE_MARKED(VALUE_TO_OBJ(value))){
                    mark_obj(VALUE_TO_OBJ(value), vm);
                }
            }

            break;
        }case NATIVE_MODULE_OTYPE:{
            // No work to be done
            break;
        }case MODULE_OTYPE:{
            mark_globals(MODULE_GLOBALS(OBJ_TO_MODULE(obj)), vm);
            break;
        }default:{
			assert("Illegal object type");
		}
	}

    vmu_remove_obj(obj, (Obj **)&vm->red_head, (Obj **)&vm->red_tail);
    vmu_insert_obj(obj, (Obj **)&vm->blue_head, (Obj **)&vm->blue_tail);
}

void mark_roots(VM *vm){
    //> MARKING STACK
    for(Value *current = vm->stack; current < vm->stack_top; current++){
        if(IS_VALUE_OBJ(current) && !IS_VALUE_MARKED(VALUE_TO_OBJ(current))){
            mark_obj(VALUE_TO_OBJ(current), vm);
        }
    }
    //< MARKING STACK
    //> MARKING OUTS
    Frame *frame = NULL;

    for (int frame_ptr = 0; frame_ptr < vm->frame_ptr; frame_ptr++){
        frame = &vm->frame_stack[frame_ptr];

        for(OutValue *out_value = frame->outs_head; out_value; out_value = out_value->next){
            if(!IS_VALUE_MARKED(out_value->closure)){
                mark_obj(out_value->closure, vm);
            }
        }
    }
    //< MARKING OUTS
    //> MARKING GLOBALS
    mark_globals(MODULE_GLOBALS(vm->modules[0]), vm);
    //< MARKING GLOBALS
}

void vmu_gc(VM *vm){
    mark_roots(vm);
    sweep_objs(vm);

    for(Obj *current = vm->blue_head; current; current = current->next){
        current->marked = 0;
    }

    vm->red_head = vm->blue_head;
    vm->red_tail = vm->blue_tail;

    vm->blue_head = NULL;
    vm->blue_tail = NULL;
}

int compare_locations(void *a, void *b){
	OPCodeLocation *al = (OPCodeLocation *)a;
	OPCodeLocation *bl = (OPCodeLocation *)b;

	if(al->offset < bl->offset){return -1;}
	else if(al->offset > bl->offset){return 1;}
	else{return 0;}
}

BStr *prepare_stacktrace(unsigned int spaces, VM *vm){
	BStr *st = FACTORY_BSTR(vm->fake_allocator);

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

    if(st && st->buff){
        fprintf(stderr, "%s", (char *)st->buff);
    }

	va_end(args);
	bstr_destroy(st);
    vmu_clean_up(vm);

    vm->exit_code = ERR_VMRESULT;

    longjmp(vm->err_jmp, 1);
}

uint32_t vmu_hash_obj(Obj *obj){
    switch (obj->type){
        case STR_OTYPE :{
            Str *str = OBJ_TO_STR(obj);
            return str->hash;
        }default:{
            uintptr_t iaddr = (uintptr_t)obj;
            uint32_t hash = lzhtable_hash((uint8_t *)&iaddr, sizeof(uintptr_t));
            return hash;
        }
    }
}

uint32_t vmu_hash_value(Value *value){
    switch (value->type){
        case BOOL_VTYPE:{
            uint32_t hash = lzhtable_hash((uint8_t *)&VALUE_TO_BOOL(value), sizeof(uint8_t));
            return hash;
        }case INT_VTYPE:{
            uint32_t hash = lzhtable_hash((uint8_t *)&VALUE_TO_INT(value), sizeof(int64_t));
            return hash;
        }case FLOAT_VTYPE:{
            uint32_t hash = lzhtable_hash((uint8_t *)&VALUE_TO_FLOAT(value), sizeof(double));
            return hash;
        }default:{
            Obj *obj = VALUE_TO_OBJ(value);
            return vmu_hash_obj(obj);
        }
    }
}

int vmu_obj_to_str(Obj *object, BStr *bstr){
    switch (object->type){
        case STR_OTYPE:{
            Str *str = OBJ_TO_STR(object);
            return bstr_append(str->buff, bstr);
        }case ARRAY_OTYPE:{
            size_t buff_len = 1024;
            char buff[buff_len];

            Array *array = OBJ_TO_ARRAY(object);

            snprintf(buff, buff_len, "<array %d at %p>", array->len, array);

            return bstr_append(buff, bstr);
		}case LIST_OTYPE:{
            size_t buff_len = 1024;
            char buff[buff_len];

            DynArr *list = OBJ_TO_LIST(object);

            snprintf(buff, buff_len, "<list %ld at %p>", list->used, list);

            return bstr_append(buff, bstr);
		}case DICT_OTYPE:{
            size_t buff_len = 1024;
            char buff[buff_len];

            LZHTable *table = OBJ_TO_DICT(object);

            snprintf(buff, buff_len, "<dict %ld at %p>", table->n, table);

            return bstr_append(buff, bstr);
        }case RECORD_OTYPE:{
            size_t buff_len = 1024;
            char buff[buff_len];

            Record *record = OBJ_TO_RECORD(object);

            snprintf(buff, buff_len, "<record %ld at %p>", record->attributes ? record->attributes->n : 0, record);

            return bstr_append(buff, bstr);
		}case NATIVE_FN_OTYPE:{
            size_t buff_len = 1024;
            char buff[buff_len];

            NativeFn *native_fn = OBJ_TO_NATIVE_FN(object);

            snprintf(buff, buff_len, "<native function '%s' - %d at %p>", native_fn->name, native_fn->arity, native_fn);

            return bstr_append(buff, bstr);
        }case FN_OTYPE:{
            size_t buff_len = 1024;
            char buff[buff_len];

            Fn *fn = OBJ_TO_FN(object);

            snprintf(buff, buff_len, "<function '%s' - %d at %p>", fn->name, (uint8_t)(fn->params ? DYNARR_LEN(fn->params) : 0), fn);

            return bstr_append(buff, bstr);
        }case CLOSURE_OTYPE:{
            size_t buff_len = 1024;
            char buff[buff_len];

            Closure *closure = OBJ_TO_CLOSURE(object);
            Fn *fn = closure->meta->fn;

            snprintf(buff, buff_len, "<closure '%s' - %d at %p>", fn->name, (uint8_t)(fn->params ? DYNARR_LEN(fn->params) : 0), fn);

            return bstr_append(buff, bstr);
        }case NATIVE_MODULE_OTYPE:{
			size_t buff_len = 1024;
            char buff[buff_len];

            NativeModule *module = OBJ_TO_NATIVE_MODULE(object);

            snprintf(buff, buff_len, "<native module '%s' at %p>", module->name, module);

            return bstr_append(buff, bstr);
		}case MODULE_OTYPE:{
            size_t buff_len = 1024;
            char buff[buff_len];

            Module *module = OBJ_TO_MODULE(object);

            snprintf(buff, buff_len, "<module '%s' from '%s' at %p>", module->name, module->pathname, module);

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
            uint8_t bool = VALUE_TO_BOOL(value);

            if(bool){
                return bstr_append("true", bstr);
            }else{
                return bstr_append("false", bstr);
            }
        }case INT_VTYPE:{
            size_t buff_len = 1024;
            char buff[buff_len];

            snprintf(buff, buff_len, "%ld", VALUE_TO_INT(value));

            return bstr_append(buff, bstr);
        }case FLOAT_VTYPE:{
            size_t buff_len = 1024;
            char buff[buff_len];

            snprintf(buff, buff_len, "%.8f", VALUE_TO_FLOAT(value));

            return bstr_append(buff, bstr);
		}case OBJ_VTYPE:{
            return vmu_obj_to_str(VALUE_TO_OBJ(value), bstr);
        }default:{
            assert("Illegal value type");
        }
    }

    return 1;
}

void vmu_print_obj(FILE *stream, Obj *object){
    switch (object->type){
        case STR_OTYPE:{
            Str *str = OBJ_TO_STR(object);
            fprintf(stream, "%s", str->buff);
            break;
        }case ARRAY_OTYPE:{
			Array *array = OBJ_TO_ARRAY(object);
            fprintf(stream, "<array %d at %p>", array->len, array);
			break;
		}case LIST_OTYPE:{
			DynArr *list = OBJ_TO_LIST(object);
            fprintf(stream, "<list %ld at %p>", list->used, list);
			break;
		}case DICT_OTYPE:{
            LZHTable *table = OBJ_TO_DICT(object);
            fprintf(stream, "<dict %ld at %p>", table->n, table);
            break;
        }case RECORD_OTYPE:{
			Record *record = OBJ_TO_RECORD(object);
            fprintf(stream, "<record %ld at %p>", record->attributes ? record->attributes->n : 0, record);
			break;
		}case NATIVE_FN_OTYPE:{
            NativeFn *native_fn = OBJ_TO_NATIVE_FN(object);
            fprintf(stream, "<native function '%s' - %d at %p>", native_fn->name, native_fn->arity, native_fn);
            break;
        }case FN_OTYPE:{
            Fn *fn = OBJ_TO_FN(object);
            fprintf(stream, "<function '%s' - %d at %p>", fn->name, (uint8_t)(fn->params ? DYNARR_LEN(fn->params) : 0), fn);
            break;
        }case CLOSURE_OTYPE:{
            Closure *closure = OBJ_TO_CLOSURE(object);
            Fn *fn = closure->meta->fn;
            fprintf(stream, "<closure '%s' - %d at %p>", fn->name, (uint8_t)(fn->params ? DYNARR_LEN(fn->params) : 0), fn);
            break;
        }case NATIVE_MODULE_OTYPE:{
            NativeModule *module = OBJ_TO_NATIVE_MODULE(object);
            fprintf(stream, "<native module '%s' at %p>", module->name, module);
            break;
        }case MODULE_OTYPE:{
            Module *module = OBJ_TO_MODULE(object);
            fprintf(stream, "<module '%s' '%s' at %p>", module->name, module->pathname, module);
            break;
        }default:{
            assert("Illegal object type");
        }
    }
}

void vmu_print_value(FILE *stream, Value *value){
    switch (value->type){
        case EMPTY_VTYPE:{
            fprintf(stream, "empty");
            break;
        }case BOOL_VTYPE:{
            uint8_t bool = VALUE_TO_BOOL(value);
            fprintf(stream, "%s", bool == 0 ? "false" : "true");
            break;
        }case INT_VTYPE:{
            fprintf(stream, "%ld", VALUE_TO_INT(value));
            break;
        }case FLOAT_VTYPE:{
			fprintf(stream, "%.8f", VALUE_TO_FLOAT(value));
            break;
		}case OBJ_VTYPE:{
            vmu_print_obj(stream, VALUE_TO_OBJ(value));
            break;
        }default:{
            assert("Illegal value type");
        }
    }
}

void vmu_clean_up(VM *vm){
	Obj *obj = vm->red_head;

	while(obj){
		Obj *next = obj->next;
		destroy_obj(obj, vm);
		obj = next;
	}

    clean_up_module(vm->modules[0], vm);
}

uint32_t vmu_raw_str_to_table(char **raw_str_ptr, VM *vm, char **out_raw_str){
    char *raw_str = *raw_str_ptr;
    uint8_t *key = (uint8_t *)raw_str;
    size_t key_size = strlen(raw_str);
    uint32_t hash = lzhtable_hash(key, key_size);
    LZHTable *strings = MODULE_STRINGS(CURRENT_FN(vm)->module);
    LZHTableNode *raw_str_node = NULL;

    if(lzhtable_hash_contains(hash, strings, &raw_str_node)){
        char *saved_raw_str = (char *)raw_str_node->value;

        if(raw_str != saved_raw_str){
            //factory_destroy_raw_str(raw_str, vm->rtallocator);
        }

        if(out_raw_str){
            *out_raw_str = saved_raw_str;
        }
    }else{
        lzhtable_hash_put(hash, raw_str, MODULE_STRINGS(CURRENT_MODULE(vm)));

        if(out_raw_str){
            *out_raw_str = raw_str;
        }
    }

    *raw_str_ptr = NULL;

    return hash;
}

Value *vmu_clone_value(Value *value, VM *vm){
    Value *cloned_value = MEMORY_ALLOC(Value, 1, vm->fake_allocator);
    if(!cloned_value){return NULL;}
    *cloned_value = *value;
    return cloned_value;
}

void vmu_destroy_value(Value *value, VM *vm){
    if(!value){return;}
    MEMORY_DEALLOC(Value, 1, value, vm->fake_allocator);
}

GlobalValue *vmu_global_value(VM *vm){
    GlobalValue *global_value = MEMORY_ALLOC(GlobalValue, 1, vm->fake_allocator);

    if(!global_value){return NULL;}

    global_value->access = PRIVATE_GVATYPE;
    global_value->value = NULL;

    return global_value;
}

void vmu_destroy_global_value(GlobalValue *global_value, VM *vm){
    if(!global_value){return;}
    MEMORY_DEALLOC(GlobalValue, 1, global_value, vm->fake_allocator);
}

Obj *vmu_create_obj(ObjType type, VM *vm){
    Obj *obj = MEMORY_ALLOC(Obj, 1, vm->fake_allocator);

    memset(obj, 0, sizeof(Obj));
    obj->type = type;

    return obj;
}

void vmu_destroy_obj(Obj *obj, VM *vm){
    if(!obj){
        return;
    }

    MEMORY_DEALLOC(Obj, 1, obj, vm->fake_allocator);
}

Obj *vmu_str_obj(char **raw_str_ptr, VM *vm){
    char *out_raw_str = NULL;
    uint32_t hash = vmu_raw_str_to_table(raw_str_ptr, vm, &out_raw_str);

    Str *str = factory_create_str(out_raw_str, vm->fake_allocator);
    Obj *str_obj = vmu_create_obj(STR_OTYPE, vm);

    str->hash = hash;
    str_obj->content = str;

    return str_obj;
}

Obj *vmu_unchecked_str_obj(char *raw_str, VM *vm){
    uint8_t *key = (uint8_t *)raw_str;
    size_t key_size = strlen(raw_str);
    uint32_t hash = lzhtable_hash(key, key_size);
    Str *str = factory_create_str(raw_str, vm->fake_allocator);
    Obj *str_obj = vmu_create_obj(STR_OTYPE, vm);

    str->hash = hash;
    str_obj->content = str;

    return str_obj;
}

Obj *vmu_array_obj(int32_t len, VM *vm){
    Array *array = factory_create_array(len, vm->fake_allocator);
    Obj *array_obj = vmu_create_obj(ARRAY_OTYPE, vm);

    array_obj->content = array;

    return array_obj;
}

Obj *vmu_list_obj(VM *vm){
    DynArr *list = FACTORY_DYNARR(sizeof(Value), vm->fake_allocator);
    Obj *list_obj = vmu_create_obj(LIST_OTYPE, vm);

    list_obj->content = list;

    return list_obj;
}

Obj *vmu_dict_obj(VM *vm){
    LZHTable *dict = FACTORY_LZHTABLE(vm->fake_allocator);
    Obj *dict_obj = vmu_create_obj(DICT_OTYPE, vm);

    dict_obj->content = dict;

    return dict_obj;
}

Obj *vmu_record_obj(uint8_t length, VM *vm){
	Record *record = factory_create_record(length, vm->fake_allocator);
	Obj *record_obj = vmu_create_obj(RECORD_OTYPE, vm);

	record_obj->content = record;

	return record_obj;
}

Obj *vmu_native_fn_obj(
    int arity,
    char *name,
    Value *target,
    RawNativeFn raw_native,
    VM *vm
){
    size_t name_len = strlen(name);

    if(name_len + 1 >= NAME_LEN){
        vmu_error(vm, "Native function name exceed the max length allowed");
    }

    NativeFn *native_fn = factory_create_native_fn(0, name, arity, target, raw_native, vm->fake_allocator);
    Obj *native_fn_obj = vmu_create_obj(NATIVE_FN_OTYPE, vm);

    native_fn_obj->content = native_fn;

    return native_fn_obj;
}

Obj *vmu_closure_obj(MetaClosure *meta, VM *vm){
    OutValue *values = factory_out_values(meta->values_len, vm->fake_allocator);
    Closure *closure = factory_closure(values, meta, vm->fake_allocator);
    Obj *closure_obj = vmu_create_obj(CLOSURE_OTYPE, vm);

    for (int i = 0; i < meta->values_len; i++){
        closure->out_values[i].closure = closure_obj;
    }

    closure_obj->content = closure;

    return closure_obj;
}

void vmu_insert_obj(Obj *obj, Obj **raw_head, Obj **raw_tail){
    Obj *tail = *raw_tail;

    if(tail){
        tail->next = obj;
        obj->prev = tail;
    }else{
        *raw_head = obj;
    }

    *raw_tail = obj;
}

void vmu_remove_obj(Obj *obj, Obj **raw_head, Obj **raw_tail){
    Obj *head = *raw_head;
    Obj *tail = *raw_tail;

    if(head == obj){
        *raw_head = obj->next;
    }
    if(tail == obj){
        *raw_tail = obj->prev;
    }
    if(obj->prev){
        obj->prev->next = obj->next;
    }
    if(obj->next){
        obj->next->prev = obj->prev;
    }

    obj->prev = NULL;
    obj->next = NULL;
}
//< PUBLIC IMPLEMENTATION