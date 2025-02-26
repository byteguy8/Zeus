#include "vm.h"
#include "vm_utils.h"
#include "memory.h"
#include "opcode.h"
#include "types.h"

#include "native_str.h"
#include "native_array.h"
#include "native_list.h"
#include "native_dict.h"
#include "native_lib.h"

#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <setjmp.h>
#include <dlfcn.h>

//> PRIVATE INTERFACE
#define CURRENT_FRAME(vm)(&(vm)->frame_stack[(vm)->frame_ptr - 1])
#define CURRENT_LOCALS(vm)(CURRENT_FRAME(vm)->locals)
#define CURRENT_FN(vm)(CURRENT_FRAME(vm)->fn)
#define CURRENT_CHUNKS(vm)(CURRENT_FN(vm)->chunks)
#define CURRENT_CONSTANTS(vm)(CURRENT_FN(vm)->constants)
#define CURRENT_FLOAT_VALUES(vm)(CURRENT_FN(vm)->float_values)

static Value *peek(VM *vm);

#define PUSH(value, vm) {                      \
    if(vm->stack_ptr >= STACK_LENGTH){         \
        vmu_error(vm, "Stack over flow"); \
    }                                          \
    vm->stack[vm->stack_ptr++] = value;        \
}

#define PUSH_EMPTY(vm) {             \
    Value empty_value = EMPTY_VALUE; \
    PUSH(empty_value, vm)            \
}

#define PUSH_BOOL(value, vm) {            \
    Value bool_value = BOOL_VALUE(value); \
    PUSH(bool_value, vm)                  \
}

#define PUSH_INT(value, vm) {           \
    Value int_value = INT_VALUE(value); \
    PUSH(int_value, vm)                 \
}

#define PUSH_FLOAT(value, vm) {             \
    Value float_value = FLOAT_VALUE(value); \
    PUSH(float_value, vm)                   \
}

#define PUSH_CORE_STR(buff, vm){                    \
    Obj *str_obj = vmu_core_str_obj(buff, vm); \
    Value obj_value = OBJ_VALUE(str_obj);           \
    PUSH(obj_value, vm);                            \
}

#define PUSH_UNCORE_STR(buff, vm){                    \
    Obj *str_obj = vmu_uncore_str_obj(buff, vm); \
    Value obj_value = OBJ_VALUE(str_obj);             \
    PUSH(obj_value, vm);                              \
}

#define PUSH_NON_NATIVE_FN(fn, vm) {               \
    Obj *obj = vmu_obj(FN_OTYPE, vm);         \
    if(!obj){vmu_error(vm, "Out of memory");} \
    obj->value.f##n = fn;                          \
    Value fn_value = OBJ_VALUE(obj);               \
    PUSH(fn_value, vm)                             \
}

#define PUSH_NATIVE_FN(fn, vm) {                   \
    Obj *obj = vmu_obj(NATIVE_FN_OTYPE, vm);  \
    if(!obj){vmu_error(vm, "Out of memory");} \
    obj->value.native_fn = fn;                     \
    Value fn_value = OBJ_VALUE(obj);               \
    PUSH(fn_value, vm)                             \
}

#define PUSH_NATIVE_MODULE(m, vm){                    \
	Obj *obj = vmu_obj(NATIVE_MODULE_OTYPE, vm); \
	if(!obj){vmu_error(vm, "out of memory");}    \
	obj->value.native_module = (m);                   \
	PUSH(OBJ_VALUE(obj), vm);                         \
}

#define PUSH_MODULE(m, vm) {                       \
    Obj *obj = vmu_obj(MODULE_OTYPE, vm);     \
	if(!obj){vmu_error(vm, "Out of memory");} \
	obj->value.module = m;                         \
	PUSH(OBJ_VALUE(obj), vm);                      \
}

#define PUSH_NATIVE_MODULE_SYMBOL(n, m, vm){												\
	size_t key_size = strlen((n));															\
	LZHTable *symbols = (m)->symbols;               										\
	LZHTableNode *symbol_node = NULL;                                                       \
	if(!lzhtable_contains((uint8_t *)(n), key_size, symbols, &symbol_node)){                \
		vmu_error(vm, "Module '%s' do not contains a symbol '%s'", (m)->name, (n));    \
    }                                                                                       \
	NativeModuleSymbol *symbol = (NativeModuleSymbol *)symbol_node->value;					\
	if(symbol->type == NATIVE_FUNCTION_NMSYMTYPE){											\
		PUSH_NATIVE_FN(symbol->value.fn, vm);												\
    }                                                                                       \
}

#define PUSH_MODULE_SYMBOL(n, m, vm) {                                                      \
    SubModule *submodule = (m)->submodule;                                                  \
    if(!submodule->resolve){                                                                \
        resolve_module((m), vm);                                                            \
    }                                                                                       \
   	size_t key_size = strlen((n));                                                          \
   	LZHTable *symbols = submodule->symbols;                                                 \
	LZHTableNode *node = NULL;                                                              \
	if(!lzhtable_contains((uint8_t *)(n), key_size, symbols, &node)){                       \
        vmu_error(vm, "Module '%s' do not contains a symbol '%s'", module->name, (n)); \
    }                                                                                       \
	ModuleSymbol *module_symbol = (ModuleSymbol *)node->value;                                     \
    if((m) != CURRENT_FRAME(vm)->fn->module && module_symbol->access == PRIVATE_MSYMATYPE){        \
        vmu_error(vm, "Symbol '%s' not public", (n));                                  \
    }                                                                                       \
	if(module_symbol->type == NATIVE_MODULE_MSYMTYPE){                                             \
        PUSH_NATIVE_MODULE(module_symbol->value.native_module, vm);							    \
    }                                                                                       \
	if(module_symbol->type == FUNCTION_MSYMTYPE){                                                  \
        push_fn(module_symbol->value.fn, vm);                                                      \
    }                                                                                       \
	if(module_symbol->type == MODULE_MSYMTYPE){                                                    \
        PUSH_MODULE(module_symbol->value.module, vm);                                              \
    }                                                                                       \
}

static Value *pop(VM *vm);

