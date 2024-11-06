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

int32_t compose_i32(uint8_t *bytes){
    return ((int32_t)bytes[3] << 24) | ((int32_t)bytes[2] << 16) | ((int32_t)bytes[1] << 8) | ((int32_t)bytes[0]);
}

void print_value(Value *value){
    switch (value->type){
        case NULL_VTYPE:{
            printf("nil\n");
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

int64_t read_i64(VM *vm){
    uint8_t bytes[4];
    
    for (size_t i = 0; i < 4; i++)
        bytes[i] = advance(vm);
    
    DynArr *constants = vm->constants;
    int32_t index = compose_i32(bytes);
    
    return *(int64_t *)dynarr_get(index, constants);
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

void execute(uint8_t chunk, VM *vm){
    switch (chunk){
        case NULL_OPCODE:{
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
            int64_t i64 = read_i64(vm);
            push_i64(i64, vm);
            break;
        }
        case ADD_OPCODE:{
            int64_t right = pop_i64_assert(vm, "Expect integer at right side.");
            int64_t left = pop_i64_assert(vm, "Expect integer at left side.");
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
            int64_t right = pop_i64_assert(vm, "Expect integer at right side.");
            int64_t left = pop_i64_assert(vm, "Expect integer at left side.");
            push_bool(left == right, vm);
            break;
        }
        case NE_OPCODE:{
            int64_t right = pop_i64_assert(vm, "Expect integer at right side.");
            int64_t left = pop_i64_assert(vm, "Expect integer at left side.");
            push_bool(left != right, vm);
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
        case PRT_OPCODE:{
            Value *value = pop(vm);
            print_value(value);
            break;
        }
        case POP_OPCODE:{
            pop(vm);
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

int vm_execute(DynArr *constants, DynArr *chunks, VM *vm){
    if(setjmp(err_jmp) == 1) return 1;
    else{
        vm->ip = 0;
        vm->temps_ptr = 0;
        vm->constants = constants;
        vm->chunks = chunks;

        while (!is_at_end(vm)){
            uint8_t chunk = advance(vm);
            execute(chunk, vm);
        }

        return 0;
    }
}