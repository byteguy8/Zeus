#include "vm.h"
#include "memory.h"
#include "opcode.h"
#include "function.h"
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <setjmp.h>

static jmp_buf err_jmp;

static void error(VM *vm, char *msg, ...){
    va_list args;
	va_start(args, msg);

	fprintf(stderr, "Runtime error:\n\t");
	vfprintf(stderr, msg, args);
    fprintf(stderr, "\n");

	va_end(args);

    longjmp(err_jmp, 1);
}

static char *join_buff(char *buffa, size_t sza, char *buffb, size_t szb, VM *vm){
    size_t szc = sza + szb;
    char *buff = malloc(szc + 1);

	if(!buff)
		error(vm, "Failed to create buffer: out of memory.");

    memcpy(buff, buffa, sza);
    memcpy(buff + sza, buffb, szb);
    buff[szc] = '\0';

    return buff;
}

static char *multiply_buff(char *buff, size_t szbuff, size_t by, VM *vm){
	size_t sz = szbuff * by;
	char *b = malloc(sz + 1);

	if(!b)
		error(vm, "failed to create buffer: out of memory.");

	for(size_t i = 0; i < by; i++)
		memcpy(b + (i * szbuff), buff, szbuff);

	b[sz] = '\0';

	return b;
}

static Frame *current_frame(VM *vm){
    if(vm->frame_ptr == 0)
        error(vm, "Frame stack is empty");

    return &vm->frame_stack[vm->frame_ptr - 1];
}

#define CURRENT_LOCALS(vm)(current_frame(vm)->locals)

static Frame *frame_up(size_t fn_index, VM *vm){
    if(vm->frame_ptr >= FRAME_LENGTH)
        error(vm, "FrameOverFlow");

    DynArrPtr *functions = vm->functions;
    
    if(fn_index >= functions->used)
        error(vm, "Illegal function index.");

    Function *fn = DYNARR_PTR_GET(fn_index, functions);
    Frame *frame = &vm->frame_stack[vm->frame_ptr++];

    frame->ip = 0;
    frame->name = fn->name;
    frame->chunks = fn->chunks;

    return frame;
}

static Frame *frame_up_fn(Function *fn, VM *vm){
    if(vm->frame_ptr >= FRAME_LENGTH)
        error(vm, "FrameOverFlow");
        
    Frame *frame = &vm->frame_stack[vm->frame_ptr++];

    frame->ip = 0;
    frame->name = fn->name;
    frame->chunks = fn->chunks;

    return frame;
}

static void frame_down(VM *vm){
    if(vm->frame_ptr == 0)
        error(vm, "FrameUnderFlow");

    vm->frame_ptr--;
}

static int is_i64(Value *value, int64_t *i64){
    if(value->type != INT_VTYPE) return 0;
    if(i64) *i64 = value->literal.i64;
    return 1;
}

static int is_str(Value *value, Str **str){
    if(value->type != OBJ_VTYPE) return 0;
    
    Obj *obj = value->literal.obj;

    if(obj->type == STRING_OTYPE){
        Str *ostr = &obj->value.str;
        if(str) *str = ostr;
    
        return 1;
    }

    return 0;
}

static int is_list(Value *value, DynArr **list){
	if(value->type != OBJ_VTYPE) return 0;
	
	Obj *obj = value->literal.obj;

	if(obj->type == LIST_OTYPE){
		DynArr *olist = obj->value.list;
		if(list) *list = olist;
		return 1;
	}

	return 0;
}

static int is_function(Value *value, Function **out_fn){
	if(value->type != OBJ_VTYPE) return 0;
	
	Obj *obj = value->literal.obj;

	if(obj->type == FN_OTYPE){
		Function *ofn = obj->value.fn;
		if(out_fn) *out_fn = ofn;
		return 1;
	}

	return 0;
}

#define TO_STR(v)(&v->literal.obj->value.str)

static int32_t compose_i32(uint8_t *bytes){
    return ((int32_t)bytes[3] << 24) | ((int32_t)bytes[2] << 16) | ((int32_t)bytes[1] << 8) | ((int32_t)bytes[0]);
}

void print_object(Obj *object){
    switch (object->type){
        case STRING_OTYPE:{
            Str *str = &object->value.str;
            printf("%s\n", str->buff);
            break;
        }
   		case LIST_OTYPE:{
			DynArr *list = object->value.list;
			printf("<list %ld at %p>\n", list->used, list);
			break;
		}
        case FN_OTYPE:{
            Function *fn = (Function *)object->value.fn;
            printf("<fn '%s' - %d at %p>\n", fn->name, (uint8_t)(fn->params ? fn->params->used : 0), fn);
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
            print_object(value->literal.obj);
            break;
        }
        default:{
            assert("Illegal value type");
        }
    }
}

