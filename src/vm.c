#include "vm.h"
#include "vm_utils.h"

#include "native_str.h"
#include "native_list.h"
#include "native_dict.h"

#include "memory.h"
#include "opcode.h"
#include "types.h"
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <setjmp.h>

//> Private Interface
static Value *peek(VM *vm);

#define PUSH(value, vm) \
    if(vm->stack_ptr >= STACK_LENGTH) \
        vm_utils_error(vm, "Stack over flow"); \
    vm->stack[vm->stack_ptr++] = value; \

#define PUSH_EMPTY(vm) { \
    Value empty_value = EMPTY_VALUE; \
    PUSH(empty_value, vm) \
}

#define PUSH_BOOL(value, vm) { \
    Value bool_value = BOOL_VALUE(value); \
    PUSH(bool_value, vm) \
}

#define PUSH_INT(value, vm) { \
    Value int_value = INT_VALUE(value); \
    PUSH(int_value, vm) \
}

#define PUSH_CORE_STR(buff, vm){ \
    Obj *str_obj = vm_utils_core_str_obj(buff, vm); \
    Value obj_value = OBJ_VALUE(str_obj); \
    PUSH(obj_value, vm); \
}

#define PUSH_UNCORE_STR(buff, vm){ \
    Obj *str_obj = vm_utils_uncore_str_obj(buff, vm); \
    Value obj_value = OBJ_VALUE(str_obj); \
    PUSH(obj_value, vm); \
}

#define PUSH_NON_NATIVE_FN(fn, vm) {\
    Obj *obj = vm_utils_obj(FN_OTYPE, vm); \
    if(!obj) vm_utils_error(vm, "Out of memory"); \
    obj->value.f##n = fn; \
    Value fn_value = OBJ_VALUE(obj); \
    PUSH(fn_value, vm) \
}

#define PUSH_NATIVE_FN(fn, vm) {\
    Obj *obj = vm_utils_obj(NATIVE_FN_OTYPE, vm); \
    if(!obj) vm_utils_error(vm, "Out of memory"); \
    obj->value.native_fn = fn; \
    Value fn_value = OBJ_VALUE(obj); \
    PUSH(fn_value, vm) \
}

static void push(Value value, VM *vm);
static void push_module(Module *module, VM *vm);
static void push_module_symbol(char *name, Module *module, VM *vm);

static Value *pop(VM *vm);
static uint8_t pop_bool_assert(VM *vm, char *err_msg, ...);
static int64_t pop_i64_assert(VM *vm, char *err_msg, ...);

static void execute(uint8_t chunk, VM *vm);
static void resolve_module(Module *module, VM *vm);
//< Private Interface

char *join_buff(char *buffa, size_t sza, char *buffb, size_t szb, VM *vm){
    size_t szc = sza + szb;
    char *buff = malloc(szc + 1);

	if(!buff)
		vm_utils_error(vm, "Failed to create buffer: out of memory");

    memcpy(buff, buffa, sza);
    memcpy(buff + sza, buffb, szb);
    buff[szc] = '\0';

    return buff;
}

char *multiply_buff(char *buff, size_t szbuff, size_t by, VM *vm){
	size_t sz = szbuff * by;
	char *b = malloc(sz + 1);

	if(!b)
		vm_utils_error(vm, "failed to create buffer: out of memory.");

	for(size_t i = 0; i < by; i++)
		memcpy(b + (i * szbuff), buff, szbuff);

	b[sz] = '\0';

	return b;
}

#define CURRENT_FRAME(vm)(&vm->frame_stack[vm->frame_ptr - 1])
#define CURRENT_LOCALS(vm)(CURRENT_FRAME(vm)->locals)
#define CURRENT_FN(vm)(CURRENT_FRAME(vm)->fn)
#define CURRENT_CHUNKS(vm)(CURRENT_FN(vm)->chunks)

Frame *frame_up(char *name, VM *vm){
    if(vm->frame_ptr >= FRAME_LENGTH)
        vm_utils_error(vm, "FrameOverFlow");

    Module *module = vm->module;
	LZHTable *symbols = module->symbols;
	LZHTableNode *symbol_node = NULL;
    
	if(!lzhtable_contains((uint8_t *)name, strlen(name), symbols, &symbol_node))
        vm_utils_error(vm, "Symbol '%s' do not exists", name);

	ModuleSymbol *symbol = (ModuleSymbol *)symbol_node->value;

	if(symbol->type != FUNCTION_MSYMTYPE)
		vm_utils_error(vm, "Expect symbol of type 'function', but got something else");

    Fn *fn = symbol->value.fn;
    Frame *frame = &vm->frame_stack[vm->frame_ptr++];

    frame->ip = 0;
    frame->fn = fn;

    return frame;
}

