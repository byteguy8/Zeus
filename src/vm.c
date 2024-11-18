#include "vm.h"
#include "memory.h"
#include "opcode.h"
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

static char *join_buff(char *buffa, size_t sza, char *buffb, size_t szb){
    size_t szc = sza + szb;
    char *buff = malloc(szc + 1);

    memcpy(buff, buffa, sza);
    memcpy(buff + sza, buffb, szb);
    buff[szc] = '\0';

    return buff;
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
    for (int i = 0; i < vm->temps_ptr; i++)
    {
        Value *value = &vm->temps[i];
        printf("%d --> ", i + 1);
        print_value(value);
    }
}

static int is_at_end(VM *vm){
    DynArr *chunks = vm->chunks;
    return vm->ip >= chunks->used;
}

static uint8_t advance(VM *vm){
    DynArr *chunks = vm->chunks;
    return *(uint8_t *)dynarr_get(vm->ip++, chunks);
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
    if(vm->temps_ptr >= TEMPS_SIZE) error(vm, "StackOverFlow");
    Value *current_value = &vm->temps[vm->temps_ptr++];
    memcpy(current_value, &value, sizeof(Value));
}

static Value *pop(VM *vm){
    if(vm->temps_ptr == 0) error(vm, "StackUnderFlow");
    return &vm->temps[--vm->temps_ptr];
}

static Value *peek(VM *vm){
    if(vm->temps_ptr == 0) error(vm, "StackUnderFlow");
    return &vm->temps[vm->temps_ptr - 1];
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

                char *buff = join_buff(astr->buff, astr->len, bstr->buff, bstr->len);
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
            int64_t right = pop_i64_assert(vm, "Expect integer at right side.");
            int64_t left = pop_i64_assert(vm, "Expect integer at left side.");
            push_i64(left * right, vm);
            break;
        }
        case DIV_OPCODE:{
            int64_t right = pop_i64_assert(vm, "Expect integer at right side.");
            int64_t left = pop_i64_assert(vm, "Expect integer at left side.");
            push_i64(left / right, vm);
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
            memcpy(&vm->locals[index], value, sizeof(Value));
            break;
        }
        case LGET_OPCODE:{
            uint8_t index = advance(vm);
            Value value = vm->locals[index];
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
            
            if(jmp_value > 0) vm->ip += jmp_value - 1;
            else vm->ip += jmp_value - 5;

            break;
        }
		case JIF_OPCODE:{
			uint8_t condition = pop_bool_assert(vm, "Expect 'bool' as conditional value.");
			int32_t jmp_value = read_i32(vm);
			if(jmp_value == 0) break;

            if(!condition){
                if(jmp_value > 0) vm->ip += jmp_value - 1;
                else vm->ip += jmp_value - 5;
            }

			break;
		}
		case JIT_OPCODE:{
			uint8_t condition = pop_bool_assert(vm, "Expect 'bool' as conditional value.");
			int32_t jmp_value = read_i32(vm);
			if(jmp_value == 0) break;

			if(condition){
                if(jmp_value > 0) vm->ip += jmp_value - 1;
                else vm->ip += jmp_value - 5;
            }

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

int vm_execute(DynArr *constants, LZHTable *strings, DynArr *chunks, VM *vm){
    if(setjmp(err_jmp) == 1) return 1;
    else{
        vm->ip = 0;
        vm->temps_ptr = 0;
        vm->constants = constants;
		vm->strings = strings;
        vm->chunks = chunks;

        while (!is_at_end(vm)){
            uint8_t chunk = advance(vm);
            execute(chunk, vm);
        }

        clean_up(vm);

        return 0;
    }
}
