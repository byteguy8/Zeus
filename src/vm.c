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

static void push(Value value, VM *vm);
static void push_bool(uint8_t bool, VM *vm);
static void push_empty(VM *vm);
static void push_i64(int64_t i64, VM *vm);
static void push_str(Str *str, VM *vm);
static void push_list(DynArr *list, VM *vm);
static void push_native_fn(NativeFn *native, VM *vm);
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

Frame *current_frame(VM *vm){
    if(vm->frame_ptr == 0)
        vm_utils_error(vm, "Frame stack is empty");

    return &vm->frame_stack[vm->frame_ptr - 1];
}

#define CURRENT_LOCALS(vm)(current_frame(vm)->locals)

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
            printf("<dict %ld %p>\n", table->n, table);
            break;
        }
        case FN_OTYPE:{
            Fn *fn = (Fn *)object->value.fn;
            printf("<fn '%s' - %d at %p>\n", fn->name, (uint8_t)(fn->params ? fn->params->used : 0), fn);
            break;
        }
        case NATIVE_FN_OTYPE:{
            NativeFn *native_fn = object->value.native_fn;
            printf("<native fn '%s' - %d at %p>\n", native_fn->name, native_fn->arity, native_fn);
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

int is_at_end(VM *vm){
    Frame *frame = current_frame(vm);
    DynArr *chunks = frame->fn->chunks;
    return frame->ip >= chunks->used;
}

uint8_t advance(VM *vm){
    Frame *frame = current_frame(vm);
    DynArr *chunks = frame->fn->chunks;
    return *(uint8_t *)dynarr_get(frame->ip++, chunks);
}

int32_t read_i32(VM *vm){
	uint8_t bytes[4];

	for(size_t i = 0; i < 4; i++)
		bytes[i] = advance(vm);

	return compose_i32(bytes);
}

int64_t read_i64_const(VM *vm){
    Module *module = current_frame(vm)->fn->module;
    DynArr *constants = module->constants;
    int32_t index = read_i32(vm);
    return *(int64_t *)dynarr_get(index, constants);
}

char *read_str(VM *vm, uint32_t *out_hash){
    Module *module = current_frame(vm)->fn->module;
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
    Value *current_value = &vm->stack[vm->stack_ptr++];
    memcpy(current_value, &value, sizeof(Value));
}

Value *peek_at(int offset, VM *vm){
    if(1 + offset > vm->stack_ptr) vm_utils_error(vm, "Illegal offset");
    size_t at = vm->stack_ptr - 1 - offset;
    return &vm->stack[at];
}

void push_bool(uint8_t bool, VM *vm){
    Value value = {0};
    value.type = BOOL_VTYPE;
    value.literal.bool = bool;
    
    push(value, vm);
}

void push_empty(VM *vm){
    Value value = {0};
    value.type = EMPTY_VTYPE;
    
    push(value, vm);   
}

void push_i64(int64_t i64, VM *vm){
    Value value = {0};
    value.type = INT_VTYPE;
    value.literal.i64 = i64;
    
    push(value, vm);
}

void push_str(Str *str, VM *vm){
    Obj *str_obj = vm_utils_obj(STR_OTYPE, vm);
    str_obj->value.str = str;

    Value value = {0};
    value.type = OBJ_VTYPE;
    value.literal.obj = str_obj;

    push(value, vm);
}

void push_list(DynArr *list, VM *vm){
    Obj *obj = vm_utils_obj(LIST_OTYPE, vm);
    obj->value.list = list;

    Value value = {0};
    value.type = OBJ_VTYPE;
    value.literal.obj = obj;

    push(value, vm);
}

void push_fn(Fn *fn, VM *vm){
	Obj *obj = vm_utils_obj(FN_OTYPE, vm);
	if(!obj) vm_utils_error(vm, "Out of memory");
	obj->value.fn = fn;
	push(OBJ_VALUE(obj), vm);
}

void push_native_fn(NativeFn *native, VM *vm){
    Obj *obj = vm_utils_obj(NATIVE_FN_OTYPE, vm);
    obj->value.native_fn = native;

    Value value = {0};
    value.type = OBJ_VTYPE;
    value.literal.obj = obj;

    push(value, vm);
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

    longjmp(vm->err_jmp, 1);
}

void execute(uint8_t chunk, VM *vm){
    switch (chunk){
        case EMPTY_OPCODE:{
            Value value = {0};
            push(value, vm);
            break;
        }
        case FALSE_OPCODE:{
            push_bool(0, vm);
            break;
        }
        case TRUE_OPCODE:{
            push_bool(1, vm);
            break;
        }
        case INT_OPCODE:{
            int64_t i64 = read_i64_const(vm);
            push_i64(i64, vm);
            break;
        }
		case STRING_OPCODE:{
			uint32_t hash = 0;
            char *buff = read_str(vm, &hash);
            Str *str = vm_utils_core_str(buff, hash, vm);
            
            push_str(str, vm);
			
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
                Str *out_str = vm_utils_uncore_str(buff, vm);

                push_str(out_str, vm);

                break;
            }

            if(!vm_utils_is_i64(va, NULL))
                vm_utils_error(vm, "Expect int at left side of sum.");

            if(!vm_utils_is_i64(vb, NULL))
                vm_utils_error(vm, "Expect int at right side of sum.");

            int64_t right = vb->literal.i64;
            int64_t left = va->literal.i64;

            push_i64(left + right, vm);
            
            break;
        }
        case SUB_OPCODE:{
            int64_t right = pop_i64_assert(vm, "Expect integer at right side.");
            int64_t left = pop_i64_assert(vm, "Expect integer at left side.");
            push_i64(left - right, vm);
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
                Str *out_str = vm_utils_uncore_str(buff, vm);
                
                push_str(out_str, vm);

                break;
            }

            if(!vm_utils_is_i64(va, NULL))
                vm_utils_error(vm, "Expect integer at left side of multiplication.");

            if(!vm_utils_is_i64(vb, NULL))
                vm_utils_error(vm, "Expect integer at right side of multiplication.");

            int64_t right = vb->literal.i64;
            int64_t left = va->literal.i64;
            
            push_i64(left * right, vm);
            
            break;

        }
        case DIV_OPCODE:{
            int64_t right = pop_i64_assert(vm, "Expect integer at right side.");
            int64_t left = pop_i64_assert(vm, "Expect integer at left side.");
            push_i64(left / right, vm);
            break;
        }
		case MOD_OPCODE:{
			int64_t right = pop_i64_assert(vm, "Expect integer at right side.");
            int64_t left = pop_i64_assert(vm, "Expect integer at left side.");
            push_i64(left % right, vm);
            break;

		}
        case LT_OPCODE:{
            int64_t right = pop_i64_assert(vm, "Expect integer at right side.");
            int64_t left = pop_i64_assert(vm, "Expect integer at left side.");
            push_bool(left < right, vm);
            break;
        }
        case GT_OPCODE:{
            int64_t right = pop_i64_assert(vm, "Expect integer at right side.");
            int64_t left = pop_i64_assert(vm, "Expect integer at left side.");
            push_bool(left > right, vm);
            break;
        }
        case LE_OPCODE:{
            int64_t right = pop_i64_assert(vm, "Expect integer at right side.");
            int64_t left = pop_i64_assert(vm, "Expect integer at left side.");
            push_bool(left <= right, vm);
            break;
        }
        case GE_OPCODE:{
            int64_t right = pop_i64_assert(vm, "Expect integer at right side.");
            int64_t left = pop_i64_assert(vm, "Expect integer at left side.");
            push_bool(left >= right, vm);
            break;
        }
        case EQ_OPCODE:{
            Value *left_value = pop(vm);
            Value *right_value = pop(vm);

            if((left_value->type == BOOL_VTYPE && right_value->type == BOOL_VTYPE) ||
            (left_value->type == INT_VTYPE && right_value->type == INT_VTYPE)){
                push_bool(left_value->literal.i64 == right_value->literal.i64, vm);
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
                push_bool(left_value->literal.i64 != right_value->literal.i64, vm);
                break;
            }

            vm_utils_error(vm, "Illegal type in not equals comparison expression.");

            break;
        }
        case LSET_OPCODE:{
            Value *value = peek(vm);
            uint8_t index = advance(vm);
            memcpy(&current_frame(vm)->locals[index], value, sizeof(Value));
            break;
        }
        case LGET_OPCODE:{
            uint8_t index = advance(vm);
            Value value = current_frame(vm)->locals[index];
            push(value, vm);
            break;
        }
        case GSET_OPCODE:{
            Value *value = peek(vm);
            uint32_t hash = (uint32_t)read_i32(vm);
            Module *module = current_frame(vm)->fn->module;
            LZHTable *globals = module->globals;
            LZHTableNode *node = NULL;

            if(lzhtable_hash_contains(hash, globals, &node))
                memcpy(node->value, value, sizeof(Value));
            else{
                Value *new_value = vm_utils_clone_value(value, vm);
                lzhtable_hash_put(hash, new_value, globals);
            }

            break;
        }
        case GGET_OPCODE:{
            uint32_t hash = 0;
            char *symbol = read_str(vm, &hash);
            Module *module = current_frame(vm)->fn->module;
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
                push_native_fn(native, vm);
                break;
            }

            vm_utils_error(vm, "Unknown native '%s'", (char *)key);

            break;
        }
		case SGET_OPCODE:{
			char *key = read_str(vm, NULL);
            Module *module = current_frame(vm)->fn->module;
			push_module_symbol(key, module, vm);
			break;
		}
        case OR_OPCODE:{
            uint8_t right = pop_bool_assert(vm, "Expect bool at right side.");
            uint8_t left = pop_bool_assert(vm, "Expect bool at left side.");
            push_bool(left || right, vm);
            break;
        }
        case AND_OPCODE:{
            uint8_t right = pop_bool_assert(vm, "Expect bool at right side.");
            uint8_t left = pop_bool_assert(vm, "Expect bool at left side.");
            push_bool(left && right, vm);
            break;
        }
        case NNOT_OPCODE:{
            int64_t right = pop_i64_assert(vm, "Expect integer at right side.");
            push_i64(-right, vm);
            break;
        }
        case NOT_OPCODE:{
            uint8_t right = pop_bool_assert(vm, "Expect bool at right side.");
            push_bool(!right, vm);
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
            
            if(jmp_value > 0) current_frame(vm)->ip += jmp_value - 1;
            else current_frame(vm)->ip += jmp_value - 5;

            break;
        }
		case JIF_OPCODE:{
			uint8_t condition = pop_bool_assert(vm, "Expect 'bool' as conditional value.");
			int32_t jmp_value = read_i32(vm);
			if(jmp_value == 0) break;

            if(!condition){
                if(jmp_value > 0) current_frame(vm)->ip += jmp_value - 1;
                else current_frame(vm)->ip += jmp_value - 5;
            }

			break;
		}
		case JIT_OPCODE:{
			uint8_t condition = pop_bool_assert(vm, "Expect 'bool' as conditional value.");
			int32_t jmp_value = read_i32(vm);
			if(jmp_value == 0) break;

			if(condition){
                if(jmp_value > 0) current_frame(vm)->ip += jmp_value - 1;
                else current_frame(vm)->ip += jmp_value - 5;
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
        case CALL_OPCODE:{
            Fn *fn = NULL;
            NativeFn *native_fn = NULL;
            uint8_t args_count = advance(vm);

            if(vm_utils_is_function(peek_at(args_count, vm), &fn)){
                uint8_t params_count = fn->params ? fn->params->used : 0;

                if(params_count != args_count)
                    vm_utils_error(vm, "Failed to call function '%s'.\n\tDeclared with %d parameter(s), but got %d argument(s).", fn->name, params_count, args_count);

                frame_up_fn(fn, vm);

                if(args_count > 0){
                    int from = (int)(args_count == 0 ? 0 : args_count - 1);

                    for (int i = from; i >= 0; i--)
                        memcpy(current_frame(vm)->locals + i, pop(vm), sizeof(Value));
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
                    memcpy(values + (i - 1), pop(vm), sizeof(Value));
                
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
			Module *module = NULL;
            char *symbol = read_str(vm, NULL);
            Value *value = pop(vm);

            if(vm_utils_is_str(value, &str)){
                if(strcmp(symbol, "len") == 0){
                    push_i64((int64_t)str->len, vm);
                }else if(strcmp(symbol, "is_core") == 0){
                    push_bool((uint8_t)str->core, vm);
                }else if(strcmp(symbol, "char_at") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(1, "char_at", str, native_fn_char_at, vm), vm);
                    push_native_fn(native_fn, vm);
                }else if(strcmp(symbol, "sub_str") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(2, "sub_str", str, native_fn_sub_str, vm), vm);
                    push_native_fn(native_fn, vm);
                }else if(strcmp(symbol, "char_code") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(1, "char_code", str, native_fn_char_code, vm), vm);
                    push_native_fn(native_fn, vm);
                }else if(strcmp(symbol, "split") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(1, "split", str, native_fn_split, vm), vm);
                    push_native_fn(native_fn, vm);
                }else if(strcmp(symbol, "lstrip") == 0){
					NativeFn *native_fn = assert_ptr(vm_utils_native_function(0, "lstrip", str, native_fn_lstrip, vm), vm);
					push_native_fn(native_fn, vm);
				}else if(strcmp(symbol, "rstrip") == 0){
					NativeFn *native_fn = assert_ptr(vm_utils_native_function(0, "rstrip", str, native_fn_rstrip, vm), vm);
					push_native_fn(native_fn, vm);
				}else if(strcmp(symbol, "strip") == 0){
					NativeFn *native_fn = assert_ptr(vm_utils_native_function(0, "strip", str, native_fn_strip, vm), vm);
					push_native_fn(native_fn, vm);
				}else if(strcmp(symbol, "lower") == 0){
					NativeFn *native_fn = assert_ptr(vm_utils_native_function(0, "lower", str, native_fn_lower, vm), vm);
					push_native_fn(native_fn, vm);
				}else if(strcmp(symbol, "upper") == 0){
					NativeFn *native_fn = assert_ptr(vm_utils_native_function(0, "upper", str, native_fn_upper, vm), vm);
					push_native_fn(native_fn, vm);
				}else if(strcmp(symbol, "title") == 0){
					NativeFn *native_fn = assert_ptr(vm_utils_native_function(0, "title", str, native_fn_title, vm), vm);
					push_native_fn(native_fn, vm);
				}else if(strcmp(symbol, "compare") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(1, "compare", str, native_fn_cmp, vm), vm);
                    push_native_fn(native_fn, vm);
                }else if(strcmp(symbol, "compare_ignore") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(1, "compare_ignore", str, native_fn_cmp_ic, vm), vm);
                    push_native_fn(native_fn, vm);
                }else{
                    vm_utils_error(vm, "string do not have symbol named as '%s'", symbol);
                }

                break;
            }

            if(vm_utils_is_list(value, &list)){
                if(strcmp(symbol, "size") == 0){
                    push_i64((int64_t)list->used, vm);
                }else if(strcmp(symbol, "capacity") == 0){
                    push_i64((int64_t)(list->count - DYNARR_LEN(list)), vm);
                }else if(strcmp(symbol, "first") == 0){
                    if(DYNARR_LEN(list) == 0) push_empty(vm);
                    else push(*(Value *)dynarr_get(0, list), vm);
                }else if(strcmp(symbol, "last") == 0){
                    if(DYNARR_LEN(list) == 0) push_empty(vm);
                    else push(*(Value *)dynarr_get(DYNARR_LEN(list) - 1, list), vm);
                }else if(strcmp(symbol, "get") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(1, "get", list, native_fn_list_get, vm), vm);
                    push_native_fn(native_fn, vm);
                }else if(strcmp(symbol, "insert") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(1, "insert", list, native_fn_list_insert, vm), vm);
                    push_native_fn(native_fn, vm);
                }else if(strcmp(symbol, "insert_at") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(2, "insert_at", list, native_fn_list_insert_at, vm), vm);
                    push_native_fn(native_fn, vm);
                }else if(strcmp(symbol, "set") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(2, "set", list, native_fn_list_set, vm), vm);
                    push_native_fn(native_fn, vm);
                }else if(strcmp(symbol, "remove") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(1, "remove", list, native_fn_list_remove, vm), vm);
                    push_native_fn(native_fn, vm);
                }else if(strcmp(symbol, "append") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(1, "append", list, native_fn_list_append, vm), vm);
                    push_native_fn(native_fn, vm);
                }else if(strcmp(symbol, "clear") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(0, "clear", list, native_fn_list_clear, vm), vm);
                    push_native_fn(native_fn, vm);
                }else{
                    vm_utils_error(vm, "list do not have symbol named as '%s'", symbol);
                }

                break;
            }

            if(vm_utils_is_dict(value, &dict)){
                if(strcmp(symbol, "contains") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(1, "contains", dict, native_dict_contains, vm), vm);
                    push_native_fn(native_fn, vm);
                }else if(strcmp(symbol, "get") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(1, "get", dict, native_dict_get, vm), vm);
                    push_native_fn(native_fn, vm);
                }else if(strcmp(symbol, "put") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(2, "put", dict, native_dict_put, vm), vm);
                    push_native_fn(native_fn, vm);
                }else if(strcmp(symbol, "remove") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(1, "remove", dict, native_fn_dict_remove, vm), vm);
                    push_native_fn(native_fn, vm);
                }else if(strcmp(symbol, "keys") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(0, "keys", dict, native_fn_dict_keys, vm), vm);
                    push_native_fn(native_fn, vm);
                }else if(strcmp(symbol, "values") == 0){
                    NativeFn *native_fn = assert_ptr(vm_utils_native_function(0, "values", dict, native_fn_dict_values, vm), vm);
                    push_native_fn(native_fn, vm);
                }else{
                    vm_utils_error(vm, "Dictionary do not have symbol named as '%s'", symbol);
                }

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
        default:{
            assert("Illegal opcode");
        }
    }
}

void resolve_module(Module *module, VM *vm){
    Module *old_module = vm->module;

    vm->module = module;
    frame_up("import", vm);

    while (!is_at_end(vm)){
        uint8_t chunk = advance(vm);
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
    LZHTable *globals,
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

        while (!is_at_end(vm)){
            uint8_t chunk = advance(vm);
            execute(chunk, vm);
        }

        frame_down(vm);

        vm_utils_clean_up(vm);

        return 0;
    }
}