static void execute(uint8_t chunk, VM *vm);
static void resolve_module(Module *module, VM *vm);
//< Private Interface
Frame *frame_up(char *name, VM *vm){
    if(vm->frame_ptr >= FRAME_LENGTH){
        vmu_error(vm, "Frame over flow");
    }

    Module *module = vm->module;
    SubModule *submodule = module->submodule;
	LZHTable *symbols = submodule->symbols;
	LZHTableNode *symbol_node = NULL;
    
	if(!lzhtable_contains((uint8_t *)name, strlen(name), symbols, &symbol_node)){
        vmu_error(vm, "Symbol '%s' do not exists", name);
    }

	ModuleSymbol *symbol = (ModuleSymbol *)symbol_node->value;

	if(symbol->type != FUNCTION_MSYMTYPE){
        vmu_error(vm, "Expect symbol of type 'function', but got something else");
    }

    Fn *fn = symbol->value.fn;
    Frame *frame = &vm->frame_stack[vm->frame_ptr++];

    frame->ip = 0;
    frame->fn = fn;

    return frame;
}

Frame *frame_up_fn(Fn *fn, VM *vm){
    if(vm->frame_ptr >= FRAME_LENGTH){
        vmu_error(vm, "Frame over flow");
    }
        
    Frame *frame = &vm->frame_stack[vm->frame_ptr++];

    frame->ip = 0;
    frame->fn = fn;

    return frame;
}

void frame_down(VM *vm){
    if(vm->frame_ptr == 0){
        vmu_error(vm, "Frame under flow");
    }

    vm->frame_ptr--;
}

int16_t compose_i16(uint8_t *bytes){
    return ((int16_t)bytes[1] << 8) | ((int16_t)bytes[0]);
}

int32_t compose_i32(uint8_t *bytes){
    return ((int32_t)bytes[3] << 24) | ((int32_t)bytes[2] << 16) | ((int32_t)bytes[1] << 8) | ((int32_t)bytes[0]);
}

void print_obj(Obj *object){
    switch (object->type){
        case STR_OTYPE:{
            Str *str = object->value.str;
            printf("%s\n", str->buff);
            break;
        }case ARRAY_OTYPE:{
			Array *array = object->value.array;
			printf("<array %d at %p>\n", array->len, array);
			break;
		}case LIST_OTYPE:{
			DynArr *list = object->value.list;
			printf("<list %ld at %p>\n", list->used, list);
			break;
		}case DICT_OTYPE:{
            LZHTable *table = object->value.dict;
            printf("<dict %ld at %p>\n", table->n, table);
            break;
        }case RECORD_OTYPE:{
			Record *record = object->value.record;
			printf("<record %ld at %p>\n", record->attributes ? record->attributes->n : 0, record);
			break;
		}case FN_OTYPE:{
            Fn *fn = (Fn *)object->value.fn;
            printf("<function '%s' - %d at %p>\n", fn->name, (uint8_t)(fn->params ? fn->params->used : 0), fn);
            break;
        }case NATIVE_FN_OTYPE:{
            NativeFn *native_fn = object->value.native_fn;
            printf("<native function '%s' - %d at %p>\n", native_fn->name, native_fn->arity, native_fn);
            break;
        }case NATIVE_MODULE_OTYPE:{
            NativeModule *module = object->value.native_module;
            printf("<native module '%s' at %p>\n", module->name, module);
            break;
        }case MODULE_OTYPE:{
            Module *module = object->value.module;
            printf("<module '%s' from '%s' at %p>\n", module->name, module->pathname, module);
            break;
        }case NATIVE_LIB_OTYPE:{
            NativeLib *module = object->value.native_lib;
            printf("<native library %p at %p>\n", module->handler, module);
            break;
        }case FOREIGN_FN_OTYPE:{
            ForeignFn *foreign = object->value.foreign_fn;
            printf("<foreign function at %p>\n", foreign);
            break;
        }default:{
            assert("Illegal object type");
        }
    }
}

void print_value(Value *value){
    switch (value->type){
        case EMPTY_VTYPE:{
            printf("empty\n");
            break;
        }case BOOL_VTYPE:{
            uint8_t bool = value->literal.bool;
            printf("%s\n", bool == 0 ? "false" : "true");
            break;
        }case INT_VTYPE:{
            printf("%ld\n", value->literal.i64);
            break;
        }case FLOAT_VTYPE:{
			printf("%.8f\n", value->literal.fvalue);   
            break;
		}case OBJ_VTYPE:{
            print_obj(value->literal.obj);
            break;
        }default:{
            assert("Illegal value type");
        }
    }
}

void print_stack(VM *vm){
    for (int i = 0; i < vm->stack_ptr; i++){
        Value *value = &vm->stack[i];
        printf("%d --> ", i + 1);
        print_value(value);
    }
}

#define IS_AT_END(vm)(CURRENT_FRAME(vm)->ip >= CURRENT_CHUNKS(vm)->used)
#define ADVANCE(vm)(DYNARR_GET_AS(uint8_t, CURRENT_FRAME(vm)->ip++, CURRENT_CHUNKS(vm)))

int16_t read_i16(VM *vm){
    uint8_t bytes[2];

	for(size_t i = 0; i < 2; i++){
        bytes[i] = ADVANCE(vm);
    }

	return compose_i16(bytes);
}

int32_t read_i32(VM *vm){
	uint8_t bytes[4];

	for(size_t i = 0; i < 4; i++){
        bytes[i] = ADVANCE(vm);
    }

	return compose_i32(bytes);
}

int64_t read_i64_const(VM *vm){
    DynArr *constants = CURRENT_CONSTANTS(vm);
    int16_t index = read_i16(vm);
    return DYNARR_GET_AS(int64_t, index, constants);
}

double read_float_const(VM *vm){
	DynArr *float_values = CURRENT_FLOAT_VALUES(vm);
	int16_t index = read_i16(vm);
	return DYNARR_GET_AS(double, index, float_values);
}

char *read_str(VM *vm, uint32_t *out_hash){
    Module *module = CURRENT_FRAME(vm)->fn->module;
    SubModule *submodule = module->submodule;
    LZHTable *strings = submodule->strings;
    uint32_t hash = (uint32_t)read_i32(vm);
    if(out_hash){*out_hash = hash;}
    return lzhtable_hash_get(hash, strings);
}

Value *peek(VM *vm){
    if(vm->stack_ptr == 0){vmu_error(vm, "Stack is empty");}
    return &vm->stack[vm->stack_ptr - 1];
}

Value *peek_at(int offset, VM *vm){
    if(1 + offset > vm->stack_ptr){
        vmu_error(vm, "Illegal offset: %d, stack: %d", offset, vm->stack_ptr);
    }
    size_t at = vm->stack_ptr - 1 - offset;
    return &vm->stack[at];
}

void push_fn(Fn *fn, VM *vm){
	Obj *obj = vmu_obj(FN_OTYPE, vm);
	if(!obj){vmu_error(vm, "Out of memory");}
	obj->value.fn = fn;
	PUSH(OBJ_VALUE(obj), vm);
}