Frame *frame_up_fn(Fn *fn, VM *vm){
    if(vm->frame_ptr >= FRAME_LENGTH)
        vm_utils_error(vm, "Frame over flow");
        
    Frame *frame = &vm->frame_stack[vm->frame_ptr++];

    frame->ip = 0;
    frame->fn = fn;

    return frame;
}

void frame_down(VM *vm){
    if(vm->frame_ptr == 0)
        vm_utils_error(vm, "Frame under flow");

    vm->frame_ptr--;
}

#define TO_STR(v)(v->literal.obj->value.str)

int32_t compose_i32(uint8_t *bytes){
    return ((int32_t)bytes[3] << 24) | ((int32_t)bytes[2] << 16) | ((int32_t)bytes[1] << 8) | ((int32_t)bytes[0]);
}

void print_obj(Obj *object){
    switch (object->type){
        case STR_OTYPE:{
            Str *str = object->value.str;
            printf("%s\n", str->buff);
            break;
        }
   		case LIST_OTYPE:{
			DynArr *list = object->value.list;
			printf("<list %ld at %p>\n", list->used, list);
			break;
		}
        case DICT_OTYPE:{
            LZHTable *table = object->value.dict;
            printf("<dict %ld at %p>\n", table->n, table);
            break;
        }
		case RECORD_OTYPE:{
			Record *record = object->value.record;
			printf("<record %ld at %p>\n", record->key_values ? record->key_values->n : 0, record);
			break;
		}
        case FN_OTYPE:{
            Fn *fn = (Fn *)object->value.fn;
            printf("<function '%s' - %d at %p>\n", fn->name, (uint8_t)(fn->params ? fn->params->used : 0), fn);
            break;
        }
        case NATIVE_FN_OTYPE:{
            NativeFn *native_fn = object->value.native_fn;
            printf("<native function '%s' - %d at %p>\n", native_fn->name, native_fn->arity, native_fn);
            break;
        }
        case MODULE_OTYPE:{
            Module *module = object->value.module;
            printf("<module '%s' at %p>\n", module->name, module);
            break;
        }
        default:{
            assert("Illegal object type");
        }
    }
}