void print_stack(VM *vm){
    for (int i = 0; i < vm->stack_ptr; i++)
    {
        Value *value = &vm->stack[i];
        printf("%d --> ", i + 1);
        print_value(value);
    }
}

static int is_at_end(VM *vm){
    Frame *frame = current_frame(vm);
    DynArr *chunks = frame->chunks;
    return frame->ip >= chunks->used;
}

static uint8_t advance(VM *vm){
    Frame *frame = current_frame(vm);
    DynArr *chunks = frame->chunks;
    return *(uint8_t *)dynarr_get(frame->ip++, chunks);
}

static int32_t read_i32(VM *vm){
	uint8_t bytes[4];

	for(size_t i = 0; i < 4; i++)
		bytes[i] = advance(vm);

	return compose_i32(bytes);
}

static int64_t read_i64_const(VM *vm){
    DynArr *constants = vm->constants;
    int32_t index = read_i32(vm);
    return *(int64_t *)dynarr_get(index, constants);
}

static void destroy_obj(Obj *obj, VM *vm){
    Obj *prev = obj->prev;
    Obj *next = obj->next;

    if(prev) prev->next = next;
	if(next) next->prev = prev;

	if(vm->head == obj) vm->head = next;
	if(vm->tail == obj) vm->tail = prev;

	switch(obj->type){
		case STRING_OTYPE:{
			Str *str = (Str *)&obj->value.str;
			if(!str->core) free(str->buff);
			break;
		}
		case LIST_OTYPE:{
			DynArr *list = obj->value.list;
			dynarr_destroy(list);
			break;
		}
		default:{
			assert("Illegal object type");
		}
	}

	free(obj);
}

static void clean_up(VM *vm){
    while (vm->head)
        destroy_obj(vm->head, vm);
}

static Obj *create_obj(ObjType type, VM *vm){
    Obj *obj = malloc(sizeof(Obj));
    
    if(!obj) error(vm, "Failed to allocate object: out of memory.");

    memset(obj, 0, sizeof(Obj));
    obj->type = type;
    
    if(vm->tail){
        obj->prev = vm->tail;
        vm->tail->next = obj;
    }else vm->head = obj;

    vm->tail = obj;

    return obj;
}

static void push(Value value, VM *vm){
    if(vm->stack_ptr >= STACK_LENGTH) error(vm, "StackOverFlow");
    Value *current_value = &vm->stack[vm->stack_ptr++];
    memcpy(current_value, &value, sizeof(Value));
}

static Value *pop(VM *vm){
    if(vm->stack_ptr == 0) error(vm, "StackUnderFlow");
    return &vm->stack[--vm->stack_ptr];
}

static Value *peek(VM *vm){
    if(vm->stack_ptr == 0) error(vm, "StackUnderFlow");
    return &vm->stack[vm->stack_ptr - 1];
}

static Value *peek_at(size_t offset, VM *vm){
    if(1 + offset > vm->stack_ptr) error(vm, "Illegal offset");
    size_t at = vm->stack_ptr - 1 - offset;
    return &vm->stack[at];
}

void push_bool(uint8_t bool, VM *vm){
    Value value = {0};
    value.type = BOOL_VTYPE;
    value.literal.bool = bool;
    
    push(value, vm);
}

void push_i64(int64_t i64, VM *vm){
    Value value = {0};
    value.type = INT_VTYPE;
    value.literal.i64 = i64;
    
    push(value, vm);
}

void push_string(char *buff, char core, VM *vm){
    Obj *obj = create_obj(STRING_OTYPE, vm);
    Str *str = &obj->value.str;

    str->core = core;
    str->len = strlen(buff);
    str->buff = buff;
    
    Value value = {0};
    value.type = OBJ_VTYPE;
    value.literal.obj = obj;

    push(value, vm);
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

    longjmp(err_jmp, 1);
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

    longjmp(err_jmp, 1);
}

void assert_value_type(ValueType type, Value *value, char *err_msg, ...){
    ValueType vtype = value->type;
    if(vtype == type) return;

    va_list args;
	va_start(args, err_msg);

	fprintf(stderr, "Runtime error:\n\t");
	vfprintf(stderr, err_msg, args);
    fprintf(stderr, "\n");

	va_end(args);

    longjmp(err_jmp, 1);
}

