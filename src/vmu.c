#include "vmu.h"
#include "memory.h"
#include "factory.h"
#include "bstr.h"
#include "fn.h"
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <assert.h>

#define CURRENT_FN(vm)(CURRENT_FRAME(vm)->fn)
#define CURRENT_LOCATIONS(vm)(CURRENT_FN(vm)->locations)
#define FIND_LOCATION(index, arr)(dynarr_find(&((OPCodeLocation){.offset = index, .line = -1}), compare_locations, arr))
#define FRAME_AT(at, vm)(&vm->frame_stack[at])

// GARBAGE COLLECTOR
void clean_up_module(Module *module, VM *vm);
void destroy_obj(ObjHeader *obj, VM *vm);
void sweep_objs(VM *vm);
void mark_globals(LZHTable *globals, VM *vm);
void mark_obj(ObjHeader *obj, VM *vm);
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

void clean_up_dict(void *value, void *key, void *vm){
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

void destroy_obj(ObjHeader *obj, VM *vm){
    assert(!obj->marked && "Must not destroy market objects");

	switch(obj->type){
		case STR_OTYPE:{
            vmu_destroy_str_obj(OBJ_TO_STR(obj), vm);
			break;
		}case ARRAY_OTYPE:{
            vmu_destroy_array_obj(OBJ_TO_ARRAY(obj), vm);
            break;
        }case LIST_OTYPE:{
            vmu_destroy_list_obj(OBJ_TO_LIST(obj), vm);
			break;
		}case DICT_OTYPE:{
            vmu_destroy_dict_obj(OBJ_TO_DICT(obj), vm);
            break;
        }case RECORD_OTYPE:{
            vmu_destroy_record_obj(OBJ_TO_RECORD(obj), vm);
            break;
		}case NATIVE_FN_OTYPE:{
            vmu_destroy_native_fn_obj(OBJ_TO_NATIVE_FN(obj), vm);
            break;
        }case FN_OTYPE:{
            vmu_destroy_fn_obj(OBJ_TO_FN(obj), vm);
            break;
        }case CLOSURE_OTYPE:{
            vmu_destroy_closure_obj(OBJ_TO_CLOSURE(obj), vm);
            break;
        }case NATIVE_MODULE_OTYPE:{
            vmu_destroy_native_module_obj(OBJ_TO_NATIVE_MODULE(obj), vm);
            break;
        }case MODULE_OTYPE:{
            vmu_destroy_module_obj(OBJ_TO_MODULE(obj), vm);
            break;
        }default:{
			assert("Illegal object type");
		}
	}
}

void sweep_objs(VM *vm){
    ObjHeader *current = vm->red_head;
    ObjHeader *next = NULL;

    while (current){
        next = current->next;
        vmu_remove_obj(current, (ObjHeader **)&vm->red_head, (ObjHeader **)&vm->red_tail);
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

void mark_obj(ObjHeader *obj, VM *vm){
    if(obj->marked){
        return;
    }

    obj->marked = 1;

	switch(obj->type){
		case ARRAY_OTYPE:{
            ArrayObj *array = OBJ_TO_ARRAY(obj);
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
            ListObj *list_obj = OBJ_TO_LIST(obj);
			DynArr *list = list_obj->list;
			Value *value = NULL;

			for(lidx_t i = 0; i < (lidx_t)DYNARR_LEN(list); i++){
				value = (Value *)DYNARR_GET(i, list);

                if(IS_VALUE_OBJ(value) && !IS_VALUE_MARKED(VALUE_TO_OBJ(value))){
                    mark_obj(VALUE_TO_OBJ(value), vm);
                }
			}

			break;
		}case DICT_OTYPE:{
            DictObj *dict_obj = OBJ_TO_DICT(obj);
            LZHTable *dict = dict_obj->dict;
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
			RecordObj *record_obj = OBJ_TO_RECORD(obj);
			LZHTable *key_values = record_obj->attributes;

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
            NativeFnObj *native_fn_obj = OBJ_TO_NATIVE_FN(obj);
            NativeFn *native_fn = native_fn_obj->native_fn;

            if(IS_VALUE_OBJ(&native_fn->target) && !IS_VALUE_MARKED(VALUE_TO_OBJ(&native_fn->target))){
                mark_obj(VALUE_TO_OBJ(&native_fn->target), vm);
            }

            break;
        }case FN_OTYPE:{
            // No work to be done
            break;
        }case CLOSURE_OTYPE:{
            ClosureObj *closure_obj = OBJ_TO_CLOSURE(obj);
            Closure *closure = closure_obj->closure;
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
            ModuleObj *module_obj = OBJ_TO_MODULE(obj);
            Module *module = module_obj->module;
            mark_globals(MODULE_GLOBALS(module), vm);
            break;
        }default:{
			assert("Illegal object type");
		}
	}

    vmu_remove_obj(obj, (ObjHeader **)&vm->red_head, (ObjHeader **)&vm->red_tail);
    vmu_insert_obj(obj, (ObjHeader **)&vm->blue_head, (ObjHeader **)&vm->blue_tail);
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

    for(ObjHeader *current = vm->blue_head; current; current = current->next){
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

uint32_t vmu_hash_obj(ObjHeader *obj){
    switch (obj->type){
        case STR_OTYPE :{
            StrObj *str = OBJ_TO_STR(obj);
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
            ObjHeader *obj = VALUE_TO_OBJ(value);
            return vmu_hash_obj(obj);
        }
    }
}

int vmu_obj_to_str(ObjHeader *object, BStr *bstr){
    switch (object->type){
        case STR_OTYPE:{
            StrObj *str = OBJ_TO_STR(object);
            return bstr_append(str->buff, bstr);
        }case ARRAY_OTYPE:{
            size_t buff_len = 1024;
            char buff[buff_len];

            ArrayObj *array = OBJ_TO_ARRAY(object);

            snprintf(buff, buff_len, "<array %d at %p>", array->len, array);

            return bstr_append(buff, bstr);
		}case LIST_OTYPE:{
            size_t buff_len = 1024;
            char buff[buff_len];

            ListObj *list_obj = OBJ_TO_LIST(object);
            DynArr *list = list_obj->list;

            snprintf(buff, buff_len, "<list %zu at %p>", list->used, list);

            return bstr_append(buff, bstr);
		}case DICT_OTYPE:{
            size_t buff_len = 1024;
            char buff[buff_len];

            DictObj *dict_obj = OBJ_TO_DICT(object);
            LZHTable *dict = dict_obj->dict;

            snprintf(buff, buff_len, "<dict %zu at %p>", dict->n, dict);

            return bstr_append(buff, bstr);
        }case RECORD_OTYPE:{
            size_t buff_len = 1024;
            char buff[buff_len];

            RecordObj *record_obj = OBJ_TO_RECORD(object);

            snprintf(buff, buff_len, "<record %zu at %p>", record_obj->attributes ? record_obj->attributes->n : 0, record_obj);

            return bstr_append(buff, bstr);
		}case NATIVE_FN_OTYPE:{
            size_t buff_len = 1024;
            char buff[buff_len];

            NativeFnObj *native_fn_obj = OBJ_TO_NATIVE_FN(object);
            NativeFn *native_fn = native_fn_obj->native_fn;

            snprintf(buff, buff_len, "<native function '%s' - %d at %p>", native_fn->name, native_fn->arity, native_fn);

            return bstr_append(buff, bstr);
        }case FN_OTYPE:{
            size_t buff_len = 1024;
            char buff[buff_len];

            FnObj *fn_obj = OBJ_TO_FN(object);
            Fn *fn = fn_obj->fn;

            snprintf(buff, buff_len, "<function '%s' - %d at %p>", fn->name, (uint8_t)(fn->params ? DYNARR_LEN(fn->params) : 0), fn);

            return bstr_append(buff, bstr);
        }case CLOSURE_OTYPE:{
            size_t buff_len = 1024;
            char buff[buff_len];

            ClosureObj *closure_obj = OBJ_TO_CLOSURE(object);
            Closure *closure = closure_obj->closure;
            Fn *fn = closure->meta->fn;

            snprintf(buff, buff_len, "<closure '%s' - %d at %p>", fn->name, (uint8_t)(fn->params ? DYNARR_LEN(fn->params) : 0), fn);

            return bstr_append(buff, bstr);
        }case NATIVE_MODULE_OTYPE:{
			size_t buff_len = 1024;
            char buff[buff_len];

            NativeModuleObj *native_module_obj = OBJ_TO_NATIVE_MODULE(object);
            NativeModule *module = native_module_obj->native_module;

            snprintf(buff, buff_len, "<native module '%s' at %p>", module->name, module);

            return bstr_append(buff, bstr);
		}case MODULE_OTYPE:{
            size_t buff_len = 1024;
            char buff[buff_len];

            ModuleObj *module_obj = OBJ_TO_MODULE(object);
            Module *module = module_obj->module;

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

            snprintf(buff, buff_len, "%zu", VALUE_TO_INT(value));

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

void vmu_print_obj(FILE *stream, ObjHeader *object){
    switch (object->type){
        case STR_OTYPE:{
            StrObj *str = OBJ_TO_STR(object);
            fprintf(stream, "%s", str->buff);
            break;
        }case ARRAY_OTYPE:{
			ArrayObj *array = OBJ_TO_ARRAY(object);
            fprintf(stream, "<array %d at %p>", array->len, array);
			break;
		}case LIST_OTYPE:{
			ListObj *list_obj = OBJ_TO_LIST(object);
            DynArr *list = list_obj->list;
            fprintf(stream, "<list %zu at %p>", list->used, list);
			break;
		}case DICT_OTYPE:{
            DictObj *dict_obj = OBJ_TO_DICT(object);
            LZHTable *dict = dict_obj->dict;
            fprintf(stream, "<dict %zu at %p>", dict->n, dict);
            break;
        }case RECORD_OTYPE:{
			RecordObj *record_obj = OBJ_TO_RECORD(object);

            switch (record_obj->type) {
                case RANDOM_RTYPE:{
                    fprintf(stream, "<random context %p>", record_obj);
                    break;
                }case FILE_RTYPE:{
                    fprintf(stream, "<file %p>", record_obj);
                    break;
                }default:{
                    fprintf(stream, "<record %zu at %p>", record_obj->attributes ? record_obj->attributes->n : 0, record_obj);
                    break;
                }
            }

			break;
		}case NATIVE_FN_OTYPE:{
            NativeFnObj *native_fn_obj = OBJ_TO_NATIVE_FN(object);
            NativeFn *native_fn = native_fn_obj->native_fn;
            fprintf(stream, "<native function '%s' - %d at %p>", native_fn->name, native_fn->arity, native_fn);
            break;
        }case FN_OTYPE:{
            FnObj *fn_obj = OBJ_TO_FN(object);
            Fn *fn = fn_obj->fn;
            fprintf(stream, "<function '%s' - %d at %p>", fn->name, (uint8_t)(fn->params ? DYNARR_LEN(fn->params) : 0), fn);
            break;
        }case CLOSURE_OTYPE:{
            ClosureObj *closure_obj = OBJ_TO_CLOSURE(object);
            Closure *closure = closure_obj->closure;
            Fn *fn = closure->meta->fn;
            fprintf(stream, "<closure '%s' - %d at %p>", fn->name, (uint8_t)(fn->params ? DYNARR_LEN(fn->params) : 0), fn);
            break;
        }case NATIVE_MODULE_OTYPE:{
            NativeModuleObj *native_module_obj = OBJ_TO_NATIVE_MODULE(object);
            NativeModule *module = native_module_obj->native_module;
            fprintf(stream, "<native module '%s' at %p>", module->name, module);
            break;
        }case MODULE_OTYPE:{
            ModuleObj *module_obj = OBJ_TO_MODULE(object);
            Module *module = module_obj->module;
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
            fprintf(stream, "%" PRId64, VALUE_TO_INT(value));
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
	ObjHeader *obj = vm->red_head;

	while(obj){
		ObjHeader *next = obj->next;
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
            factory_destroy_raw_str(raw_str, vm->fake_allocator);
            *raw_str_ptr = NULL;
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
    if(!value){
        return;
    }

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

ObjHeader *vmu_raw_str_obj(char runtime, uint32_t hash, size_t len, char *raw_str, VM *vm){
    StrObj *str_obj = MEMORY_ALLOC(StrObj, 1, vm->fake_allocator);

    str_obj->header.type = STR_OTYPE;
    str_obj->hash = hash;
    str_obj->len = len;
    str_obj->runtime = runtime;
    str_obj->buff = raw_str;

    return &str_obj->header;
}

ObjHeader *vmu_create_str_obj(char **raw_str_ptr, VM *vm){
    char *out_raw_str = NULL;
    uint32_t hash = vmu_raw_str_to_table(raw_str_ptr, vm, &out_raw_str);
    StrObj *str_obj = MEMORY_ALLOC(StrObj, 1, vm->fake_allocator);

    str_obj->header.type = STR_OTYPE;
    str_obj->runtime = 1;
    str_obj->hash = hash;
    str_obj->len = strlen(out_raw_str);
    str_obj->buff = out_raw_str;

    return &str_obj->header;
}

ObjHeader *vmu_create_str_cpy_obj(char *raw_str, VM *vm){
    uint8_t *key = (uint8_t *)raw_str;
    size_t key_size = strlen(raw_str);
    uint32_t hash = lzhtable_hash(key, key_size);

    LZHTable *strings = MODULE_STRINGS(CURRENT_FN(vm)->module);
    LZHTableNode *string_node = NULL;

    char runtime = 0;
    char *out_str = NULL;

    if(lzhtable_hash_contains(hash, strings, &string_node)){
        out_str = (char *)string_node->value;
    }else{
        runtime = 1;
        out_str = factory_clone_raw_str(raw_str, vm->fake_allocator);
    }

    return vmu_raw_str_obj(runtime, hash, key_size, out_str, vm);
}

ObjHeader *vmu_create_unchecked_str_obj(char *raw_str, VM *vm){
    uint8_t *key = (uint8_t *)raw_str;
    size_t key_size = strlen(raw_str);
    uint32_t hash = lzhtable_hash(key, key_size);
    StrObj *str_obj = MEMORY_ALLOC(StrObj, 1, vm->fake_allocator);

    str_obj->header.type = STR_OTYPE;
    str_obj->runtime = 1;
    str_obj->hash = hash;
    str_obj->len = strlen(raw_str);
    str_obj->buff = raw_str;

    return &str_obj->header;
}

void vmu_destroy_str_obj(StrObj *str_obj, VM *vm){
    if(!str_obj){
        return;
    }

    MEMORY_DEALLOC(StrObj, 1, str_obj, vm->fake_allocator);
}

ObjHeader *vmu_create_array_obj(aidx_t len, VM *vm){
    Value *values = MEMORY_ALLOC(Value, (size_t)len, vm->fake_allocator);
    ArrayObj *array_obj = MEMORY_ALLOC(ArrayObj, 1, vm->fake_allocator);

    array_obj->header.type = ARRAY_OTYPE;
    array_obj->len = len;
    array_obj->values = values;

    return &array_obj->header;
}

void vmu_destroy_array_obj(ArrayObj *array_obj, VM *vm){
    if(!array_obj){
        return;
    }

    MEMORY_DEALLOC(Value, (size_t)array_obj->values, array_obj->values, vm->fake_allocator);
    MEMORY_DEALLOC(ArrayObj, 1, array_obj, vm->fake_allocator);
}

ObjHeader *vmu_create_list_obj(VM *vm){
    DynArr *values = FACTORY_DYNARR_TYPE(Value, vm->fake_allocator);
    ListObj *list_obj = MEMORY_ALLOC(ListObj, 1, vm->fake_allocator);

    list_obj->header.type = LIST_OTYPE;
    list_obj->list = values;

    return &list_obj->header;
}

void vmu_destroy_list_obj(ListObj *list_obj, VM *vm){
    if(!list_obj){
        return;
    }

    dynarr_destroy(list_obj->list);
    MEMORY_DEALLOC(ListObj, 1, list_obj, vm->fake_allocator);
}

ObjHeader *vmu_create_dict_obj(VM *vm){
    LZHTable *dict = FACTORY_LZHTABLE(vm->fake_allocator);
    DictObj *dict_obj = MEMORY_ALLOC(DictObj, 1, vm->fake_allocator);

    dict_obj->header.type = DICT_OTYPE;
    dict_obj->dict = dict;

    return &dict_obj->header;
}

void vmu_destroy_dict_obj(DictObj *dict_obj, VM *vm){
    if(!dict_obj){
        return;
    }

    lzhtable_destroy(vm, clean_up_dict, dict_obj->dict);
    MEMORY_DEALLOC(Value, 1, dict_obj, vm->fake_allocator);
}

ObjHeader *vmu_create_record_obj(uint8_t length, VM *vm){
    LZHTable *attributes = length == 0 ? NULL : FACTORY_LZHTABLE_LEN(length, vm->fake_allocator);
    RecordObj *record_obj = MEMORY_ALLOC(RecordObj, 1, vm->fake_allocator);

    record_obj->header.type = RECORD_OTYPE;
    record_obj->type = NONE_RTYPE;
    record_obj->attributes = attributes;

	return &record_obj->header;
}

RecordRandom *vmu_create_record_random(VM *vm){
    RecordRandom *random = MEMORY_ALLOC(RecordRandom, 1, vm->fake_allocator);
    return random;
}

RecordFile *vmu_create_record_file(char *raw_mode, char mode, char *pathname, VM *vm){
    char *cloned_pathname = factory_clone_raw_str(pathname, vm->fake_allocator);
    FILE *handler = fopen(pathname, raw_mode);
    RecordFile *record_file = MEMORY_ALLOC(RecordFile, 1, vm->fake_allocator);

    if(!handler){
        factory_destroy_raw_str(cloned_pathname, vm->fake_allocator);
        MEMORY_DEALLOC(RecordFile, 1, record_file, vm->fake_allocator);
        return NULL;
    }

    record_file->mode = mode;
    record_file->handler = handler;
    record_file->pathname = cloned_pathname;

    return record_file;
}

ObjHeader *vmu_create_record_random_obj(VM *vm){
    RecordObj *record = OBJ_TO_RECORD(vmu_create_record_obj(0, vm));
    RecordRandom *record_random = vmu_create_record_random(vm);

    record->type = RANDOM_RTYPE;
    record->content = record_random;

    return &record->header;
}

ObjHeader *vmu_create_record_file_obj(char *raw_mode, char mode, char *pathname, VM *vm){
    RecordObj *record = OBJ_TO_RECORD(vmu_create_record_obj(0, vm));
    RecordFile *record_file = vmu_create_record_file(raw_mode, mode, pathname, vm);

    if(!record_file){
        return NULL;
    }

    record->type = FILE_RTYPE;
    record->content = record_file;

    return &record->header;
}

void vmu_destroy_record_obj(RecordObj *record_obj, VM *vm){
    if(!record_obj){
        return;
    }

    if(record_obj->type == RANDOM_RTYPE){
        vmu_destroy_record_random(RECORD_RANDOM(record_obj), vm);
    }

    if(record_obj->type == FILE_RTYPE){
        vmu_destroy_record_file(RECORD_FILE(record_obj), vm);
    }

    lzhtable_destroy(vm, clean_up_record, record_obj->attributes);
}

void vmu_destroy_record_random(RecordRandom *record_random, VM *vm){
    if(!record_random){
        return;
    }

    MEMORY_DEALLOC(RecordRandom, 1, record_random, vm->fake_allocator);
}

void vmu_destroy_record_file(RecordFile *record_file, VM *vm){
    if(!record_file){
        return;
    }

    factory_destroy_raw_str(record_file->pathname, vm->fake_allocator);
    fclose(record_file->handler);
    MEMORY_DEALLOC(RecordFile, 1, record_file, vm->fake_allocator);
}

ObjHeader *vmu_create_raw_native_fn_obj(
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

    NativeFn *fn = factory_create_native_fn(0, name, arity, target, raw_native, vm->fake_allocator);
    NativeFnObj *native_fn = MEMORY_ALLOC(NativeFnObj, 1, vm->fake_allocator);

    native_fn->header.type = NATIVE_FN_OTYPE;
    native_fn->native_fn = fn;

    return &native_fn->header;
}

ObjHeader *vmu_create_native_fn_obj(NativeFn *native_fn, VM *vm){
    NativeFnObj *native_fn_obj = MEMORY_ALLOC(NativeFnObj, 1, vm->fake_allocator);

    native_fn_obj->header.type = NATIVE_FN_OTYPE;
    native_fn_obj->native_fn = native_fn;

    return &native_fn_obj->header;
}

void vmu_destroy_native_fn_obj(NativeFnObj *native_fn_obj, VM *vm){
    NativeFn *native_fn = native_fn_obj->native_fn;

    if(!native_fn->core){
        factory_destroy_native_fn(native_fn, vm->fake_allocator);
    }

    MEMORY_DEALLOC(NativeFnObj, 1, native_fn_obj, vm->fake_allocator);
}

ObjHeader *vmu_create_fn_obj(Fn *fn, VM *vm){
    FnObj *fn_obj = MEMORY_ALLOC(FnObj, 1, vm->fake_allocator);

    fn_obj->header.type = FN_OTYPE;
    fn_obj->fn = fn;

    return &fn_obj->header;
}

void vmu_destroy_fn_obj(FnObj *fn_obj, VM *vm){
    if(!fn_obj){
        return;
    }

    MEMORY_DEALLOC(FnObj, 1, fn_obj, vm->fake_allocator);
}

ObjHeader *vmu_closure_obj(MetaClosure *meta, VM *vm){
    OutValue *values = factory_out_values(meta->values_len, vm->fake_allocator);
    Closure *closure = factory_closure(values, meta, vm->fake_allocator);
    ClosureObj *closure_obj = MEMORY_ALLOC(ClosureObj, 1, vm->fake_allocator);

    for (int i = 0; i < meta->values_len; i++){
        closure->out_values[i].closure = closure_obj;
    }

    closure_obj->header.type = CLOSURE_OTYPE;
    closure_obj->closure = closure;

    return &closure_obj->header;
}

void vmu_destroy_closure_obj(ClosureObj *closure_obj, VM *vm){
    Closure *closure = closure_obj->closure;
    MetaClosure *meta_closure = closure->meta;

    for (int i = 0; i < meta_closure->values_len; i++){
        OutValue *out_value = &closure->out_values[i];

        if(!out_value->linked){
            vmu_destroy_value(out_value->value, vm);
        }
    }

    factory_destroy_out_values(meta_closure->values_len, closure->out_values, vm->fake_allocator);
    factory_destroy_closure(closure, vm->fake_allocator);

    MEMORY_DEALLOC(ClosureObj, 1, closure_obj, vm->fake_allocator);
}

ObjHeader *vmu_create_native_module_obj(NativeModule *native_module, VM *vm){
    NativeModuleObj *native_module_obj = MEMORY_ALLOC(NativeModuleObj, 1, vm->fake_allocator);

    native_module_obj->header.type = NATIVE_MODULE_OTYPE;
    native_module_obj->native_module = native_module;

    return &native_module_obj->header;
}

void vmu_destroy_native_module_obj(NativeModuleObj *native_module_obj, VM *vm){
    if(!native_module_obj){
        return;
    }

    MEMORY_DEALLOC(NativeModuleObj, 1, native_module_obj, vm->fake_allocator);
}

ObjHeader *vmu_create_module_obj(Module *module, VM *vm){
    ModuleObj *module_obj = MEMORY_ALLOC(ModuleObj, 1, vm->fake_allocator);

    module_obj->header.type = MODULE_OTYPE;
    module_obj->module = module;

    return &module_obj->header;
}

void vmu_destroy_module_obj(ModuleObj *module_obj, VM *vm){
    if(!module_obj){
        return;
    }

    MEMORY_DEALLOC(ModuleObj, 1, module_obj, vm->fake_allocator);
}

void vmu_insert_obj(ObjHeader *obj, ObjHeader **raw_head, ObjHeader **raw_tail){
    ObjHeader *tail = *raw_tail;

    if(tail){
        tail->next = obj;
        obj->prev = tail;
    }else{
        *raw_head = obj;
    }

    *raw_tail = obj;
}

void vmu_remove_obj(ObjHeader *obj, ObjHeader **raw_head, ObjHeader **raw_tail){
    ObjHeader *head = *raw_head;
    ObjHeader *tail = *raw_tail;

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