void print_value(Value *value){
    switch (value->type){
        case EMPTY_VTYPE:{
            printf("empty\n");
            break;
        }
        case BOOL_VTYPE:{
            uint8_t bool = value->literal.bool;
            printf("%s\n", bool == 0 ? "false" : "true");
            break;
        }
        case INT_VTYPE:{
            printf("%ld\n", value->literal.i64);   
            break;
        }
        case OBJ_VTYPE:{
            print_obj(value->literal.obj);
            break;
        }
        default:{
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

int32_t read_i32(VM *vm){
	uint8_t bytes[4];

	for(size_t i = 0; i < 4; i++)
		bytes[i] = ADVANCE(vm);

	return compose_i32(bytes);
}

int64_t read_i64_const(VM *vm){
    Module *module = CURRENT_FRAME(vm)->fn->module;
    DynArr *constants = module->constants;
    int32_t index = read_i32(vm);
    return DYNARR_GET_AS(int64_t, index, constants);
}

char *read_str(VM *vm, uint32_t *out_hash){
    Module *module = CURRENT_FRAME(vm)->fn->module;
    LZHTable *strings = module->strings;
    uint32_t hash = (uint32_t)read_i32(vm);
    if(out_hash) *out_hash = hash;
    return lzhtable_hash_get(hash, strings);
}

Value *peek(VM *vm){
    if(vm->stack_ptr == 0) vm_utils_error(vm, "Stack is empty");
    return &vm->stack[vm->stack_ptr - 1];
}

void push(Value value, VM *vm){
    if(vm->stack_ptr >= STACK_LENGTH) vm_utils_error(vm, "Stack over flow");
    vm->stack[vm->stack_ptr++] = value;
}

Value *peek_at(int offset, VM *vm){
    if(1 + offset > vm->stack_ptr) vm_utils_error(vm, "Illegal offset");
    size_t at = vm->stack_ptr - 1 - offset;
    return &vm->stack[at];
}

void push_fn(Fn *fn, VM *vm){
	Obj *obj = vm_utils_obj(FN_OTYPE, vm);
	if(!obj) vm_utils_error(vm, "Out of memory");
	obj->value.fn = fn;
	push(OBJ_VALUE(obj), vm);
}

void push_module(Module *module, VM *vm){
	Obj *obj = vm_utils_obj(MODULE_OTYPE, vm);
	if(!obj) vm_utils_error(vm, "Out of memory");
	obj->value.module = module;
	push(OBJ_VALUE(obj), vm);
}

void push_module_symbol(char *name, Module *module, VM *vm){
    if(!module->to_resolve){
        resolve_module(module, vm);
        module->to_resolve = 1;
    }

   	size_t key_size = strlen(name);
   	LZHTable *symbols = module->symbols;
	LZHTableNode *node = NULL;

	if(!lzhtable_contains((uint8_t *)name, key_size, symbols, &node))
		vm_utils_error(vm, "Module '%s' do not contains a symbol '%s'", module->name, name);

	ModuleSymbol *symbol = (ModuleSymbol *)node->value;
			
	if(symbol->type == FUNCTION_MSYMTYPE)
		push_fn(symbol->value.fn, vm);
	if(symbol->type == MODULE_MSYMTYPE)
		push_module(symbol->value.module, vm);
}

Value *pop(VM *vm){
    if(vm->stack_ptr == 0) vm_utils_error(vm, "Stack under flow");
    return &vm->stack[--vm->stack_ptr];
}

uint8_t pop_bool_assert(VM *vm, char *err_msg, ...){
    Value *value = pop(vm);
    
    if(value->type == BOOL_VTYPE) return value->literal.bool;

    va_list args;
	va_start(args, err_msg);

	fprintf(stderr, "Runtime error:\n\t");
	vfprintf(stderr, err_msg, args);
    fprintf(stderr, "\n");

	va_end(args);

    vm_utils_clean_up(vm);
    longjmp(vm->err_jmp, 1);
}

int64_t pop_i64_assert(VM *vm, char *err_msg, ...){
    Value *value = pop(vm);
    
    if(value->type == INT_VTYPE) return value->literal.i64;

    va_list args;
	va_start(args, err_msg);

	fprintf(stderr, "Runtime error:\n\t");
	vfprintf(stderr, err_msg, args);
    fprintf(stderr, "\n");

	va_end(args);

    vm_utils_clean_up(vm);
    longjmp(vm->err_jmp, 1);
}

void execute(uint8_t chunk, VM *vm){
    Frame *frame = CURRENT_FRAME(vm);
    frame->last_offset = frame->ip - 1;

    switch (chunk){
        case EMPTY_OPCODE:{
            PUSH_EMPTY(vm)
            break;
        }
        case FALSE_OPCODE:{
            PUSH_BOOL(0, vm);
            break;
        }
        case TRUE_OPCODE:{
            PUSH_BOOL(1, vm);
            break;
        }
        case INT_OPCODE:{
            int64_t i64 = read_i64_const(vm);
            PUSH_INT(i64, vm);
            break;
        }
		case STRING_OPCODE:{
            char *buff = read_str(vm, NULL);
            PUSH_CORE_STR(buff, vm)
            break;
		}
        case ADD_OPCODE:{
            Value *vb = pop(vm);
            Value *va = pop(vm);

            Str *astr = NULL;
            Str *bstr = NULL;

            vm_utils_is_str(va, &astr);
            vm_utils_is_str(vb, &bstr);

            if(astr || bstr){
                if(!astr)
                    vm_utils_error(vm, "Expect string at left side of string concatenation.");
                if(!bstr)
                    vm_utils_error(vm, "Expect string at right side of string concatenation.");

                char *buff = join_buff(astr->buff, astr->len, bstr->buff, bstr->len, vm);
                PUSH_UNCORE_STR(buff, vm);

                break;
            }

            if(!vm_utils_is_i64(va, NULL))
                vm_utils_error(vm, "Expect int at left side of sum.");

            if(!vm_utils_is_i64(vb, NULL))
                vm_utils_error(vm, "Expect int at right side of sum.");

            int64_t right = vb->literal.i64;
            int64_t left = va->literal.i64;

            PUSH_INT(left + right, vm)
            
            break;
        }
        case SUB_OPCODE:{
            int64_t right = pop_i64_assert(vm, "Expect integer at right side.");
            int64_t left = pop_i64_assert(vm, "Expect integer at left side.");
            PUSH_INT(left - right, vm)
            break;
        }
        case MUL_OPCODE:{
            Value *vb = pop(vm);
            Value *va = pop(vm);

            Str *bstr = NULL;
            Str *astr = NULL;

            vm_utils_is_str(va, &astr);
            vm_utils_is_str(vb, &bstr);

            if(astr || bstr){
                int64_t by = -1;
                char *in_buff = NULL;
                size_t in_buff_len = 0;

                if(astr){
                    in_buff = astr->buff;
                    in_buff_len = astr->len;
                    
                    if(!vm_utils_is_i64(vb, &by))
                        vm_utils_error(vm, "Expect integer at right side of string multiplication.");
                }

                if(bstr){
                    in_buff = bstr->buff;
                    in_buff_len = bstr->len;

                    if(!vm_utils_is_i64(va, &by))
                        vm_utils_error(vm, "Expect integer at left side of string multiplication.");
                }

                char *buff = multiply_buff(in_buff, in_buff_len, (size_t)by, vm);
                PUSH_UNCORE_STR(buff, vm);

                break;
            }

            if(!vm_utils_is_i64(va, NULL))
                vm_utils_error(vm, "Expect integer at left side of multiplication.");

            if(!vm_utils_is_i64(vb, NULL))
                vm_utils_error(vm, "Expect integer at right side of multiplication.");

            int64_t right = vb->literal.i64;
            int64_t left = va->literal.i64;
            
            PUSH_INT(left * right, vm);
            
            break;

        }
        case DIV_OPCODE:{
            int64_t right = pop_i64_assert(vm, "Expect integer at right side.");
            int64_t left = pop_i64_assert(vm, "Expect integer at left side.");
            PUSH_INT(left / right, vm)
            break;
        }
		case MOD_OPCODE:{
			int64_t right = pop_i64_assert(vm, "Expect integer at right side.");
            int64_t left = pop_i64_assert(vm, "Expect integer at left side.");
            PUSH_INT(left % right, vm)
            break;

		}
        case LT_OPCODE:{
            int64_t right = pop_i64_assert(vm, "Expect integer at right side.");
            int64_t left = pop_i64_assert(vm, "Expect integer at left side.");
            PUSH_BOOL(left < right, vm)
            break;
        }
        case GT_OPCODE:{
            int64_t right = pop_i64_assert(vm, "Expect integer at right side.");
            int64_t left = pop_i64_assert(vm, "Expect integer at left side.");
            PUSH_BOOL(left > right, vm)
            break;
        }
        case LE_OPCODE:{
            int64_t right = pop_i64_assert(vm, "Expect integer at right side.");
            int64_t left = pop_i64_assert(vm, "Expect integer at left side.");
            PUSH_BOOL(left <= right, vm)
            break;
        }
        case GE_OPCODE:{
            int64_t right = pop_i64_assert(vm, "Expect integer at right side.");
            int64_t left = pop_i64_assert(vm, "Expect integer at left side.");
            PUSH_BOOL(left >= right, vm)
            break;
        }
        case EQ_OPCODE:{
            Value *left_value = pop(vm);
            Value *right_value = pop(vm);

            if((left_value->type == BOOL_VTYPE && right_value->type == BOOL_VTYPE) ||
               (left_value->type == INT_VTYPE && right_value->type == INT_VTYPE)){
                PUSH_BOOL(left_value->literal.i64 == right_value->literal.i64, vm)
                break;
            }

            vm_utils_error(vm, "Illegal type in equals comparison expression.");

            break;
        }
        case NE_OPCODE:{
            Value *left_value = pop(vm);
            Value *right_value = pop(vm);

            if((left_value->type == BOOL_VTYPE && right_value->type == BOOL_VTYPE) ||
               (left_value->type == INT_VTYPE && right_value->type == INT_VTYPE)){
                PUSH_BOOL(left_value->literal.i64 != right_value->literal.i64, vm)
                break;
            }

            vm_utils_error(vm, "Illegal type in not equals comparison expression.");

            break;
        }
        case LSET_OPCODE:{
            Value *value = peek(vm);
            uint8_t index = ADVANCE(vm);
            CURRENT_FRAME(vm)->locals[index] = *value;
            break;
        }
        case LGET_OPCODE:{
            uint8_t index = ADVANCE(vm);
            Value value = CURRENT_FRAME(vm)->locals[index];
            push(value, vm);
            break;
        }
        case GSET_OPCODE:{
            Value *value = peek(vm);
            uint32_t hash = (uint32_t)read_i32(vm);
            Module *module = CURRENT_FRAME(vm)->fn->module;
            LZHTable *globals = module->globals;
            LZHTableNode *node = NULL;

            if(lzhtable_hash_contains(hash, globals, &node))
                *(Value *)node->value = *value;
            else{
                Value *new_value = vm_utils_clone_value(value, vm);
                lzhtable_hash_put(hash, new_value, globals);
            }

            break;
        }
        case GGET_OPCODE:{
            uint32_t hash = 0;
            char *symbol = read_str(vm, &hash);
            Module *module = CURRENT_FRAME(vm)->fn->module;
            LZHTable *globals = module->globals;
            LZHTableNode *node = NULL;

            if(!lzhtable_hash_contains(hash, globals, &node))
                vm_utils_error(vm, "Global symbol '%s' does not exists", symbol);
            
            push(*(Value *)node->value, vm);

            break;
        }
        case NGET_OPCODE:{
            char *key = read_str(vm, NULL);
            size_t key_size = strlen(key);
            LZHTableNode *node = NULL;

            if(lzhtable_contains((uint8_t *)key, key_size, vm->natives, &node)){
                NativeFn *native = (NativeFn *)node->value;
                PUSH_NATIVE_FN(native, vm);
                break;
            }

            vm_utils_error(vm, "Unknown native '%s'", (char *)key);

            break;
        }
		case SGET_OPCODE:{
			char *key = read_str(vm, NULL);
            Module *module = CURRENT_FRAME(vm)->fn->module;
			push_module_symbol(key, module, vm);
			break;
		}
		case PUT_OPCODE:{
			Value *target = pop(vm);
			Value *value = peek(vm);
			char *symbol = read_str(vm, NULL);
			Record *record = NULL;
	
			if(!vm_utils_is_record(target, &record))
				vm_utils_error(vm, "Expect a record, but got something else");

			uint8_t *key = (uint8_t *)symbol;
			size_t key_size = strlen(symbol);
			uint32_t hash = lzhtable_hash(key, key_size);
			LZHTableNode *node = NULL;

			if(!lzhtable_hash_contains(hash, record->key_values, &node))
				vm_utils_error(vm, "Record do not contains key '%s'", symbol);

            *(Value *)node->value = *value;

			break;
		}
        case OR_OPCODE:{
            uint8_t right = pop_bool_assert(vm, "Expect bool at right side.");
            uint8_t left = pop_bool_assert(vm, "Expect bool at left side.");
            PUSH_BOOL(left || right, vm)
            break;
        }
        case AND_OPCODE:{
            uint8_t right = pop_bool_assert(vm, "Expect bool at right side.");
            uint8_t left = pop_bool_assert(vm, "Expect bool at left side.");
            PUSH_BOOL(left && right, vm)
            break;
        }
        case NNOT_OPCODE:{
            int64_t right = pop_i64_assert(vm, "Expect integer at right side.");
            PUSH_INT(-right, vm)
            break;
        }
        case NOT_OPCODE:{
            uint8_t right = pop_bool_assert(vm, "Expect bool at right side.");
            PUSH_BOOL(!right, vm)
            break;
        }
        case PRT_OPCODE:{
            Value *value = pop(vm);
            print_value(value);
            break;
        }
        case POP_OPCODE:{
            pop(vm);
            break;
        }
        case JMP_OPCODE:{
            int32_t jmp_value = read_i32(vm);
            if(jmp_value == 0) break;
            
            if(jmp_value > 0) CURRENT_FRAME(vm)->ip += jmp_value - 1;
            else CURRENT_FRAME(vm)->ip += jmp_value - 5;

            break;
        }
		case JIF_OPCODE:{
			uint8_t condition = pop_bool_assert(vm, "Expect 'bool' as conditional value.");
			int32_t jmp_value = read_i32(vm);
			if(jmp_value == 0) break;

            if(!condition){
                if(jmp_value > 0) CURRENT_FRAME(vm)->ip += jmp_value - 1;
                else CURRENT_FRAME(vm)->ip += jmp_value - 5;
            }

			break;
		}
		case JIT_OPCODE:{
			uint8_t condition = pop_bool_assert(vm, "Expect 'bool' as conditional value.");
			int32_t jmp_value = read_i32(vm);
			if(jmp_value == 0) break;

			if(condition){
                if(jmp_value > 0) CURRENT_FRAME(vm)->ip += jmp_value - 1;
                else CURRENT_FRAME(vm)->ip += jmp_value - 5;
            }

			break;
		}
		case LIST_OPCODE:{
			int32_t len = read_i32(vm);
			DynArr *list = assert_ptr(vm_utils_dyarr(vm), vm);

			for(int32_t i = 0; i < len; i++){
				Value *value = pop(vm);

				if(dynarr_insert(value, list))
					vm_utils_error(vm, "Failed to insert value at list: out of memory\n");		
			}

			Obj *obj = vm_utils_obj(LIST_OTYPE, vm);
			obj->value.list = list;

			Value value = {0};
			value.type = OBJ_VTYPE;
			value.literal.obj = obj;

			push(value, vm);

			break;
		}
        case DICT_OPCODE:{
            int32_t len = read_i32(vm);
            
            Obj *dict_obj = vm_utils_dict_obj(vm);
            if(!dict_obj) vm_utils_error(vm, "Out of memory");

            LZHTable *dict = dict_obj->value.dict;

            for (int32_t i = 0; i < len; i++){
                Value *value = pop(vm);
                Value *key = pop(vm);
                
                Value *key_clone = vm_utils_clone_value(key, vm);
                Value *value_clone = vm_utils_clone_value(value, vm);

                if(!key_clone || !value_clone){
                    free(key_clone);
                    free(value_clone);
                    vm_utils_error(vm, "Out of memory");
                }

                uint32_t hash = vm_utils_hash_value(key_clone);
                
                if(lzhtable_hash_put_key(key_clone, hash, value_clone, dict))
                    vm_utils_error(vm, "Out of memory");
            }

            push(OBJ_VALUE(dict_obj), vm);

            break;
        }
		case RECORD_OPCODE:{
			uint8_t len = ADVANCE(vm);
			Obj *record_obj = vm_utils_record_obj(len == 0, vm);

			if(!record_obj) vm_utils_error(vm, "Out of memory");

			if(len == 0){
				push(OBJ_VALUE(record_obj), vm);
				break;
			}

			Record *record = record_obj->value.record;
	
			for(size_t i = 0; i < len; i++){
				char *key = read_str(vm, NULL);
				Value *value = pop(vm);

				char *cloned_key = vm_utils_clone_buff(key, vm);
				Value *cloned_value = vm_utils_clone_value(value, vm);

				if(!cloned_key || !cloned_value){
					free(cloned_key);
					free(cloned_value);
					vm_utils_error(vm, "Out of memory");
				}

				uint8_t *k = (uint8_t *)cloned_key;
				size_t k_size = strlen(cloned_key);
				uint32_t hash = lzhtable_hash(k, k_size);

				if(lzhtable_hash_put_key(cloned_key, hash, cloned_value, record->key_values)){
					free(cloned_key);
					free(cloned_value);
					vm_utils_error(vm, "Out of memory");
				}
			}

			push(OBJ_VALUE(record_obj), vm);	

			break;
		}
        case CALL_OPCODE:{
            Fn *fn = NULL;
            NativeFn *native_fn = NULL;
            uint8_t args_count = ADVANCE(vm);

            if(vm_utils_is_function(peek_at(args_count, vm), &fn)){
                uint8_t params_count = fn->params ? fn->params->used : 0;

                if(params_count != args_count)
                    vm_utils_error(vm, "Failed to call function '%s'. Declared with %d parameter(s), but got %d argument(s)", fn->name, params_count, args_count);

                frame_up_fn(fn, vm);

                if(args_count > 0){
                    int from = (int)(args_count == 0 ? 0 : args_count - 1);

                    for (int i = from; i >= 0; i--)
                        CURRENT_FRAME(vm)->locals[i] = *pop(vm);
                }

                pop(vm);
            }else if(vm_utils_is_native_function(peek_at(args_count, vm), &native_fn)){
                void *target = native_fn->target;
                RawNativeFn native = native_fn->native;

                if(native_fn->arity != args_count)
                    vm_utils_error(
                        vm,
                        "Failed to call function '%s'.\n\tDeclared with %d parameter(s), but got %d argument(s).",
                        native_fn->name,
                        native_fn->arity,
                        args_count
                    );

                Value values[args_count];
                
                for (int i = args_count; i > 0; i--)
                    values[i - 1] = *pop(vm);
                
                pop(vm);

                Value out_value = native(args_count, values, target, vm);
                push(out_value, vm);
            }else
                vm_utils_error(vm, "Expect function after %d parameters count.", args_count);

            break;
        }
        case ACCESS_OPCODE:{
            Str *str = NULL;
            DynArr *list = NULL;
            LZHTable *dict = NULL;
			Record *record = NULL;
			Module *module = NULL;
            char *symbol = read_str(vm, NULL);
            Value *value = pop(vm);

            if(vm_utils_is_str(value, &str)){
                if(strcmp(symbol, "len") == 0){
                    PUSH_INT((int64_t)str->len, vm)
                }else if(strcmp(symbol, "is_core") == 0){
                    PUSH_BOOL((uint8_t)str->core, vm)
                }else if(strcmp(symbol, "char_at") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(1, "char_at", str, native_fn_str_char_at, vm), vm);
                    PUSH_NATIVE_FN(native_fn, vm);
                }else if(strcmp(symbol, "sub_str") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(2, "sub_str", str, native_fn_str_sub_str, vm), vm);
                    PUSH_NATIVE_FN(native_fn, vm);
                }else if(strcmp(symbol, "char_code") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(1, "char_code", str, native_fn_str_char_code, vm), vm);
                    PUSH_NATIVE_FN(native_fn, vm);
                }else if(strcmp(symbol, "split") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(1, "split", str, native_fn_str_split, vm), vm);
                    PUSH_NATIVE_FN(native_fn, vm);
                }else if(strcmp(symbol, "lstrip") == 0){
					NativeFn *native_fn = assert_ptr(vm_utils_native_function(0, "lstrip", str, native_fn_str_lstrip, vm), vm);
					PUSH_NATIVE_FN(native_fn, vm);
				}else if(strcmp(symbol, "rstrip") == 0){
					NativeFn *native_fn = assert_ptr(vm_utils_native_function(0, "rstrip", str, native_fn_str_rstrip, vm), vm);
					PUSH_NATIVE_FN(native_fn, vm);
				}else if(strcmp(symbol, "strip") == 0){
					NativeFn *native_fn = assert_ptr(vm_utils_native_function(0, "strip", str, native_fn_str_strip, vm), vm);
					PUSH_NATIVE_FN(native_fn, vm);
				}else if(strcmp(symbol, "lower") == 0){
					NativeFn *native_fn = assert_ptr(vm_utils_native_function(0, "lower", str, native_fn_str_lower, vm), vm);
					PUSH_NATIVE_FN(native_fn, vm);
				}else if(strcmp(symbol, "upper") == 0){
					NativeFn *native_fn = assert_ptr(vm_utils_native_function(0, "upper", str, native_fn_str_upper, vm), vm);
					PUSH_NATIVE_FN(native_fn, vm);
				}else if(strcmp(symbol, "title") == 0){
					NativeFn *native_fn = assert_ptr(vm_utils_native_function(0, "title", str, native_fn_str_title, vm), vm);
					PUSH_NATIVE_FN(native_fn, vm);
				}else if(strcmp(symbol, "compare") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(1, "compare", str, native_fn_str_cmp, vm), vm);
                    PUSH_NATIVE_FN(native_fn, vm);
                }else if(strcmp(symbol, "compare_ignore") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(1, "compare_ignore", str, native_fn_str_cmp_ic, vm), vm);
                    PUSH_NATIVE_FN(native_fn, vm);
                }else{
                    vm_utils_error(vm, "string do not have symbol named as '%s'", symbol);
                }

                break;
            }

            if(vm_utils_is_list(value, &list)){
                if(strcmp(symbol, "size") == 0){
                    PUSH_INT((int64_t)list->used, vm)
                }else if(strcmp(symbol, "capacity") == 0){
                    PUSH_INT((int64_t)(list->count), vm)
                }else if(strcmp(symbol, "available") == 0){
                    PUSH_INT((int64_t)DYNARR_AVAILABLE(list), vm)
                }else if(strcmp(symbol, "first") == 0){
                    if(DYNARR_LEN(list) == 0) PUSH_EMPTY(vm)
                    else push(DYNARR_GET_AS(Value, 0, list), vm);
                }else if(strcmp(symbol, "last") == 0){
                    if(DYNARR_LEN(list) == 0) PUSH_EMPTY(vm)
                    else push(DYNARR_GET_AS(Value, DYNARR_LEN(list) - 1, list), vm);
                }else if(strcmp(symbol, "get") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(1, "get", list, native_fn_list_get, vm), vm);
                    PUSH_NATIVE_FN(native_fn, vm);
                }else if(strcmp(symbol, "insert") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(1, "insert", list, native_fn_list_insert, vm), vm);
                    PUSH_NATIVE_FN(native_fn, vm);
                }else if(strcmp(symbol, "insert_at") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(2, "insert_at", list, native_fn_list_insert_at, vm), vm);
                    PUSH_NATIVE_FN(native_fn, vm);
                }else if(strcmp(symbol, "set") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(2, "set", list, native_fn_list_set, vm), vm);
                    PUSH_NATIVE_FN(native_fn, vm);
                }else if(strcmp(symbol, "remove") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(1, "remove", list, native_fn_list_remove, vm), vm);
                    PUSH_NATIVE_FN(native_fn, vm);
                }else if(strcmp(symbol, "append") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(1, "append", list, native_fn_list_append, vm), vm);
                    PUSH_NATIVE_FN(native_fn, vm);
                }else if(strcmp(symbol, "append_new") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(1, "append_new", list, native_fn_list_append_new, vm), vm);
                    PUSH_NATIVE_FN(native_fn, vm);
                }else if(strcmp(symbol, "clear") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(0, "clear", list, native_fn_list_clear, vm), vm);
                    PUSH_NATIVE_FN(native_fn, vm);
                }else if(strcmp(symbol, "reverse") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(0, "reverse", list, native_fn_list_reverse, vm), vm);
                    PUSH_NATIVE_FN(native_fn, vm);
                }else{
                    vm_utils_error(vm, "list do not have symbol named as '%s'", symbol);
                }

                break;
            }

            if(vm_utils_is_dict(value, &dict)){
                if(strcmp(symbol, "contains") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(1, "contains", dict, native_fn_dict_contains, vm), vm);
                    PUSH_NATIVE_FN(native_fn, vm);
                }else if(strcmp(symbol, "get") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(1, "get", dict, native_fn_dict_get, vm), vm);
                    PUSH_NATIVE_FN(native_fn, vm);
                }else if(strcmp(symbol, "put") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(2, "put", dict, native_fn_dict_put, vm), vm);
                    PUSH_NATIVE_FN(native_fn, vm);
                }else if(strcmp(symbol, "remove") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(1, "remove", dict, native_fn_dict_remove, vm), vm);
                    PUSH_NATIVE_FN(native_fn, vm);
                }else if(strcmp(symbol, "keys") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(0, "keys", dict, native_fn_dict_keys, vm), vm);
                    PUSH_NATIVE_FN(native_fn, vm);
                }else if(strcmp(symbol, "values") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(0, "values", dict, native_fn_dict_values, vm), vm);
                    PUSH_NATIVE_FN(native_fn, vm);
                }else{
                    vm_utils_error(vm, "Dictionary do not have symbol named as '%s'", symbol);
                }

                break;
            }

			if(vm_utils_is_record(value, &record)){
				uint8_t *key = (uint8_t *)symbol;
				size_t key_size = strlen(symbol);
				uint32_t hash = lzhtable_hash(key, key_size);
				LZHTableNode *node = NULL;

				if(!lzhtable_hash_contains(hash, record->key_values, &node))
					vm_utils_error(vm, "Record do not constains key '%s'", symbol);

				Value *value = (Value *)node->value;

				push(*value, vm);

				break;
			}

			if(vm_utils_is_module(value, &module)){
				push_module_symbol(symbol, module, vm);
				break;
			}

            vm_utils_error(vm, "Illegal target to access.");

            break;
        }
        case RET_OPCODE:{
            frame_down(vm);
            break;
        }
		case IS_OPCODE:{
			Value *value = pop(vm);
			uint8_t type = ADVANCE(vm);

			if(IS_OBJ(value)){
				Obj *obj = TO_OBJ(value);

				switch(obj->type){
					case STR_OTYPE:{
						push(BOOL_VALUE(type == 3), vm);
						break;
					}case LIST_OTYPE:{
						push(BOOL_VALUE(type == 4), vm);
						break;
					}case DICT_OTYPE:{
						push(BOOL_VALUE(type == 5), vm);
						break;
					}case RECORD_OTYPE:{
						push(BOOL_VALUE(type == 6), vm);
						break;
					}default:{
						vm_utils_error(vm, "Illegal object type");
                        break;
					}
				}
			}else{
				switch(value->type){
					case EMPTY_VTYPE:{
						push(BOOL_VALUE(type == 0), vm);
						break;
					}case BOOL_VTYPE:{
						push(BOOL_VALUE(type == 1), vm);
						break;
					}case INT_VTYPE:{
						push(BOOL_VALUE(type == 2), vm);
						break;
					}default:{
						vm_utils_error(vm, "Illegal value type");
                        break;
					}
				}
			}

			break;
		}
        case THROW_OPCODE:{
			if(vm->frame_ptr == 0)
				vm_utils_error(vm, "Can not throw in a empty frame stack");
            
            Value *value = pop(vm);
            Str *throw_message = NULL;

            if(IS_STR(value)){
                throw_message = TO_STR(value);
            }else if(IS_RECORD(value)){
                Record *record = TO_RECORD(value);

                if(record->key_values){
                    char *key = "message";
                    
                    uint8_t *k = (uint8_t *)key;
                    size_t ks = strlen(key);
                    
                    LZHTableNode *message_node = NULL;

                    if(lzhtable_contains(k, ks, record->key_values, &message_node)){
                        Value *value = (Value *)message_node->value;

                        if(!IS_STR(value))
                            vm_utils_error(vm, "Expect record attribute 'message' to be of type string");

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
                LZHTable *fn_tries = module->tries;
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

						if(ip < start || ip > end) continue;

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
                if(throw_message)
                    vm_utils_error(vm, throw_message->buff);
                else
                    vm_utils_error(vm, "");
            }
            
            break;
        }
        default:{
            assert("Illegal opcode");
        }
    }
}

void resolve_module(Module *module, VM *vm){
    Module *old_module = vm->module;

    vm->module = module;
    frame_up("import", vm);

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
        module->to_resolve = 1;

        vm->stack_ptr = 0;
        vm->natives = natives;
        vm->module = module;

        frame_up("main", vm);

        while (!IS_AT_END(vm)){
            uint8_t chunk = ADVANCE(vm);
            execute(chunk, vm);
        }

        frame_down(vm);

        vm_utils_clean_up(vm);

        return 0;
    }
}