static void execute(uint8_t chunk, VM *vm){
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
			uint32_t hash = (uint32_t)read_i32(vm);
            char *str = lzhtable_hash_get(hash, vm->strings);
            push_string(str, 1, vm);
			break;
		}
        case ADD_OPCODE:{
            Value *vb = pop(vm);
            Value *va = pop(vm);

            if(is_str(va, NULL)){
                Str *astr = TO_STR(va);
                Str *bstr = NULL;

                if(!is_str(vb, &bstr))
                    error(vm, "Expect string at right side of string concatenation.");

                char *buff = join_buff(astr->buff, astr->len, bstr->buff, bstr->len, vm);
                push_string(buff, 0, vm);

                break;
            }

            if(!is_i64(va, NULL))
                error(vm, "Expect int at left side of string sum.");

            if(!is_i64(vb, NULL))
                error(vm, "Expect int at right side of string sum.");

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

            if(is_str(va, NULL)){
                Str *str = TO_STR(va);
                int64_t by = -1;

                if(!is_i64(vb, &by))
                    error(vm, "Expect integer at right side of string multiplication.");

				if(by < 0)
					error(vm, "Negative values not allowed in string multiplication.");

                char *buff = multiply_buff(str->buff, str->len, (size_t)by, vm);
                push_string(buff, 0, vm);

                break;
            }

			if(is_str(vb, NULL)){
                Str *str = TO_STR(vb);
                int64_t by = -1;

                if(!is_i64(va, &by))
                    error(vm, "Expect integer at left side of string multiplication.");

				if(by < 0)
					error(vm, "Negative values not allowed in string multiplication.");

                char *buff = multiply_buff(str->buff, str->len, (size_t)by, vm);
                push_string(buff, 0, vm);

                break;
            }


            if(!is_i64(va, NULL))
                error(vm, "Expect int at left side of string sum.");

            if(!is_i64(vb, NULL))
                error(vm, "Expect int at right side of string sum.");

            int64_t right = vb->literal.i64;
            int64_t left = va->literal.i64;
            
            push_i64(left + right, vm);
            
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

            error(vm, "Illegal type in equals comparison expression.");

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

            error(vm, "Illegal type in not equals comparison expression.");

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
        case SGET_OPCODE:{
            int32_t index = read_i32(vm);
            DynArrPtr *functions = vm->functions;
            Function *function = (Function *)DYNARR_PTR_GET((size_t)index, functions);

            Obj *fn_obj = create_obj(FN_OTYPE, vm);
            fn_obj->value.fn = function;

            Value value = {0};
            value.type = OBJ_VTYPE;
            value.literal.obj = fn_obj;

            push(value, vm);

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

			DynArr *list = dynarr_create(sizeof(Value), NULL);
			if(!list) error(vm, "Failed to create list: out of memory\n");

			for(int32_t i = 0; i < len; i++){
				Value *value = pop(vm);

				if(dynarr_insert(value, list))
					error(vm, "Failed to insert value at list: out of memory\n");		
			}

			Obj *obj = create_obj(LIST_OTYPE, vm);
			obj->value.list = list;

			Value value = {0};
			value.type = OBJ_VTYPE;
			value.literal.obj = obj;

			push(value, vm);

			break;
		}
        case CALL_OPCODE:{
            Function *fn = NULL;
            uint8_t args_count = advance(vm);

            if(!is_function(peek_at(args_count, vm), &fn))
                error(vm, "Expect function after %d parameters count.", args_count);

            uint8_t params_count = fn->params ? fn->params->used : 0;

            if(params_count != args_count)
                error(vm, "Failed to call function '%s'.\n\tDeclared with %d parameter(s), but got %d argument(s).", fn->name, params_count, args_count);

            frame_up_fn(fn, vm);

            int from = (int)(args_count == 0 ? 0 : args_count - 1);

            for (int i = from; i >= 0; i--)
                memcpy(current_frame(vm)->locals + i, pop(vm), sizeof(Value));

            pop(vm);

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

VM *vm_create(){
    VM *vm = (VM *)memory_alloc(sizeof(VM));
    memset(vm, 0, sizeof(VM));
    return vm;
}

void vm_print_stack(VM *vm){
    print_stack(vm);
}

int vm_execute(
    DynArr *constants,
    LZHTable *strings,
    DynArrPtr *functions,
    VM *vm
){
    if(setjmp(err_jmp) == 1) return 1;
    else{
        vm->stack_ptr = 0;
        vm->constants = constants;
		vm->strings = strings;
        vm->functions = functions;

        frame_up(0, vm);

        while (!is_at_end(vm)){
            uint8_t chunk = advance(vm);
            execute(chunk, vm);
        }

        frame_down(vm);

        clean_up(vm);

        return 0;
    }
}