Value *pop(VM *vm){
    if(vm->stack_ptr == 0){vmu_error(vm, "Stack under flow");}
    return &vm->stack[--vm->stack_ptr];
}

void execute(uint8_t chunk, VM *vm){
    Frame *frame = CURRENT_FRAME(vm);
    frame->last_offset = frame->ip - 1;

    switch (chunk){
        case EMPTY_OPCODE:{
            PUSH_EMPTY(vm)
            break;
        }case FALSE_OPCODE:{
            PUSH_BOOL(0, vm);
            break;
        }case TRUE_OPCODE:{
            PUSH_BOOL(1, vm);
            break;
        }case CINT_OPCODE:{
            int64_t i64 = (int64_t)ADVANCE(vm);
            PUSH_INT(i64, vm);
            break;
        }case INT_OPCODE:{
            int64_t i64 = read_i64_const(vm);
            PUSH_INT(i64, vm);
            break;
        }case FLOAT_OPCODE:{
			double value = read_float_const(vm);
			PUSH_FLOAT(value, vm);
			break;
		}case STRING_OPCODE:{
            char *buff = read_str(vm, NULL);
            PUSH_CORE_STR(buff, vm)
            break;
		}case TEMPLATE_OPCODE:{
            int16_t len = read_i16(vm);
            BStr *bstr = bstr_create_empty(NULL);
            
            if(!bstr){
                vmu_error(vm, "Failed to create template: out of memory");
            }
            
            for (int16_t i = 0; i < len; i++){
                Value *value = pop(vm);
                
                if(vmu_value_to_str(value, bstr)){
                    bstr_destroy(bstr);
                    vmu_error(vm, "Failed to create template: out of memory");
                }
            }

            Obj *str_obj = vmu_clone_str_obj((char *)bstr->buff, NULL, vm);

            bstr_destroy(bstr);

            if(!str_obj){
                vmu_error(vm, "Failed to create template: out of memory");
            }

            PUSH(OBJ_VALUE(str_obj), vm);
            
            break;
        }case ARRAY_OPCODE:{
            uint8_t parameter = ADVANCE(vm);
            int32_t index = read_i32(vm);

            if(parameter == 1){
                Value *length_value = pop(vm);

                if(!IS_INT(length_value)){
                    vmu_error(vm, "Expect 'length' to be of type integer, but got something else");
                }
                
                int64_t length = TO_INT(length_value);

                if(length < 0 || length > INT32_MAX){
                    vmu_error(vm, "Illegal 'length' value. Must be 0 <= LENGTH(%ld) <= %d", length, INT32_MAX);
                }

                Obj *array_obj = vmu_array_obj((int32_t)length, vm);
                
                if(!array_obj){
                    vmu_error(vm, "Out of memory");
                }

                PUSH(OBJ_VALUE(array_obj), vm)
            }else if(parameter == 2){
                Value *value = pop(vm);
                Value *array_value = peek(vm);

                if(!IS_ARRAY(array_value)){
                    vmu_error(vm, "Expect an array, but got something else");
                }
                if(index < 0){
                    vmu_error(vm, "Illegal 'index' value. Must be: 0 <= INDEX(%d)", index);
                }

                Array *array = TO_ARRAY(array_value);

                if(index >= array->len){
                    vmu_error(vm, "Index out of bounds. Must be: 0 <= INDEX(%d) < %d", index, array->len);
                }

                array->values[index] = *value;
            }else{
                vmu_error(vm, "Illegal ARRAY opcode parameter: %d", parameter);
            }

            break;
        }case LIST_OPCODE:{
            int16_t len = read_i16(vm);
            
            Obj *list_obj = vmu_list_obj(vm);
            if(!list_obj) vmu_error(vm, "Out of memory");

			DynArr *list = list_obj->value.list;

            for(int32_t i = 0; i < len; i++){
				Value *value = pop(vm);

    			if(dynarr_insert(value, list)){
                    vmu_error(vm, "Out of memory");
                }
			}

            PUSH(OBJ_VALUE(list_obj), vm);

            break;
        }case DICT_OPCODE:{
            int32_t len = read_i16(vm);
            
            Obj *dict_obj = vmu_dict_obj(vm);
            if(!dict_obj){vmu_error(vm, "Out of memory");}

            LZHTable *dict = dict_obj->value.dict;

            for (int32_t i = 0; i < len; i++){
                Value *value = pop(vm);
                Value *key = pop(vm);
                
                Value *key_clone = vmu_clone_value(key, vm);
                Value *value_clone = vmu_clone_value(value, vm);

                if(!key_clone || !value_clone){
                    vmu_dealloc(key_clone);
                    vmu_dealloc(value_clone);
                    vmu_error(vm, "Out of memory");
                }

                uint32_t hash = vmu_hash_value(key_clone);
                
                if(lzhtable_hash_put_key(key_clone, hash, value_clone, dict)){
                    vmu_error(vm, "Out of memory");
                }
            }

            PUSH(OBJ_VALUE(dict_obj), vm);

            break;
        }case RECORD_OPCODE:{
            uint8_t len = ADVANCE(vm);    
		    Obj *record_obj = vmu_record_obj(len, vm);
		    
            if(!record_obj){
                vmu_error(vm, "Out of memory");
            }
		    if(len == 0){
                PUSH(OBJ_VALUE(record_obj), vm);
			    break;
		    }

			Record *record = record_obj->value.record;
            LZHTable *attributes = record->attributes;
	
			for(size_t i = 0; i < len; i++){
			    char *key = read_str(vm, NULL);
			    Value *value = pop(vm);
                size_t key_size = strlen(key);
                uint32_t hash = lzhtable_hash((uint8_t *)key, key_size);

                if(lzhtable_hash_contains(hash, attributes, NULL)){
                    vmu_error(vm, "Record already contains attribute '%s'", key);
                }

				char *cloned_key = vmu_clone_buff(key, vm);
			    Value *cloned_value = vmu_clone_value(value, vm);

				if(!cloned_key || !cloned_value){
                    vmu_dealloc(cloned_key);
                    vmu_dealloc(cloned_value);
					vmu_error(vm, "Out of memory");
				}

				if(lzhtable_hash_put_key(cloned_key, hash, cloned_value, record->attributes)){
					vmu_dealloc(cloned_key);
                    vmu_dealloc(cloned_value);
					vmu_error(vm, "Out of memory");
			    }
			}
	
            PUSH(OBJ_VALUE(record_obj), vm);

			break;
		}case ADD_OPCODE:{
            Value *vb = pop(vm);
            Value *va = pop(vm);

            if(IS_STR(va) || IS_STR(vb)){
                if(!IS_STR(va)){
                    vmu_error(vm, "Expect string at left side of string concatenation");
                }
                if(!IS_STR(vb)){
                    vmu_error(vm, "Expect string at right side of string concatenation");
                }

                Str *astr = TO_STR(va) ;
                Str *bstr = TO_STR(vb);

                char *buff = vmu_join_buff(astr->buff, astr->len, bstr->buff, bstr->len, vm);
                PUSH_UNCORE_STR(buff, vm);

                break;
            }

            if(IS_INT(va) || IS_INT(vb)){
                if(!IS_INT(va)){
                    vmu_error(vm, "Expect integer at left side of sum");
                }
                if(!IS_INT(vb)){
                    vmu_error(vm, "Expect integer at right side of sum");
                }

                int64_t left = TO_INT(va);
                int64_t right = TO_INT(vb);

                PUSH_INT(left + right, vm)

                break;
            }

            if(IS_FLOAT(va) || IS_FLOAT(vb)){
                if(!IS_FLOAT(va)){
                    vmu_error(vm, "Expect float at left side of sum");
                }
                if(!IS_FLOAT(vb)){
                    vmu_error(vm, "Expect float at right side of sum");
                }

                double left = TO_FLOAT(va);
                double right = TO_FLOAT(vb);

                PUSH_FLOAT(left + right, vm);

                break;
            }

            vmu_error(vm, "Unsuported types using + operator");
            
            break;
        }case SUB_OPCODE:{
            Value *vb = pop(vm);
            Value *va = pop(vm);

            if(IS_INT(va) || IS_INT(vb)){
                if(!IS_INT(va)){
                    vmu_error(vm, "Expect integer at left side subtraction");
                }
                if(!IS_INT(vb)){
                    vmu_error(vm, "Expect integer at right side subtraction");
                }

                int64_t left = TO_INT(va);
                int64_t right = TO_INT(vb);

                PUSH_INT(left - right, vm)

                break;
            }

            if(IS_FLOAT(va) || IS_FLOAT(vb)){
                if(!IS_FLOAT(va)){
                    vmu_error(vm, "Expect float at left side of subtraction");
                }
                if(!IS_FLOAT(vb)){
                    vmu_error(vm, "Expect float at right side of subtraction");
                }

                double left = TO_FLOAT(va);
                double right = TO_FLOAT(vb);

                PUSH_FLOAT(left - right, vm);

                break;
            }

            vmu_error(vm, "Unsuported types using - operator");

            break;
        }case MUL_OPCODE:{
            Value *vb = pop(vm);
            Value *va = pop(vm);

            if(IS_STR(va) || IS_STR(vb)){
                int64_t by = 0;
                char *in_buff = NULL;
                size_t in_buff_len = 0;

                if(IS_STR(va)){
                    Str *astr = TO_STR(va);
                    in_buff = astr->buff;
                    in_buff_len = astr->len;
                    
                    if(!IS_INT(vb))
                        vmu_error(vm, "Expect integer at right side of string multiplication");

                    by = TO_INT(vb);
                }

                if(IS_STR(vb)){
                    Str *bstr = TO_STR(vb);
                    in_buff = bstr->buff;
                    in_buff_len = bstr->len;

                    if(!IS_INT(va))
                        vmu_error(vm, "Expect integer at left side of string multiplication");

                    by = TO_INT(va);
                }

                char *buff = vmu_multiply_buff(in_buff, in_buff_len, (size_t)by, vm);
                PUSH_UNCORE_STR(buff, vm);

                break;
            }

            if(IS_INT(va) || IS_INT(vb)){
                if(!IS_INT(va)){
                    vmu_error(vm, "Expect integer at left side of multiplication");
                }
                if(!IS_INT(vb)){
                    vmu_error(vm, "Expect integer at right side of multiplication");
                }

                int64_t left = TO_INT(va);
                int64_t right = TO_INT(vb);
                
                PUSH_INT(left * right, vm);

                break;
            }
            
            if(IS_FLOAT(va) || IS_FLOAT(vb)){
                if(!IS_FLOAT(va)){
                    vmu_error(vm, "Expect float at left side of multiplication");
                }
                if(!IS_FLOAT(vb)){
                    vmu_error(vm, "Expect float at right side of multiplication");
                }

                double left = TO_FLOAT(va);
                double right = TO_FLOAT(vb);

                PUSH_FLOAT(left * right, vm);

                break;
            }

            vmu_error(vm, "Unsuported types using * operator");

            break;
        }case DIV_OPCODE:{
            Value *vb = pop(vm);
            Value *va = pop(vm);

            if(IS_INT(va) || IS_INT(vb)){
                if(!IS_INT(va)){
                    vmu_error(vm, "Expect integer at left side of division");
                }
                if(!IS_INT(vb)){
                    vmu_error(vm, "Expect integer at right side of division");
                }

                int64_t left = TO_INT(va);
                int64_t right = TO_INT(vb);

                PUSH_INT(left / right, vm)

                break;
            }

            if(IS_FLOAT(va) || IS_FLOAT(vb)){
                if(!IS_FLOAT(va)){
                    vmu_error(vm, "Expect float at left side of division");
                }
                if(!IS_FLOAT(vb)){
                    vmu_error(vm, "Expect float at right side of division");
                }

                double left = TO_FLOAT(va);
                double right = TO_FLOAT(vb);

                PUSH_FLOAT(left / right, vm)

                break;
            }

            vmu_error(vm, "Unsuported types using / operator");

            break;
        }case MOD_OPCODE:{
			Value *vb = pop(vm);
            Value *va = pop(vm);

            if(!IS_INT(va)){
                vmu_error(vm, "Expect integer at left side of module");
            }
            if(!IS_INT(vb)){
                vmu_error(vm, "Expect integer at right side of module");
            }

            int64_t left = TO_INT(va);
            int64_t right = TO_INT(vb);

            PUSH_INT(left % right, vm)

            break;
		}case LT_OPCODE:{
			Value *vb = pop(vm);
            Value *va = pop(vm);

            if(IS_INT(va) || IS_INT(vb)){
                if(!IS_INT(va)){
                    vmu_error(vm, "Expect integer at left side of comparison");
                }
                if(!IS_INT(vb)){
                    vmu_error(vm, "Expect integer at right side of comparison");
                }

                int64_t left = TO_INT(va);
                int64_t right = TO_INT(vb);

                PUSH_BOOL(left < right, vm)

                break;
            }

            if(IS_FLOAT(va) || IS_FLOAT(vb)){
                if(!IS_FLOAT(va)){
                    vmu_error(vm, "Expect float at left side of comparison");
                }
                if(!IS_FLOAT(vb)){
                    vmu_error(vm, "Expect float at right side of comparison");
                }

                double left = TO_FLOAT(va);
                double right = TO_FLOAT(vb);

                PUSH_BOOL(left < right, vm)

                break;
            }

            vmu_error(vm, "Unsuported types using < operator");

            break;
        }case GT_OPCODE:{
   			Value *vb = pop(vm);
            Value *va = pop(vm);

            if(IS_INT(va) || IS_INT(vb)){
                if(!IS_INT(va)){
                    vmu_error(vm, "Expect integer at left side of comparison");
                }
                if(!IS_INT(vb)){
                    vmu_error(vm, "Expect integer at right side of comparison");
                }

                int64_t left = TO_INT(va);
                int64_t right = TO_INT(vb);

                PUSH_BOOL(left > right, vm)

                break;
            }

            if(IS_FLOAT(va) || IS_FLOAT(vb)){
                if(!IS_FLOAT(va)){
                    vmu_error(vm, "Expect float at left side of comparison");
                }
                if(!IS_FLOAT(vb)){
                    vmu_error(vm, "Expect float at right side of comparison");
                }

                double left = TO_FLOAT(va);
                double right = TO_FLOAT(vb);

                PUSH_BOOL(left > right, vm)

                break;
            }

            vmu_error(vm, "Unsuported types using > operator");

            break;
        }case LE_OPCODE:{
            Value *vb = pop(vm);
            Value *va = pop(vm);

            if(IS_INT(va) || IS_INT(vb)){
                if(!IS_INT(va)){
                    vmu_error(vm, "Expect integer at left side of comparison");
                }          
                if(!IS_INT(vb)){
                    vmu_error(vm, "Expect integer at right side of comparison");
                }

                int64_t left = TO_INT(va);
                int64_t right = TO_INT(vb);

                PUSH_BOOL(left <= right, vm)

                break;
            }

            if(IS_FLOAT(va) || IS_FLOAT(vb)){
                if(!IS_FLOAT(va)){
                    vmu_error(vm, "Expect float at left side of comparison");
                }
                if(!IS_FLOAT(vb)){
                    vmu_error(vm, "Expect float at right side of comparison");
                }

                double left = TO_FLOAT(va);
                double right = TO_FLOAT(vb);

                PUSH_BOOL(left <= right, vm)

                break;
            }

            vmu_error(vm, "Unsuported types using <= operator");

            break;
        }case GE_OPCODE:{
            Value *vb = pop(vm);
            Value *va = pop(vm);

            if(IS_INT(va) || IS_INT(vb)){
                if(!IS_INT(va)){
                    vmu_error(vm, "Expect integer at left side of comparison");
                }
                if(!IS_INT(vb)){
                    vmu_error(vm, "Expect integer at right side of comparison");
                }

                int64_t left = TO_INT(va);
                int64_t right = TO_INT(vb);

                PUSH_BOOL(left >= right, vm)

                break;
            }

            if(IS_FLOAT(va) || IS_FLOAT(vb)){
                if(!IS_FLOAT(va)){
                    vmu_error(vm, "Expect float at left side of comparison");
                }
                if(!IS_FLOAT(vb)){
                    vmu_error(vm, "Expect float at right side of comparison");
                }

                double left = TO_FLOAT(va);
                double right = TO_FLOAT(vb);

                PUSH_BOOL(left >= right, vm)

                break;
            }

            vmu_error(vm, "Unsuported types using >= operator");

            break;
        }case EQ_OPCODE:{
            Value *vb = pop(vm);
            Value *va = pop(vm);

            if(IS_BOOL(va) || IS_BOOL(vb)){
                if(!IS_BOOL(va)){
                    vmu_error(vm, "Expect boolean at left side of equality");
                }
                if(!IS_BOOL(vb)){
                    vmu_error(vm, "Expect boolean at right side of equality");
                }

                uint8_t left = TO_BOOL(va);
                uint8_t right = TO_BOOL(vb);

                PUSH_BOOL(left == right, vm);

                break;
            }

            if(IS_INT(va) || IS_INT(vb)){
                if(!IS_INT(va)){
                    vmu_error(vm, "Expect integer at left side of equality");
                }
                if(!IS_INT(vb)){
                    vmu_error(vm, "Expect integer at right side of equality");
                }

                int64_t left = TO_INT(va);
                int64_t right = TO_INT(vb);

                PUSH_BOOL(left == right, vm)

                break;
            }

            if(IS_FLOAT(va) || IS_FLOAT(vb)){
                if(!IS_FLOAT(va)){
                    vmu_error(vm, "Expect float at left side of equality");
                }
                if(!IS_FLOAT(vb)){
                    vmu_error(vm, "Expect float at right side of equality");
                }

                double left = TO_FLOAT(va);
                double right = TO_FLOAT(vb);

                PUSH_BOOL(left == right, vm)

                break;
            }

            vmu_error(vm, "Unsuported types using == operator");

            break;
        }case NE_OPCODE:{
            Value *vb = pop(vm);
            Value *va = pop(vm);

            if(IS_BOOL(va) || IS_BOOL(vb)){
                if(!IS_BOOL(va)){
                    vmu_error(vm, "Expect boolean at left side of equality");
                }
                if(!IS_BOOL(vb)){
                    vmu_error(vm, "Expect boolean at right side of equality");
                }

                uint8_t left = TO_BOOL(va);
                uint8_t right = TO_BOOL(vb);

                PUSH_BOOL(left != right, vm);

                break;
            }

            if(IS_INT(va) || IS_INT(vb)){
                if(!IS_INT(va)){
                    vmu_error(vm, "Expect integer at left side of equality");
                }
                if(!IS_INT(vb)){
                    vmu_error(vm, "Expect integer at right side of equality");
                }

                int64_t left = TO_INT(va);
                int64_t right = TO_INT(vb);

                PUSH_BOOL(left != right, vm)

                break;
            }

            if(IS_FLOAT(va) || IS_FLOAT(vb)){
                if(!IS_FLOAT(va)){
                    vmu_error(vm, "Expect float at left side of equality");
                }
                if(!IS_FLOAT(vb)){
                    vmu_error(vm, "Expect float at right side of equality");
                }

                double left = TO_FLOAT(va);
                double right = TO_FLOAT(vb);

                PUSH_BOOL(left != right, vm)

                break;
            }

            vmu_error(vm, "Unsuported types using != operator");

            break;
        }case LSET_OPCODE:{
            Value *value = peek(vm);
            uint8_t index = ADVANCE(vm);
            CURRENT_FRAME(vm)->locals[index] = *value;
            break;
        }case LGET_OPCODE:{
            uint8_t index = ADVANCE(vm);
            Value value = CURRENT_FRAME(vm)->locals[index];
            PUSH(value, vm);
            break;
        }case GSET_OPCODE:{
            Value *value = peek(vm);
            uint32_t hash = (uint32_t)read_i32(vm);
            Module *module = CURRENT_FRAME(vm)->fn->module;
            SubModule *submodule = module->submodule;
            LZHTable *globals = submodule->globals;
            LZHTableNode *node = NULL;

            if(lzhtable_hash_contains(hash, globals, &node)){
                *(Value *)node->value = *value;
            }else{
                Value *new_value = vmu_clone_value(value, vm);
                lzhtable_hash_put(hash, new_value, globals);
            }

            break;
        }case GGET_OPCODE:{
            uint32_t hash = 0;
            char *symbol = read_str(vm, &hash);
            Module *module = CURRENT_FRAME(vm)->fn->module;
            SubModule *submodule = module->submodule;
            LZHTable *globals = submodule->globals;
            LZHTableNode *node = NULL;

            if(!lzhtable_hash_contains(hash, globals, &node)){
                vmu_error(vm, "Global symbol '%s' does not exists", symbol);
            }
            
            PUSH(*(Value *)node->value, vm);

            break;
        }case NGET_OPCODE:{
            char *key = read_str(vm, NULL);
            size_t key_size = strlen(key);
            LZHTableNode *node = NULL;

            if(lzhtable_contains((uint8_t *)key, key_size, vm->natives, &node)){
                NativeFn *native = (NativeFn *)node->value;
                PUSH_NATIVE_FN(native, vm);
                break;
            }

            vmu_error(vm, "Unknown native '%s'", (char *)key);

            break;
        }case SGET_OPCODE:{
			char *key = read_str(vm, NULL);
            Module *module = CURRENT_FRAME(vm)->fn->module;
            PUSH_MODULE_SYMBOL(key, module, vm);
			break;
		}case ASET_OPCODE:{
			Value *target_value =  pop(vm);
            Value *index_value =  pop(vm);
            Value *value =  peek(vm);

            if(!IS_ARRAY(target_value)){
                vmu_error(vm, "Expect array, but got something else");
            }

            Array *array = TO_ARRAY(target_value);
                
            if(!IS_INT(index_value)){
                vmu_error(vm, "Expect integer as index");
            }

            int64_t index = TO_INT(index_value);
            
            if(index < 0 || index > INT16_MAX){
                vmu_error(vm, "Illegal index range. Must be > 0 and < %s", INT16_MAX);
            }
            if((int16_t)index >= array->len){
                vmu_error(vm, "Index out of bounds. Array length: %d, index: %d", array->len, INT16_MAX);
            }

            array->values[index] = *value;

			break;
		}case PUT_OPCODE:{
			Value *target = pop(vm);
			Value *value = peek(vm);
			char *symbol = read_str(vm, NULL);
			Record *record = NULL;
	
			if(!IS_RECORD(target)){
                vmu_error(vm, "Expect a record, but got something else");
            }

			record = TO_RECORD(target);

			uint8_t *key = (uint8_t *)symbol;
			size_t key_size = strlen(symbol);
			uint32_t hash = lzhtable_hash(key, key_size);
			LZHTableNode *node = NULL;

			if(!lzhtable_hash_contains(hash, record->attributes, &node)){
                vmu_error(vm, "Record do not contains key '%s'", symbol);
            }

            *(Value *)node->value = *value;

			break;
		}case OR_OPCODE:{
            Value *vb = pop(vm);
            Value *va = pop(vm);

            if(!IS_BOOL(va)){
                vmu_error(vm, "Expect boolean at left side");
            }
            if(!IS_BOOL(vb)){
                vmu_error(vm, "Expect boolean at right side");
            }

            uint8_t left = TO_BOOL(va);
            uint8_t right = TO_BOOL(vb);

            PUSH_BOOL(left || right, vm)
            
            break;
        }case AND_OPCODE:{
            Value *vb = pop(vm);
            Value *va = pop(vm);

            if(!IS_BOOL(va)){
                vmu_error(vm, "Expect boolean at left side");
            }
            if(!IS_BOOL(vb)){
                vmu_error(vm, "Expect boolean at right side");
            }

            uint8_t left = TO_BOOL(va);
            uint8_t right = TO_BOOL(vb);

            PUSH_BOOL(left && right, vm)
            
            break;
        }case NNOT_OPCODE:{
            Value *vb = pop(vm);

			if(IS_INT(vb)){
				int64_t right = TO_INT(vb);
				PUSH_INT(-right, vm)
				break;
			}
			
			if(IS_FLOAT(vb)){
				double right = TO_FLOAT(vb);
				PUSH_FLOAT(-right, vm)
				break;
			}
            
            vmu_error(vm, "Expect integer or float at right side");
            
            break;
        }case NOT_OPCODE:{
            Value *vb = pop(vm);

            if(!IS_BOOL(vb)){
                vmu_error(vm, "Expect boolean at right side");
            }

            uint8_t right = TO_BOOL(vb);
            PUSH_BOOL(!right, vm)

            break;
        }case PRT_OPCODE:{
            Value *value = pop(vm);
            print_value(value);
            break;
        }case POP_OPCODE:{
            pop(vm);
            break;
        }case JMP_OPCODE:{
            int16_t jmp_value = read_i16(vm);
            if(jmp_value == 0){break;}
            
            if(jmp_value > 0){
                CURRENT_FRAME(vm)->ip += jmp_value - 1;
            }else{
                CURRENT_FRAME(vm)->ip += jmp_value - 3;
            }

            break;
        }case JIF_OPCODE:{
            Value *value = pop(vm);
            
            if(!IS_BOOL(value)){
                vmu_error(vm, "Expect boolean as conditional value");
            }

			uint8_t condition = TO_BOOL(value);
			int16_t jmp_value = read_i16(vm);
			
            if(jmp_value == 0){break;}

            if(!condition){
                if(jmp_value > 0){
                    CURRENT_FRAME(vm)->ip += jmp_value - 1;
                }else{
                    CURRENT_FRAME(vm)->ip += jmp_value - 3;
                }
            }

			break;
		}case JIT_OPCODE:{
            Value *value = pop(vm);

            if(!IS_BOOL(value)){
                vmu_error(vm, "Expect boolean as conditional value");
            }

			uint8_t condition = TO_BOOL(value);
			int16_t jmp_value = read_i16(vm);

			if(jmp_value == 0){break;}

			if(condition){
                if(jmp_value > 0){
                    CURRENT_FRAME(vm)->ip += jmp_value - 1;
                }else{
                    CURRENT_FRAME(vm)->ip += jmp_value - 3;
                }
            }

			break;
		}case CALL_OPCODE:{
            uint8_t args_count = ADVANCE(vm);
			Value *fn_value = peek_at(args_count, vm);
			
            if(IS_FN(fn_value)){
				Fn *fn = TO_FN(fn_value);
                uint8_t params_count = fn->params ? fn->params->used : 0;

                if(params_count != args_count){
                    vmu_error(vm, "Failed to call function '%s'. Declared with %d parameter(s), but got %d argument(s)", fn->name, params_count, args_count);
                }

                frame_up_fn(fn, vm);

                if(args_count > 0){
                    int from = (int)(args_count == 0 ? 0 : args_count - 1);

                    for (int i = from; i >= 0; i--){
                        CURRENT_FRAME(vm)->locals[i] = *pop(vm);
                    }
                }

                pop(vm);
            }else if(IS_NATIVE_FN(fn_value)){
				NativeFn *native_fn = TO_NATIVE_FN(fn_value);
                void *target = native_fn->target;
                RawNativeFn raw_fn = native_fn->raw_fn;

                if(native_fn->arity != args_count)
                    vmu_error(
                        vm,
                        "Failed to call function '%s'.\n\tDeclared with %d parameter(s), but got %d argument(s)",
                        native_fn->name,
                        native_fn->arity,
                        args_count
                    );

                Value values[args_count];
                
                for (int i = args_count; i > 0; i--){
                    values[i - 1] = *pop(vm);
                }
                
                pop(vm);

                Value out_value = raw_fn(args_count, values, target, vm);
                PUSH(out_value, vm);
            }else if(IS_FOREIGN_FN(fn_value)){
                ForeignFn *foreign_fn = TO_FOREIGN_FN(fn_value);
                RawForeignFn raw_fn = foreign_fn->raw_fn;
                Value values[args_count];
                
                for (int i = args_count; i > 0; i--){
                    values[i - 1] = *pop(vm);
                }
                
                pop(vm);

                Value out_value = *raw_fn(values);

                PUSH(out_value, vm);
            }else{
                vmu_error(vm, "Expect function after %d parameters count", args_count);
            }

            break;
        }case ACCESS_OPCODE:{
            char *symbol = read_str(vm, NULL);
            Value *value = pop(vm);

            if(IS_STR(value)){
				Str *str = TO_STR(value);
                Obj *native_fn_obj = native_str_get(symbol, str, vm);

                if(!native_fn_obj){
                    vmu_error(vm, "String does not have symbol '%s'", symbol);
                }

                PUSH(OBJ_VALUE(native_fn_obj), vm)

                break;
            }

            if(IS_ARRAY(value)){
                Array *array = TO_ARRAY(value);
                Obj *native_fn_obj = native_array_get(symbol, array, vm);

                if(!native_fn_obj){
                    vmu_error(vm, "Array does not have symbol '%s'", symbol);
                }

                PUSH(OBJ_VALUE(native_fn_obj), vm)

                break;
            }

            if(IS_LIST(value)){
				DynArr *list = TO_LIST(value);
                Obj *native_fn_obj = native_list_get(symbol, list, vm);
                
                if(!native_fn_obj){
                    vmu_error(vm, "List does not have symbol '%s'", symbol);
                }

                PUSH(OBJ_VALUE(native_fn_obj), vm)

                break;
            }

            if(IS_DICT(value)){
				LZHTable *dict = TO_DICT(value);
				Obj *native_fn_obj = native_dict_get(symbol, dict, vm);

                if(!native_fn_obj){
                    vmu_error(vm, "List does not have symbol '%s'", symbol);
                }

                PUSH(OBJ_VALUE(native_fn_obj), vm)

                break;
            }

			if(IS_RECORD(value)){
				Record *record = TO_RECORD(value);
                LZHTable *key_values = record->attributes;

                if(!key_values){
                    vmu_error(vm, "Record has no attributes");
                }

				uint8_t *key = (uint8_t *)symbol;
				size_t key_size = strlen(symbol);
                Value *value = (Value *)lzhtable_get(key, key_size, key_values);

                if(value){
                    PUSH(*value, vm);
                    break;
                }

				vmu_error(vm, "Record do not constains key '%s'", symbol);

				break;
			}

			if(IS_NATIVE_MODULE(value)){
				NativeModule *module = TO_NATIVE_MODULE(value);
				PUSH_NATIVE_MODULE_SYMBOL(symbol, module, vm);
				break;
			}

			if(IS_MODULE(value)){
				Module *module = TO_MODULE(value);
                PUSH_MODULE_SYMBOL(symbol, module, vm);
				break;
			}

            if(IS_NATIVE_LIBRARY(value)){
                NativeLib *library = TO_NATIVE_LIBRARY(value);
                void *handler = library->handler;
                
                void *(*znative_symbol)(char *symbol_name) = dlsym(handler, "znative_symbol");
                
                if(!znative_symbol){
                    vmu_error(vm, "Something is wrong with loaded native library. Expect 'znative_symbol' to be present");
                }

                RawForeignFn raw_foreign_fn = znative_symbol(symbol);
                
                if(!raw_foreign_fn){
                    vmu_error(vm, "Failed to get '%s' from native library: do not exists", symbol);
                }

                Obj *foreign_fn_obj = vmu_obj(FOREIGN_FN_OTYPE, vm);
                
                if(!foreign_fn_obj){
                    vmu_error(vm, "Failed to get native library symbol: out of memory");
                }

                ForeignFn *foreign_fn = (ForeignFn *)malloc(sizeof(ForeignFn));
                
                if(!foreign_fn){
                    vmu_error(vm, "Failed to get native library symbol: out of memory");
                }

                foreign_fn->raw_fn = raw_foreign_fn;
                foreign_fn_obj->value.foreign_fn = foreign_fn;
                
                PUSH(OBJ_VALUE(foreign_fn_obj), vm);

                break;
            }

            vmu_error(vm, "Illegal target to access");

            break;
        }case INDEX_OPCODE:{
            Value *target_value = pop(vm);
            Value *index_value = pop(vm);

            if(IS_ARRAY(target_value)){
                Array *array = TO_ARRAY(target_value);
                
                if(!IS_INT(index_value)){
                    vmu_error(vm, "Expect integer as index, but got something else");
                }

                int64_t index = TO_INT(index_value);
                
                if(index < 0 || index > INT32_MAX){
                    vmu_error(vm, "Illegal index value. Must be: 0 <= INDEX(%ld) <= %s", index, INT32_MAX);
                }
                if((int16_t)index >= array->len){
                    vmu_error(vm, "Index out of bounds. Must be: 0 <= INDEX(%ld) < %d", index, array->len);
                }

                Value value = array->values[(int32_t)index];

                PUSH(value, vm);

                break;
            }

            vmu_error(vm, "Illegal target");

            break;
        }case RET_OPCODE:{
            frame_down(vm);
            break;
        }case IS_OPCODE:{
			Value *value = pop(vm);
			uint8_t type = ADVANCE(vm);

			if(IS_OBJ(value)){
				Obj *obj = TO_OBJ(value);

				switch(obj->type){
					case STR_OTYPE:{
						PUSH(BOOL_VALUE(type == 4), vm);
						break;
					}case ARRAY_OTYPE:{
						PUSH(BOOL_VALUE(type == 5), vm);
						break;
					}case LIST_OTYPE:{
						PUSH(BOOL_VALUE(type == 6), vm);
						break;
					}case DICT_OTYPE:{
						PUSH(BOOL_VALUE(type == 7), vm);
						break;
					}case RECORD_OTYPE:{
						PUSH(BOOL_VALUE(type == 8), vm);
						break;
					}default:{
						vmu_error(vm, "Illegal object type");
                        break;
					}
				}
			}else{
				switch(value->type){
					case EMPTY_VTYPE:{
						PUSH(BOOL_VALUE(type == 0), vm);
						break;
					}case BOOL_VTYPE:{
						PUSH(BOOL_VALUE(type == 1), vm);
						break;
					}case INT_VTYPE:{
						PUSH(BOOL_VALUE(type == 2), vm);
						break;
					}case FLOAT_VTYPE:{
						PUSH(BOOL_VALUE(type == 3), vm);
						break;
					}default:{
						vmu_error(vm, "Illegal value type");
                        break;
					}
				}
			}

			break;
		}case THROW_OPCODE:{
			if(vm->frame_ptr == 0){
                vmu_error(vm, "Can not throw in a empty frame stack");
            }
            
            Value *value = pop(vm);
            Str *throw_message = NULL;

            if(IS_STR(value)){
                throw_message = TO_STR(value);
            }else if(IS_RECORD(value)){
                Record *record = TO_RECORD(value);

                if(record->attributes){
                    char *key = "message";
                    
                    uint8_t *k = (uint8_t *)key;
                    size_t ks = strlen(key);
                    
                    LZHTableNode *message_node = NULL;

                    if(lzhtable_contains(k, ks, record->attributes, &message_node)){
                        Value *value = (Value *)message_node->value;

                        if(!IS_STR(value)){
                            vmu_error(vm, "Expect record attribute 'message' to be of type string");
                        }

                        throw_message = TO_STR(value);   
                    }
                }
            }
            
            char throw = 1;
            int ptr = (int)vm->frame_ptr;

            while(ptr > 0){
                Frame *frame = &vm->frame_stack[--ptr];
                Fn *fn = frame->fn;
                Module *module = fn->module;
                SubModule *submodule = module->submodule;
                LZHTable *fn_tries = submodule->tries;
                LZHTableNode *node = NULL;

				uint8_t *key = (uint8_t *)fn;
				size_t key_size = sizeof(Fn);
                
                if(lzhtable_contains(key, key_size, fn_tries, &node)){
					TryBlock *try = NULL;
					DynArrPtr *tries = (DynArrPtr *)node->value;

					for(size_t i = 0; i < tries->used; i++){
						TryBlock *try_block = (TryBlock *)DYNARR_PTR_GET(i, tries);
                        size_t start = try_block->try;
                        size_t end = try_block->catch;
                        size_t ip = frame->ip;

						if(ip < start || ip > end){continue;}

						try = try_block;

                        break;
					}

					if(try){
						throw = 0;
						frame->ip = try->catch;
						vm->frame_ptr = ptr + 1;
                        frame->locals[try->local] = *value;

						break;
					}
                }
            }

            if(throw){
                if(throw_message){
                    vmu_error(vm, throw_message->buff);
                }else{
                    vmu_error(vm, "");
                }
            }
            
            break;
        }case LOAD_OPCODE:{
            char *path = read_str(vm, NULL);
            void *handler = dlopen(path, RTLD_LAZY);

            if(!handler){
                vmu_error(vm, "Failed to load native library: %s", dlerror());
            }

            native_lib_init(handler, vm);

            Obj *native_lib_obj = vmu_native_lib_obj(handler, vm);
            if(!native_lib_obj){vmu_error(vm, "Out of memory");}

            PUSH(OBJ_VALUE(native_lib_obj), vm);

            break;
        }default:{
            assert("Illegal opcode");
        }
    }
}

void resolve_module(Module *module, VM *vm){
    Module *old_module = vm->module;

    vm->module = module;
    frame_up("import", vm);

    module->submodule->resolve = 1;

    while (!IS_AT_END(vm)){
        uint8_t chunk = ADVANCE(vm);
        execute(chunk, vm);
    }

    frame_down(vm);

    vm->module = old_module;
}

VM *vm_create(){
    VM *vm = (VM *)A_RUNTIME_ALLOC(sizeof(VM));
    memset(vm, 0, sizeof(VM));
    return vm;
}

void vm_print_stack(VM *vm){
    print_stack(vm);
}

int vm_execute(
    LZHTable *natives,
    Module *module,
    VM *vm
){
    if(setjmp(vm->err_jmp) == 1) return 1;
    else{
        SubModule *submodule = module->submodule;
        submodule->resolve = 1;

        vm->stack_ptr = 0;
        vm->natives = natives;
        vm->module = module;

        frame_up("main", vm);

        while (!IS_AT_END(vm)){
            uint8_t chunk = ADVANCE(vm);
            execute(chunk, vm);
        }

        frame_down(vm);

        vmu_clean_up(vm);

        return 0;
    }
}
