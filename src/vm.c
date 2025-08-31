#include "vm.h"
#include "vmu.h"
#include "opcode.h"
#include "memory.h"
#include "types.h"
#include "value.h"
#include "utils.h"
#include "tutils.h"

#include "native_module/native_module_str.h"
#include "native_module/native_module_array.h"
#include "native_module/native_module_list.h"
#include "native_module/native_module_dict.h"

#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <setjmp.h>

void *vm_alloc(size_t size, void * ctx){
    VM *vm = (VM *)ctx;
    Allocator *allocator = vm->allocator;
    void *real_ctx = allocator->ctx;
    size_t new_allocated_bytes = vm->allocated_bytes + size;

    if(new_allocated_bytes >= vm->allocation_limit_size){
        size_t bytes_before = vm->allocated_bytes;

        vmu_gc(vm);

        size_t bytes_after = vm->allocated_bytes;
        size_t freeded_bytes = bytes_after - bytes_before;

        if(freeded_bytes < size){
            vm->allocation_limit_size *= 2;
        }
    }

    void *ptr = allocator->alloc(size, real_ctx);

    if(!ptr){
        vmu_error(vm, "Failed to allocated %zu bytes: out of memory", size);
    }

    vm->allocated_bytes = new_allocated_bytes;

    return ptr;
}

void *vm_realloc(void *ptr, size_t old_size, size_t new_size, void *ctx){
    VM *vm = (VM *)ctx;
    Allocator *allocator = vm->allocator;
    void *real_ctx = allocator->ctx;
    ssize_t size = (ssize_t)new_size - (ssize_t)old_size;
    size_t new_allocated_bytes = vm->allocated_bytes + size;

    if(new_allocated_bytes > vm->allocation_limit_size){
        size_t bytes_before = vm->allocated_bytes;

        vmu_gc(vm);

        size_t bytes_after = vm->allocated_bytes;
        size_t freeded_bytes = bytes_after - bytes_before;

        if(freeded_bytes < (size_t)size){
            vm->allocation_limit_size *= 2;
        }
    }else if(new_allocated_bytes < vm->allocation_limit_size / 2){
        vm->allocation_limit_size /= 2;
    }

    void *new_ptr = allocator->realloc(ptr, old_size, new_size, real_ctx);

    if(!new_ptr){
        vmu_error(vm, "Failed to allocated %zu bytes: out of memory", new_size - old_size);
    }

    vm->allocated_bytes = new_allocated_bytes;

    return new_ptr;
}

void vm_dealloc(void *ptr, size_t size, void *ctx){
    VM *vm = (VM *)ctx;
    Allocator *allocator = vm->allocator;
    void *real_ctx = allocator->ctx;
    size_t new_allocated_bytes = vm->allocated_bytes - size;

    if(new_allocated_bytes < vm->allocation_limit_size / 2){
        vm->allocation_limit_size /= 2;
    }

    vm->allocated_bytes = new_allocated_bytes;

    allocator->dealloc(ptr, size, real_ctx);
}

//> PRIVATE INTERFACE
// UTILS FUNCTIONS
static int16_t compose_i16(uint8_t *bytes);
static int32_t compose_i32(uint8_t *bytes);
static inline int16_t read_i16(VM *vm);
static inline int32_t read_i32(VM *vm);
static inline int64_t read_i64_const(VM *vm);
static double read_float_const(VM *vm);
static inline char *read_str(VM *vm, size_t *out_len);
static inline void *get_symbol(size_t index, SubModuleSymbolType type, Module *module, VM *vm);
//----------     STACK RELATED FUNCTIONS     ----------//
static inline Value *peek(VM *vm);
static inline Value *peek_at(uint16_t offset, VM *vm);
static inline void push(Value value, VM *vm);
#define PUSH_EMPTY(_vm) (push(EMPTY_VALUE, (_vm)))
#define PUSH_BOOL(_value, _vm) (push(BOOL_VALUE(_value), (_vm)))
#define PUSH_INT(_value, _vm) (push(INT_VALUE(_value), (_vm)))
#define PUSH_FLOAT(_value, _vm) (push(FLOAT_VALUE(_value), (_vm)))
#define PUSH_OBJ(_obj, _vm)(push(OBJ_VALUE((Obj *)(_obj)), (_vm)))
static inline FnObj *push_fn(Fn *fn, VM *vm);
static ClosureObj *init_closure(MetaClosure *meta, VM *vm);
static Obj *push_native_module(NativeModule *native_module, VM *vm);
static Obj *push_module(Module *module, VM *vm);
static Value *pop(VM *vm);
#define IS_AT_END(vm)((vm)->frame_ptr == 0 || CURRENT_FRAME(vm)->ip >= CURRENT_CHUNKS(vm)->used)
#define ADVANCE(vm)(DYNARR_GET_AS(uint8_t, CURRENT_FRAME(vm)->ip++, CURRENT_CHUNKS(vm)))
//----------     FRAMES RELATED FUNCTIONS     ----------//
static inline Frame *current_frame(VM *vm);
#define VM_CURRENT_FN(_vm)(current_frame(vm)->fn)
#define VM_CURRENT_ICONSTS(_vm)(VM_CURRENT_FN(_vm)->iconsts)
#define VM_CURRENT_FCONSTS(_vm)(VM_CURRENT_FN(_vm)->fconsts)
#define VM_CURRENT_MODULE(_vm)(VM_CURRENT_FN(_vm)->module)
#define VM_CURRENT_CLOSURE(_vm)(current_frame(vm)->closure)
static inline uint8_t advance(VM *vm);
static void add_out_value_to_current_frame(OutValue *value, VM *vm);
static void remove_value_from_current_frame(OutValue *value, VM *vm);
static Frame *push_frame(uint8_t argsc, VM *vm);
static inline void pop_frame(VM *vm);
static inline Value *frame_local(uint8_t which, VM *vm);
// OTHERS
static int execute(VM *vm);
//< PRIVATE INTERFACE
//> PRIVATE IMPLEMENTATION
int16_t compose_i16(uint8_t *bytes){
    return (int16_t)((uint16_t)bytes[1] << 8) | ((uint16_t)bytes[0]);
}

int32_t compose_i32(uint8_t *bytes){
    return (int32_t)((uint32_t)bytes[3] << 24) | ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[0]);
}

static inline int16_t read_i16(VM *vm){
    uint8_t bytes[] = {
        advance(vm),
        advance(vm)
    };

	return compose_i16(bytes);
}

static inline int32_t read_i32(VM *vm){
	uint8_t bytes[4] = {
        advance(vm),
        advance(vm),
        advance(vm),
        advance(vm)
    };

	return compose_i32(bytes);
}

int64_t read_i64_const(VM *vm){
    DynArr *constants = VM_CURRENT_ICONSTS(vm);
    int16_t idx = read_i16(vm);
    return DYNARR_GET_AS(int64_t, (size_t)idx, constants);
}

double read_float_const(VM *vm){
	DynArr *float_values = VM_CURRENT_FCONSTS(vm);
	int16_t idx = read_i16(vm);
	return DYNARR_GET_AS(double, (size_t)idx, float_values);
}

static char *read_str(VM *vm, size_t *out_len){
    DynArr *static_strs = MODULE_STRINGS(VM_CURRENT_MODULE(vm));
    size_t idx = (size_t)read_i16(vm);

    if(idx >= DYNARR_LEN(static_strs)){
        vmu_error(vm, "Illegal module static strings access index");
    }

    RawStr raw_str = DYNARR_GET_AS(RawStr, idx, static_strs);

    if(out_len){
        *out_len = raw_str.len;
    }

    return raw_str.buff;
}

static inline void *get_symbol(size_t index, SubModuleSymbolType type, Module *module, VM *vm){
    DynArr *symbols = MODULE_SYMBOLS(module);

    if(index >= DYNARR_LEN(symbols)){
        vmu_error(vm, "Failed to get module symbol: index out of bounds");
    }

    SubModuleSymbol *symbol = &DYNARR_GET_AS(SubModuleSymbol, index, symbols);

    if(symbol->type != type){
        vmu_error(vm, "Failed to get module symbol: mismatch types");
    }

    return symbol->value;
}

static inline Value *peek(VM *vm){
    if(vm->stack_top == vm->stack){
        vmu_error(vm, "Stack is empty");
    }

    return vm->stack_top - 1;
}

static inline Value *peek_at(uint16_t offset, VM *vm){
    if(vm->stack == vm->stack_top){
        vmu_internal_error(vm, "Stack is empty");
    }

    if(vm->stack_top - 1 - offset <= vm->stack){
        vmu_internal_error(vm, "Illegal stack peek offset");
    }

    return vm->stack_top - 1 - offset;
}

static inline void push(Value value, VM *vm){
    if(vm->stack_top >= vm->stack + STACK_LENGTH){
        vmu_error(vm, "Stack over flow");
    }

    *(vm->stack_top++) = value;
}

static inline FnObj *push_fn(Fn *fn, VM *vm){
    FnObj *fn_obj = vmu_create_fn(fn, vm);
    PUSH_OBJ(fn_obj, vm);
    return fn_obj;
}

ClosureObj *init_closure(MetaClosure *meta, VM *vm){
    ClosureObj *closure_obj = vmu_create_closure(meta, vm);
    Closure *closure = closure_obj->closure;
    OutValue *out_values = closure->out_values;
    MetaOutValue *meta_out_values = meta->meta_out_values;
    size_t out_values_len = meta->meta_out_values_len;

    for (size_t i = 0; i < out_values_len; i++){
        OutValue *out_value = &out_values[i];
        MetaOutValue *meta_out_value = &meta_out_values[i];

        out_value->linked = 1;
        out_value->at = meta_out_value->at;
        out_value->value = *frame_local(meta_out_value->at, vm);
        out_value->prev = NULL;
        out_value->next = NULL;

        add_out_value_to_current_frame(out_value, vm);
    }

    return closure_obj;
}

Obj *push_native_module(NativeModule *native_module, VM *vm){
    return NULL;
}

Obj *push_module(Module *module, VM *vm){
    return NULL;
}

Value *pop(VM *vm){
    if(vm->stack_top == vm->stack){
        vmu_error(vm, "Stack under flow");
    }

    Value *value = --vm->stack_top;

    return value;
}

static inline Frame *current_frame(VM *vm){
    if(vm->frame_ptr == vm->frame_stack){
        vmu_error(vm, "Frame stack is empty");
    }

    return vm->frame_ptr - 1;
}

static inline uint8_t advance(VM *vm){
    Frame *frame = current_frame(vm);
    DynArr *chunks = frame->fn->chunks;

    if(frame->ip >= DYNARR_LEN(chunks)){
        vmu_error(vm, "IP excceded chunks length");
    }

    return DYNARR_GET_AS(uint8_t, frame->ip++, chunks);
}

void add_out_value_to_current_frame(OutValue *value, VM *vm){
    Frame *frame = current_frame(vm);

    if(frame->outs_tail){
        frame->outs_tail->next = value;
        value->prev = frame->outs_tail;
    }else{
        frame->outs_head = value;
    }

    frame->outs_tail = value;
}

void remove_value_from_current_frame(OutValue *value, VM *vm){
    Frame *frame = current_frame(vm);

    if(value == frame->outs_head){
        frame->outs_head = value->next;
    }
    if(value == frame->outs_tail){
        frame->outs_tail = value->prev;
    }

    if(value->prev){
        value->prev->next = value->next;
    }
    if(value->next){
        value->next->prev = value->prev;
    }
}

static Frame *push_frame(uint8_t argsc, VM *vm){
    if(vm->frame_ptr >= vm->frame_stack + FRAME_LENGTH){
        vmu_error(vm, "Frame stack is full");
    }

    // frame's locals pointer must always point to the frame's function:
    //     frame->locals == frame->fn
    Value *locals = vm->stack_top - 1 - argsc;

    if(locals < vm->stack){
        vmu_error(vm, "Frame locals out of value stack");
    }

    Frame *frame = vm->frame_ptr++;

    frame->ip = 0;
    frame->last_offset = 0;
    frame->fn = NULL;
    frame->closure = NULL;
    frame->locals = locals;
    frame->outs_head = NULL;
    frame->outs_tail = NULL;

    return frame;
}

static inline void pop_frame(VM *vm){
    if(vm->frame_ptr == vm->frame_stack){
        vmu_error(vm, "Frame stack is empty");
    }

    vm->frame_ptr--;
}

static inline Value *frame_local(uint8_t which, VM *vm){
    Frame *frame = current_frame(vm);
    Value *locals = frame->locals;
    Value *local = locals + 1 + which;

    if(local >= vm->stack_top){
        vmu_error(vm, "Index for frame local pass value stack top");
    }

    return local;
}

static int execute(VM *vm){
    for (;;){
        uint8_t chunk = advance(vm);
        Frame *frame = current_frame(vm);
        frame->last_offset = frame->ip - 1;

        switch (chunk){
            case EMPTY_OPCODE:{
                PUSH_EMPTY(vm);
                break;
            }case FALSE_OPCODE:{
                PUSH_BOOL(0, vm);
                break;
            }case TRUE_OPCODE:{
                PUSH_BOOL(1, vm);
                break;
            }case CINT_OPCODE:{
                int64_t i64 = (int64_t)advance(vm);
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
                size_t len;
                StrObj *str_obj = NULL;
                char *str = read_str(vm, &len);

                vmu_create_str(0, len, str, vm, &str_obj);
                PUSH_OBJ(str_obj, vm);

                break;
            }case TEMPLATE_OPCODE:{
                break;
            }case ARRAY_OPCODE:{
                Value *len_value = pop(vm);

                if(!IS_VALUE_INT(len_value)){
                    vmu_error(vm, "Expect 'INT' as array length");
                }

                int64_t array_len = VALUE_TO_INT(len_value);
                ArrayObj *array_obj = vmu_create_array(array_len, vm);

                PUSH_OBJ(array_obj, vm);

                break;
            }case LIST_OPCODE:{
                ListObj *list_obj = vmu_create_list(vm);
                PUSH_OBJ(list_obj, vm);
                break;
            }case DICT_OPCODE:{
                DictObj *dict_obj = vmu_create_dict(vm);
                PUSH_OBJ(dict_obj, vm);
                break;
            }case RECORD_OPCODE:{
                uint16_t len = (uint16_t)read_i16(vm);
                RecordObj *record_obj = vmu_create_record(len, vm);
                PUSH_OBJ(record_obj, vm);
                break;
            }case IARRAY_OPCODE:{
                int64_t idx = (int64_t)read_i16(vm);
                Value *value = pop(vm);
                Value *array_value = peek(vm);
                ArrayObj *array_obj = VALUE_TO_ARRAY(array_value);

                vmu_array_set_at(idx, *value, array_obj, vm);

                break;
            }case ILIST_OPCODE:{
                Value *value = peek_at(0, vm);
                Value *list_value = peek_at(1, vm);

                if(!IS_VALUE_LIST(list_value)){
                    vmu_internal_error(vm, "Expect value of type 'dict', but got something else");
                }

                vmu_list_insert(*value, VALUE_TO_LIST(list_value), vm);
                pop(vm);

                break;
            }case IDICT_OPCODE:{
                Value *raw_value = peek_at(0, vm);
                Value *key_value = peek_at(1, vm);
                Value *dict_value = peek_at(2, vm);

                if(!IS_VALUE_DICT(dict_value)){
                    vmu_internal_error(vm, "Expect value of type 'dict', but got something else");
                }

                vmu_dict_put(*key_value, *raw_value, VALUE_TO_DICT(dict_value), vm);
                pop(vm);
                pop(vm);

                break;
            }case IRECORD_OPCODE:{
                size_t key_size;
                char *key = read_str(vm, &key_size);
                Value *raw_value = peek_at(0, vm);
                Value *record_value = peek_at(1, vm);

                if(!IS_VALUE_RECORD(record_value)){
                    vmu_internal_error(vm, "Expect value of type 'record', but got something else");
                }

                vmu_record_insert_attr(key_size, key, *raw_value, VALUE_TO_RECORD(record_value), vm);
                pop(vm);

                break;
            }case CONCAT_OPCODE:{
                Value *right_value = peek_at(0, vm);
                Value *left_value = peek_at(1, vm);

                if(IS_VALUE_STR(left_value) && IS_VALUE_STR(right_value)){
                    StrObj *left_str_obj = VALUE_TO_STR(left_value);
                    StrObj *right_str_obj = VALUE_TO_STR(right_value);
                    StrObj *result_str_obj = vmu_str_concat(left_str_obj, right_str_obj, vm);

                    pop(vm);
                    pop(vm);
                    PUSH_OBJ(result_str_obj, vm);

                    break;
                }

                if(IS_VALUE_ARRAY(left_value) && IS_VALUE_ARRAY(right_value)){
                    ArrayObj *left_array_obj = VALUE_TO_ARRAY(left_value);
                    ArrayObj *right_array_obj = VALUE_TO_ARRAY(right_value);
                    ArrayObj *new_array_obj = vmu_array_join(left_array_obj, right_array_obj, vm);

                    pop(vm);
                    pop(vm);
                    PUSH_OBJ(new_array_obj, vm);

                    break;
                }

                if(IS_VALUE_LIST(left_value) && IS_VALUE_LIST(right_value)){
                    ListObj *left_list_obj = VALUE_TO_LIST(left_value);
                    ListObj *right_list_obj = VALUE_TO_LIST(right_value);
                    ListObj *new_list_obj = vmu_list_join(left_list_obj, right_list_obj, vm);

                    pop(vm);
                    pop(vm);
                    PUSH_OBJ(new_list_obj, vm);

                    break;
                }

                if(IS_VALUE_ARRAY(left_value) || IS_VALUE_ARRAY(right_value)){
                    ArrayObj *array_obj = NULL;
                    Value *raw_value = NULL;

                    if(IS_VALUE_ARRAY(left_value)){
                        array_obj = VALUE_TO_ARRAY(left_value);
                        raw_value = right_value;
                    }else{
                        array_obj = VALUE_TO_ARRAY(right_value);
                        raw_value = left_value;
                    }

                    ArrayObj *new_array_obj = vmu_array_join_value(*raw_value, array_obj, vm);

                    pop(vm);
                    pop(vm);
                    PUSH_OBJ(new_array_obj, vm);

                    break;
                }

                if(IS_VALUE_LIST(left_value) || IS_VALUE_LIST(right_value)){
                    ListObj *list_obj = NULL;
                    Value *raw_value = NULL;

                    if(IS_VALUE_LIST(left_value)){
                        list_obj = VALUE_TO_LIST(left_value);
                        raw_value = right_value;
                    }else{
                        list_obj = VALUE_TO_LIST(right_value);
                        raw_value = left_value;
                    }

                    ListObj *new_list_obj = vmu_list_insert_new(*raw_value, list_obj, vm);

                    pop(vm);
                    pop(vm);
                    PUSH_OBJ(new_list_obj, vm);

                    break;
                }

                vmu_error(vm, "Illegal operands for concatenation");

                break;
            }case MULSTR_OPCODE:{
                Value *right_value = peek_at(0, vm);
                Value *left_value = peek_at(1, vm);

                if(IS_VALUE_INT(left_value) && IS_VALUE_STR(right_value)){
                    int64_t by = VALUE_TO_INT(left_value);
                    StrObj *str_obj = VALUE_TO_STR(right_value);
                    StrObj *new_str_obj = vmu_str_mul(by, str_obj, vm);

                    pop(vm);
                    pop(vm);
                    PUSH_OBJ(new_str_obj, vm);

                    break;
                }

                if(IS_VALUE_STR(left_value) && IS_VALUE_INT(right_value)){
                    int64_t by = VALUE_TO_INT(right_value);
                    StrObj *str_obj = VALUE_TO_STR(left_value);
                    StrObj *new_str_obj = vmu_str_mul(by, str_obj, vm);

                    pop(vm);
                    pop(vm);
                    PUSH_OBJ(new_str_obj, vm);

                    break;
                }

                vmu_error(vm, "Illegal operands for string multiplication");

                break;
            }case ADD_OPCODE:{
                Value *right_value = pop(vm);
                Value *left_value = pop(vm);

                if(IS_VALUE_INT(left_value) && IS_VALUE_INT(right_value)){
                    int64_t left = VALUE_TO_INT(left_value);
                    int64_t right = VALUE_TO_INT(right_value);

                    push(INT_VALUE(left + right), vm);

                    break;
                }

                if((IS_VALUE_INT(left_value) || IS_VALUE_FLOAT(left_value)) && (IS_VALUE_INT(right_value) || IS_VALUE_FLOAT(right_value))){
                    double left;
                    double right;

                    left = VALUE_TO_FLOAT(left_value);

                    if(IS_VALUE_INT(left_value)){
                        left = (double)VALUE_TO_INT(left_value);
                    }

                    right = VALUE_TO_FLOAT(right_value);

                    if(IS_VALUE_INT(right_value)){
                        right = (double)VALUE_TO_INT(right_value);
                    }

                    push(FLOAT_VALUE(left + right), vm);

                    break;
                }

                vmu_error(vm, "Unsuported types using + operator");

                break;
            }case SUB_OPCODE:{
                Value *right_value = pop(vm);
                Value *left_value = pop(vm);

                if(IS_VALUE_INT(left_value) && IS_VALUE_INT(right_value)){
                    int64_t left = VALUE_TO_INT(left_value);
                    int64_t right = VALUE_TO_INT(right_value);

                    push(INT_VALUE(left - right), vm);

                    break;
                }

                if((IS_VALUE_INT(left_value) || IS_VALUE_FLOAT(left_value)) && (IS_VALUE_INT(right_value) || IS_VALUE_FLOAT(right_value))){
                    double left;
                    double right;

                    left = VALUE_TO_FLOAT(left_value);

                    if(IS_VALUE_INT(left_value)){
                        left = (double)VALUE_TO_INT(left_value);
                    }

                    right = VALUE_TO_FLOAT(right_value);

                    if(IS_VALUE_INT(right_value)){
                        right = (double)VALUE_TO_INT(right_value);
                    }

                    push(FLOAT_VALUE(left - right), vm);

                    break;
                }

                vmu_error(vm, "Unsuported types using - operator");

                break;
            }case MUL_OPCODE:{
                Value *right_value = peek_at(0, vm);
                Value *left_value = peek_at(1, vm);

                if(IS_VALUE_INT(left_value) && IS_VALUE_INT(right_value)){
                    int64_t left = VALUE_TO_INT(left_value);
                    int64_t right = VALUE_TO_INT(right_value);

                    pop(vm);
                    pop(vm);
                    push(INT_VALUE(left * right), vm);

                    break;
                }

                if((IS_VALUE_INT(left_value) || IS_VALUE_FLOAT(left_value)) && (IS_VALUE_INT(right_value) || IS_VALUE_FLOAT(right_value))){
                    double left;
                    double right;

                    left = VALUE_TO_FLOAT(left_value);

                    if(IS_VALUE_INT(left_value)){
                        left = (double)VALUE_TO_INT(left_value);
                    }

                    right = VALUE_TO_FLOAT(right_value);

                    if(IS_VALUE_INT(right_value)){
                        right = (double)VALUE_TO_INT(right_value);
                    }

                    pop(vm);
                    pop(vm);
                    push(FLOAT_VALUE(left * right), vm);

                    break;
                }

                vmu_error(vm, "Unsuported types using * operator");

                break;
            }case DIV_OPCODE:{
                Value *right_value = pop(vm);
                Value *left_value = pop(vm);

                if(IS_VALUE_INT(left_value) && IS_VALUE_INT(right_value)){
                    int64_t left = VALUE_TO_INT(left_value);
                    int64_t right = VALUE_TO_INT(right_value);

                    push(INT_VALUE(left / right), vm);

                    break;
                }

                if((IS_VALUE_INT(left_value) || IS_VALUE_FLOAT(left_value)) && (IS_VALUE_INT(right_value) || IS_VALUE_FLOAT(right_value))){
                    double left;
                    double right;

                    left = VALUE_TO_FLOAT(left_value);

                    if(IS_VALUE_INT(left_value)){
                        left = (double)VALUE_TO_INT(left_value);
                    }

                    right = VALUE_TO_FLOAT(right_value);

                    if(IS_VALUE_INT(right_value)){
                        right = (double)VALUE_TO_INT(right_value);
                    }

                    push(FLOAT_VALUE(left / right), vm);

                    break;
                }

                vmu_error(vm, "Unsuported types using / operator");

                break;
            }case MOD_OPCODE:{
                Value *right_value = pop(vm);
                Value *left_value = pop(vm);

                if(IS_VALUE_INT(left_value) && IS_VALUE_INT(right_value)){
                    int64_t left = VALUE_TO_INT(left_value);
                    int64_t right = VALUE_TO_INT(right_value);

                    push(INT_VALUE(left % right), vm);

                    break;
                }

                vmu_error(vm, "Unsuported types using 'mod' operator");

                break;
            }case BNOT_OPCODE:{
                Value *value = pop(vm);

                if(IS_VALUE_INT(value)){
                    PUSH_INT(~VALUE_TO_INT(value), vm);
                    break;
                }

                vmu_error(vm, "Unsuported types using '~' operator");

                break;
            }case LSH_OPCODE:{
                Value *vb = pop(vm);
                Value *va = pop(vm);

                if(IS_VALUE_INT(va) && IS_VALUE_INT(vb)){
                    int64_t left = VALUE_TO_INT(va);
                    int64_t right = VALUE_TO_INT(vb);

                    PUSH_INT(left << right, vm);

                    break;
                }

                vmu_error(vm, "Unsuported types using '<<' operator");

                break;
            }case RSH_OPCODE:{
                Value *vb = pop(vm);
                Value *va = pop(vm);

                if(IS_VALUE_INT(va) && IS_VALUE_INT(vb)){
                    int64_t left = VALUE_TO_INT(va);
                    int64_t right = VALUE_TO_INT(vb);

                    PUSH_INT(left >> right, vm);

                    break;
                }

                vmu_error(vm, "Unsuported types using '>>' operator");

                break;
            }case BAND_OPCODE:{
                Value *vb = pop(vm);
                Value *va = pop(vm);

                if(IS_VALUE_INT(va) && IS_VALUE_INT(vb)){
                    int64_t left = VALUE_TO_INT(va);
                    int64_t right = VALUE_TO_INT(vb);

                    PUSH_INT(left & right, vm);

                    break;
                }

                vmu_error(vm, "Unsuported types using '&' operator");

                break;
            }case BXOR_OPCODE:{
                Value *vb = pop(vm);
                Value *va = pop(vm);

                if(IS_VALUE_INT(va) && IS_VALUE_INT(vb)){
                    int64_t left = VALUE_TO_INT(va);
                    int64_t right = VALUE_TO_INT(vb);

                    PUSH_INT(left ^ right, vm);

                    break;
                }

                vmu_error(vm, "Unsuported types using '^' operator");

                break;
            }case BOR_OPCODE:{
                Value *vb = pop(vm);
                Value *va = pop(vm);

                if(IS_VALUE_INT(va) && IS_VALUE_INT(vb)){
                    int64_t left = VALUE_TO_INT(va);
                    int64_t right = VALUE_TO_INT(vb);

                    PUSH_INT(left | right, vm);

                    break;
                }

                vmu_error(vm, "Unsuported types using '|' operator");

                break;
            }case LT_OPCODE:{
                Value *vb = pop(vm);
                Value *va = pop(vm);

                if(IS_VALUE_INT(va) && IS_VALUE_INT(vb)){
                    int64_t left = VALUE_TO_INT(va);
                    int64_t right = VALUE_TO_INT(vb);

                    PUSH_BOOL(left < right, vm);

                    break;
                }

                if(IS_VALUE_FLOAT(va) && IS_VALUE_FLOAT(vb)){
                    double left = VALUE_TO_FLOAT(va);
                    double right = VALUE_TO_FLOAT(vb);

                    PUSH_BOOL(left < right, vm);

                    break;
                }

                vmu_error(vm, "Unsuported types using < operator");

                break;
            }case GT_OPCODE:{
                Value *vb = pop(vm);
                Value *va = pop(vm);

                if(IS_VALUE_INT(va) && IS_VALUE_INT(vb)){
                    int64_t left = VALUE_TO_INT(va);
                    int64_t right = VALUE_TO_INT(vb);

                    PUSH_BOOL(left > right, vm);

                    break;
                }

                if(IS_VALUE_FLOAT(va) && IS_VALUE_FLOAT(vb)){
                    double left = VALUE_TO_FLOAT(va);
                    double right = VALUE_TO_FLOAT(vb);

                    PUSH_BOOL(left > right, vm);

                    break;
                }

                vmu_error(vm, "Unsuported types using > operator");

                break;
            }case LE_OPCODE:{
                Value *vb = pop(vm);
                Value *va = pop(vm);

                if(IS_VALUE_INT(va) && IS_VALUE_INT(vb)){
                    int64_t left = VALUE_TO_INT(va);
                    int64_t right = VALUE_TO_INT(vb);

                    PUSH_BOOL(left <= right, vm);

                    break;
                }

                if(IS_VALUE_FLOAT(va) && IS_VALUE_FLOAT(vb)){
                    double left = VALUE_TO_FLOAT(va);
                    double right = VALUE_TO_FLOAT(vb);

                    PUSH_BOOL(left <= right, vm);

                    break;
                }

                vmu_error(vm, "Unsuported types using <= operator");

                break;
            }case GE_OPCODE:{
                Value *vb = pop(vm);
                Value *va = pop(vm);

                if(IS_VALUE_INT(va) && IS_VALUE_INT(vb)){
                    int64_t left = VALUE_TO_INT(va);
                    int64_t right = VALUE_TO_INT(vb);

                    PUSH_BOOL(left >= right, vm);

                    break;
                }

                if(IS_VALUE_FLOAT(va) && IS_VALUE_FLOAT(vb)){
                    double left = VALUE_TO_FLOAT(va);
                    double right = VALUE_TO_FLOAT(vb);

                    PUSH_BOOL(left >= right, vm);

                    break;
                }

                vmu_error(vm, "Unsuported types using >= operator");

                break;
            }case EQ_OPCODE:{
                Value *vb = pop(vm);
                Value *va = pop(vm);

                if(IS_VALUE_BOOL(va) && IS_VALUE_BOOL(vb)){
                    uint8_t left = VALUE_TO_BOOL(va);
                    uint8_t right = VALUE_TO_BOOL(vb);

                    PUSH_BOOL(left == right, vm);

                    break;
                }

                if(IS_VALUE_INT(va) && IS_VALUE_INT(vb)){
                    int64_t left = VALUE_TO_INT(va);
                    int64_t right = VALUE_TO_INT(vb);

                    PUSH_BOOL(left == right, vm);

                    break;
                }

                if(IS_VALUE_FLOAT(va) && IS_VALUE_FLOAT(vb)){
                    double left = VALUE_TO_FLOAT(va);
                    double right = VALUE_TO_FLOAT(vb);

                    PUSH_BOOL(left == right, vm);

                    break;
                }

                if(IS_VALUE_STR(va) && IS_VALUE_STR(vb)){
                    StrObj *left = VALUE_TO_STR(va);
                    StrObj *right = VALUE_TO_STR(vb);

                    PUSH_BOOL(left == right, vm);

                    break;
                }

                vmu_error(vm, "Unsuported types using == operator");

                break;
            }case NE_OPCODE:{
                Value *right_value = pop(vm);
                Value *left_value = pop(vm);

                if(IS_VALUE_BOOL(left_value) && IS_VALUE_BOOL(right_value)){
                    uint8_t left = VALUE_TO_BOOL(left_value);
                    uint8_t right = VALUE_TO_BOOL(right_value);

                    PUSH_BOOL(left != right, vm);

                    break;
                }

                if(IS_VALUE_INT(left_value) && IS_VALUE_INT(right_value)){
                    int64_t left = VALUE_TO_INT(left_value);
                    int64_t right = VALUE_TO_INT(right_value);

                    PUSH_BOOL(left != right, vm);

                    break;
                }

                if(IS_VALUE_FLOAT(left_value) && IS_VALUE_FLOAT(right_value)){
                    double left = VALUE_TO_FLOAT(left_value);
                    double right = VALUE_TO_FLOAT(right_value);

                    PUSH_BOOL(left != right, vm);

                    break;
                }

                vmu_error(vm, "Unsuported types using != operator");

                break;
            }case OR_OPCODE:{
                Value *right_value = pop(vm);
                Value *left_value = pop(vm);

                if(IS_VALUE_BOOL(left_value) && IS_VALUE_BOOL(right_value)){
                    uint8_t left = VALUE_TO_BOOL(left_value);
                    uint8_t right = VALUE_TO_BOOL(right_value);

                    PUSH_BOOL(left || right, vm);

                    break;
                }

                vmu_error(vm, "Unsupported types using 'or' operator");

                break;
            }case AND_OPCODE:{
                Value *right_value = pop(vm);
                Value *left_value = pop(vm);

                if(IS_VALUE_BOOL(left_value) && IS_VALUE_BOOL(right_value)){
                    uint8_t left = VALUE_TO_BOOL(left_value);
                    uint8_t right = VALUE_TO_BOOL(right_value);

                    PUSH_BOOL(left && right, vm);

                    break;
                }

                vmu_error(vm, "Unsupported types using 'and' operator");

                break;
            }case NOT_OPCODE:{
                Value *value = pop(vm);

                if(!IS_VALUE_BOOL(value)){
                    vmu_error(vm, "Expect boolean at right side");
                }

                PUSH_BOOL(!VALUE_TO_BOOL(value), vm);

                break;
            }case NNOT_OPCODE:{
                Value *value = pop(vm);

                if(IS_VALUE_INT(value)){
                    PUSH_INT(-VALUE_TO_INT(value), vm);
                    break;
                }

                if(IS_VALUE_FLOAT(value)){
                    PUSH_FLOAT(-VALUE_TO_FLOAT(value), vm);
                    break;
                }

                vmu_error(vm, "Expect integer or float at right side");

                break;
            }case LSET_OPCODE:{
                Value value = *peek(vm);
                uint8_t index = advance(vm);
                *frame_local(index, vm) = value;
                break;
            }case LGET_OPCODE:{
                uint8_t index = advance(vm);
                Value value = *frame_local(index, vm);
                push(value, vm);
                break;
            }case OSET_OPCODE:{
                uint8_t index = advance(vm);
                Value *value = peek(vm);
                Closure *closure = VM_CURRENT_CLOSURE(vm);
                OutValue *out_values = closure->out_values;
                MetaClosure *meta = closure->meta;
                size_t meta_out_values_len = meta->meta_out_values_len;

                for (size_t i = 0; i < meta_out_values_len; i++){
                    OutValue *closure_value = &out_values[i];

                    if(closure_value->at == index){
                        closure_value->value = *value;
                        break;
                    }
                }

                pop(vm);

                break;
            }case OGET_OPCODE:{
                uint8_t index = advance(vm);
                Closure *closure = VM_CURRENT_CLOSURE(vm);
                OutValue *out_values = closure->out_values;
                MetaClosure *meta = closure->meta;
                size_t meta_out_values_len = meta->meta_out_values_len;

                for (size_t i = 0; i < meta_out_values_len; i++){
                    OutValue *out_value = &out_values[i];

                    if(out_value->at == index){
                        push(out_value->value, vm);
                        break;
                    }
                }

                break;
            }case GDEF_OPCODE:{
                size_t key_size;
                char *key = read_str(vm, &key_size);
                Value *value = pop(vm);
                LZOHTable *globals = MODULE_GLOBALS(VM_CURRENT_FN(vm)->module);

                if(lzohtable_lookup(key, key_size, globals, NULL)){
                    vmu_error(vm, "Cannot define global '%s': already exists", key);
                }

                GlobalValue global_value = {0};

                global_value.access = PRIVATE_GLOVAL_VALUE_TYPE;
                global_value.value = *value;

                lzohtable_put_ckv(key, key_size, &global_value, sizeof(GlobalValue), globals, NULL);

                break;
            }case GASET_OPCODE:{
                Module *module = VM_CURRENT_MODULE(vm);
                LZOHTable *globals = MODULE_GLOBALS(module);

                char *key = read_str(vm, NULL);
                size_t key_size = strlen(key);

                GlobalValue *global_value = NULL;

                if(!lzohtable_lookup(key, key_size, globals, (void **)(&global_value))){
                    vmu_error(vm, "Global symbol '%s' does not exists", key);
                    break;
                }

                Value *value = &global_value->value;
                uint8_t access_type = advance(vm);

                if(IS_VALUE_NATIVE_MODULE(value) || IS_VALUE_MODULE(value)){
                    vmu_error(vm, "Modules cannot modify its access");
                }

                if(access_type == 0){
                    global_value->access = PRIVATE_GLOVAL_VALUE_TYPE;
                }else if(access_type == 1){
                    global_value->access = PUBLIC_GLOBAL_VALUE_TYPE;
                }else{
                    vmu_error(vm, "Illegal access type: %d", access_type);
                }

                break;
            }case GSET_OPCODE:{
                size_t key_size;
                char *key = read_str(vm, &key_size);
                Value *value = peek(vm);
                GlobalValue *global_value = NULL;
                LZOHTable *globals = MODULE_GLOBALS(VM_CURRENT_FN(vm)->module);

                if(lzohtable_lookup(key, key_size, globals, (void **)(&global_value))){
                    global_value->value = *value;
                    break;
                }

                vmu_error(vm, "Global '%s' does not exists", key);

                break;
            }case GGET_OPCODE:{
                size_t key_size;
                char *key = read_str(vm, &key_size);
                GlobalValue *global_value = NULL;
                LZOHTable *globals = MODULE_GLOBALS(VM_CURRENT_FN(vm)->module);

                if(!lzohtable_lookup(key, key_size, globals, (void **)(&global_value))){
                    vmu_error(vm, "Global symbol '%s' does not exists", key);
                }

                push(global_value->value, vm);

                break;
            }case NGET_OPCODE:{
                size_t key_size;
                char *key = read_str(vm, &key_size);
                NativeFn *native_fn = NULL;

                if(lzohtable_lookup(key, key_size, vm->native_fns, (void **)(&native_fn))){
                    NativeFnObj *native_fn_obj = vmu_create_native_fn((Value){0}, native_fn, vm);
                    PUSH_OBJ(native_fn_obj, vm);
                    break;
                }

                vmu_internal_error(vm, "Unknown native symbol '%s'", key);

                break;
            }case SGET_OPCODE:{
                size_t index = (size_t)read_i32(vm);
                Module *module = VM_CURRENT_MODULE(vm);
                DynArr *symbols = MODULE_SYMBOLS(module);
                size_t symbols_len = DYNARR_LEN(symbols);

                if(index >= symbols_len){
                    vmu_error(vm, "Failed to get module symbol: index (%zu) out of bounds", index);
                }

                SubModuleSymbol symbol = DYNARR_GET_AS(SubModuleSymbol, index, symbols);

                switch (symbol.type){
                    case FUNCTION_SUBMODULE_SYM_TYPE:{
                        Fn *fn = (Fn *)symbol.value;
                        FnObj *fn_obj = vmu_create_fn(fn, vm);

                        PUSH_OBJ(fn_obj, vm);

                        break;
                    }case CLOSURE_SUBMODULE_SYM_TYPE:{
                        MetaClosure *meta_closure = (MetaClosure *)symbol.value;
                        ClosureObj *closure_obj = init_closure(meta_closure, vm);

                        PUSH_OBJ(closure_obj, vm);

                        break;
                    }case NATIVE_MODULE_SUBMODULE_SYM_TYPE:{
                        NativeModule *native_module = (NativeModule *)symbol.value;
                        NativeModuleObj *native_module_obj = vmu_create_native_module(native_module, vm);

                        PUSH_OBJ(native_module_obj, vm);

                        break;
                    }default:{
                        vmu_internal_error(vm, "Unknown submodule symbol type");
                    }
                }

                break;
            }case ASET_OPCODE:{
                Value *indexable_value = peek_at(0, vm);
                Value *idx_value = peek_at(1, vm);
                Value *value = peek_at(2, vm);

                if(!IS_VALUE_OBJ(indexable_value)){
                    vmu_error(vm, "Illegal assignment target");
                }

                Obj *obj = VALUE_TO_OBJ(indexable_value);

                switch (obj->type){
                    case ARRAY_OBJ_TYPE:{
                        if(!IS_VALUE_INT(idx_value)){
                            vmu_error(vm, "Expect index value of type 'int'");
                        }

                        int64_t idx = VALUE_TO_INT(idx_value);
                        ArrayObj *array_obj = VALUE_TO_ARRAY(indexable_value);
                        vmu_array_set_at(idx, *value, array_obj, vm);

                        break;
                    }case LIST_OBJ_TYPE:{
                        if(!IS_VALUE_INT(idx_value)){
                            vmu_error(vm, "Expect index value of type 'int'");
                        }

                        int64_t idx = VALUE_TO_INT(idx_value);
                        ListObj *list_obj = VALUE_TO_LIST(indexable_value);
                        vmu_list_set_at(idx, *value, list_obj, vm);

                        break;
                    }case DICT_OBJ_TYPE:{
                        DictObj *dict_obj = VALUE_TO_DICT(indexable_value);
                        vmu_dict_put(*idx_value, *value, dict_obj, vm);

                        break;
                    }default:{
                        vmu_error(vm, "Illegal assignment target");
                    }
                }

                pop(vm);
                pop(vm);

                break;
            }case PUT_OPCODE:{
                size_t key_size;
                char *key = read_str(vm, &key_size);
                Value *target_value = pop(vm);
                Value *raw_value = peek(vm);

                if(!IS_VALUE_RECORD(target_value)){
                    vmu_error(vm, "Expect record in assignment");
                }

                RecordObj *record_obj = VALUE_TO_RECORD(target_value);

                vmu_record_set_attr(key_size, key, *raw_value, record_obj, vm);

                break;
            }case POP_OPCODE:{
                pop(vm);
                break;
            }case JMP_OPCODE:{
                int16_t jmp_value = read_i16(vm);
                if(jmp_value == 0){break;}

                if(jmp_value > 0){
                    current_frame(vm)->ip += jmp_value - 1;
                }else{
                    current_frame(vm)->ip += jmp_value - 3;
                }

                break;
            }case JIF_OPCODE:{
                Value *value = pop(vm);

                if(!IS_VALUE_BOOL(value)){
                    vmu_error(vm, "Expect boolean as conditional value");
                }

                uint8_t condition = VALUE_TO_BOOL(value);
                int16_t jmp_value = read_i16(vm);

                if(jmp_value == 0){break;}

                if(!condition){
                    if(jmp_value > 0){
                        current_frame(vm)->ip += jmp_value - 1;
                    }else{
                        current_frame(vm)->ip += jmp_value - 3;
                    }
                }

                break;
            }case JIT_OPCODE:{
                Value *value = pop(vm);

                if(!IS_VALUE_BOOL(value)){
                    vmu_error(vm, "Expect boolean as conditional value");
                }

                uint8_t condition = VALUE_TO_BOOL(value);
                int16_t jmp_value = read_i16(vm);

                if(jmp_value == 0){break;}

                if(condition){
                    if(jmp_value > 0){
                        current_frame(vm)->ip += jmp_value - 1;
                    }else{
                        current_frame(vm)->ip += jmp_value - 3;
                    }
                }

                break;
            }case CALL_OPCODE:{
                uint8_t args_count = advance(vm);
                Value *callable_value = peek_at(args_count, vm);

                if(IS_VALUE_FN(callable_value)){
                    FnObj *fn_obj = VALUE_TO_FN(callable_value);
                    Fn *fn = fn_obj->fn;
                    uint8_t params_count = fn->params ? DYNARR_LEN(fn->params) : 0;

                    if(params_count != args_count){
                        vmu_error(vm, "Failed to call function '%s'. Declared with %d parameter(s), but got %d argument(s)", fn->name, params_count, args_count);
                    }

                    Frame *frame = push_frame(args_count, vm);

                    frame->fn = fn;

                    break;
                }

                if(IS_VALUE_CLOSURE(callable_value)){
                    ClosureObj *closure_obj = VALUE_TO_CLOSURE(callable_value);
                    Closure *closure = closure_obj->closure;
                    Fn *fn = closure->meta->fn;
                    uint8_t params_count = fn->params ? DYNARR_LEN(fn->params) : 0;

                    if(params_count != args_count){
                        vmu_error(vm, "Failed to call function '%s'. Declared with %d parameter(s), but got %d argument(s)", fn->name, params_count, args_count);
                    }

                    Frame *frame = push_frame(args_count, vm);

                    frame->fn = fn;
                    frame->closure = closure;

                    break;
                }

                if(IS_VALUE_NATIVE_FN(callable_value)){
                    NativeFnObj *native_fn_obj = VALUE_TO_NATIVE_FN(callable_value);
                    NativeFn *native_fn = native_fn_obj->native_fn;
                    Value *target = &native_fn_obj->target;
                    RawNativeFn raw_fn = native_fn->raw_fn;

                    if(native_fn->arity != args_count){
                        vmu_error(
                            vm,
                            "Failed to call function '%s'.\n\tDeclared with %d parameter(s), but got %d argument(s)",
                            native_fn->name,
                            native_fn->arity,
                            args_count
                        );
                    }

                    // VLA must not be declared with a zero length
                    Value args[args_count == 0 ? 1 : args_count];

                    for (int i = args_count; i > 0; i--){
                        args[i - 1] = *peek_at(args_count - i, vm);
                    }

                    Value out_value = raw_fn(args_count, args, target, vm);

                    vm->stack_top = callable_value;

                    push(out_value, vm);

                    break;
                }

                vmu_internal_error(vm, "Expect function after %d parameters count", args_count);

                break;
            }case ACCESS_OPCODE:{
                Value *target_value = peek(vm);

                if(!IS_VALUE_OBJ(target_value)){
                    vmu_error(vm, "Expect object as target of access");
                }

                size_t key_size;
                char *key = read_str(vm, &key_size);
                Obj *target_obj = VALUE_TO_OBJ(target_value);

                switch (target_obj->type){
                    case STR_OBJ_TYPE:{
                        NativeFn *native_fn = native_str_get(key_size, key, vm);

                        if(native_fn){
                            NativeFnObj *native_fn_obj = vmu_create_native_fn(*target_value, native_fn, vm);

                            pop(vm);
                            PUSH_OBJ(native_fn_obj, vm);

                            break;
                        }

                        vmu_error(vm, "Target does not contain symbol '%s'", key);

                        break;
                    }case ARRAY_OBJ_TYPE:{
                        NativeFn *native_fn = native_array_get(key_size, key, vm);

                        if(native_fn){
                            NativeFnObj *native_fn_obj = vmu_create_native_fn(*target_value, native_fn, vm);

                            pop(vm);
                            PUSH_OBJ(native_fn_obj, vm);

                            break;
                        }

                        vmu_error(vm, "Target does not contain symbol '%s'", key);

                        break;
                    }case LIST_OBJ_TYPE:{
                        NativeFn *native_fn = native_list_get(key_size, key, vm);

                        if(native_fn){
                            NativeFnObj *native_fn_obj = vmu_create_native_fn(*target_value, native_fn, vm);

                            pop(vm);
                            PUSH_OBJ(native_fn_obj, vm);

                            break;
                        }

                        vmu_error(vm, "Target does not contain symbol '%s'", key);

                        break;
                    }case DICT_OBJ_TYPE:{
                        NativeFn *native_fn = native_dict_get(key_size, key, vm);

                        if(native_fn){
                            NativeFnObj *native_fn_obj = vmu_create_native_fn(*target_value, native_fn, vm);

                            pop(vm);
                            PUSH_OBJ(native_fn_obj, vm);

                            break;
                        }

                        vmu_error(vm, "Target does not contain symbol '%s'", key);

                        break;
                    }case RECORD_OBJ_TYPE:{
                        RecordObj *record_obj = OBJ_TO_RECORD(target_obj);
                        Value out_value = vmu_record_get_attr(key_size, key, record_obj, vm);

                        pop(vm);
                        push(out_value, vm);

                        break;
                    }case NATIVE_MODULE_OBJ_TYPE:{
                        NativeModuleObj *native_module_obj = OBJ_TO_NATIVE_MODULE(target_obj);
                        NativeModule *native_module = native_module_obj->native_module;
                        NativeModuleSymbol *native_module_symbol = NULL;

                        if(!lzohtable_lookup(key, key_size, native_module->symbols, (void **)(&native_module_symbol))){
                            vmu_error(vm, "Native module '%s' does not contain symbol '%s'", native_module->name, key);
                        }

                        switch (native_module_symbol->type){
                            case NATIVE_FUNCTION_NMSYMTYPE:{
                                NativeFn *native_fn = (NativeFn *)native_module_symbol->content.native_fn;
                                NativeFnObj *native_fn_obj = vmu_create_native_fn((Value){0}, native_fn, vm);

                                pop(vm);
                                PUSH_OBJ(native_fn_obj, vm);

                                break;
                            }default:{
                                vmu_internal_error(vm, "Unknown native symbol type");
                            }
                        }

                        break;
                    }default:{
                        vmu_error(vm, "Illegal access target");
                    }
                }

                break;
            }case INDEX_OPCODE:{
                Value *target_value = peek_at(0, vm);
                Value *idx_value = peek_at(1, vm);

                Value out_value = {0};

                if(IS_VALUE_ARRAY(target_value)){
                    if(!IS_VALUE_INT(idx_value)){
                        vmu_error(vm, "Expect 'INT' as index");
                    }

                    int64_t idx = VALUE_TO_INT(idx_value);
                    ArrayObj *array_obj = VALUE_TO_ARRAY(target_value);
                    out_value = vmu_array_get_at(idx, array_obj, vm);
                }else if(IS_VALUE_LIST(target_value)){
                    if(!IS_VALUE_INT(idx_value)){
                        vmu_error(vm, "Expect 'INT' as index");
                    }

                    int64_t idx = VALUE_TO_INT(idx_value);
                    ListObj *list_obj = VALUE_TO_LIST(target_value);
                    out_value = vmu_list_get_at(idx, list_obj, vm);
                }else if(IS_VALUE_DICT(target_value)){
                    DictObj *dict_obj = VALUE_TO_DICT(target_value);
                    out_value = vmu_dict_get(*idx_value, dict_obj, vm);
                }else if(IS_VALUE_STR(target_value)){
                    if(!IS_VALUE_INT(idx_value)){
                        vmu_error(vm, "Expect 'INT' as index");
                    }

                    int64_t idx = VALUE_TO_INT(idx_value);
                    StrObj *old_str_obj = VALUE_TO_STR(target_value);
                    StrObj *new_str_obj = vmu_str_char(idx, old_str_obj, vm);
                    out_value = OBJ_VALUE(new_str_obj);
                }

                pop(vm);
                pop(vm);
                push(out_value, vm);

                break;
            }case RET_OPCODE:{
                Value *result_value = pop(vm);
                Frame *current = current_frame(vm);

                for (OutValue *out_value = current->outs_head; out_value; out_value = out_value->next){
                    out_value->linked = 0;
                    remove_value_from_current_frame(out_value, vm);
                }

                vm->stack_top = current->locals;

                pop_frame(vm);

                if(vm->frame_ptr == vm->frame_stack){
                    return vm->exit_code;
                }

                push(*result_value, vm);

                break;
            }case IS_OPCODE:{
                Value *value = pop(vm);
                uint8_t type = advance(vm);

                if(IS_VALUE_OBJ(value)){
                    Obj *obj = VALUE_TO_OBJ(value);

                    switch(obj->type){
                        case STR_OBJ_TYPE:{
                            push(BOOL_VALUE(type == 4), vm);
                            break;
                        }case ARRAY_OBJ_TYPE:{
                            push(BOOL_VALUE(type == 5), vm);
                            break;
                        }case LIST_OBJ_TYPE:{
                            push(BOOL_VALUE(type == 6), vm);
                            break;
                        }case DICT_OBJ_TYPE:{
                            push(BOOL_VALUE(type == 7), vm);
                            break;
                        }case RECORD_OBJ_TYPE:{
                            push(BOOL_VALUE(type == 8), vm);
                            break;
                        }case FN_OBJ_TYPE:
                        case CLOSURE_OBJ_TYPE:
                        case NATIVE_FN_OBJ_TYPE:{
                            push(BOOL_VALUE(type == 9), vm);
                            break;
                        }default:{
                            vmu_error(vm, "Illegal object type");
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
                        }case FLOAT_VTYPE:{
                            push(BOOL_VALUE(type == 3), vm);
                            break;
                        }default:{
                            vmu_error(vm, "Illegal value type");
                            break;
                        }
                    }
                }

                break;
            }case TRYO_OPCODE:{
                size_t catch_ip = (size_t)read_i16(vm);
                Exception *ex = MEMORY_ALLOC(Exception, 1, vm->allocator);

                ex->catch_ip = catch_ip;
                ex->stack_top = vm->stack_top;
                ex->frame = current_frame(vm);
                ex->prev = vm->exception_stack;
                vm->exception_stack = ex;

                break;
            }case TRYC_OPCODE:{
                Exception *ex = vm->exception_stack;

                if(ex){
                    vm->exception_stack = ex->prev;
                    MEMORY_DEALLOC(Exception, 1, ex, vm->allocator);
                    break;
                }

                vmu_internal_error(vm, "Exception stack is empty");

                break;
            }case THROW_OPCODE:{
                uint8_t has_value = advance(vm);
                Value *raw_value = NULL;
                StrObj *throw_msg = NULL;

                if(has_value){
                    raw_value = pop(vm);

                    if(IS_VALUE_STR(raw_value)){
                        throw_msg = VALUE_TO_STR(raw_value);
                    }else if(IS_VALUE_RECORD(raw_value)){
                        RecordObj *record = VALUE_TO_RECORD(raw_value);

                        if(record->attrs){
                            char *raw_key = "msg";
                            size_t key_size = 3;
                            Value *msg_value = NULL;

                            if(lzohtable_lookup(raw_key, key_size, record->attrs, (void **)(&msg_value))){
                                if(!IS_VALUE_STR(msg_value)){
                                    vmu_error(vm, "Expect record attribute 'msg' to be of type 'str'");
                                }

                                throw_msg = VALUE_TO_STR(msg_value);
                            }
                        }
                    }
                }

                Exception *ex = vm->exception_stack;

                if(ex){
                    ex->throw_value = raw_value ? *raw_value : (Value){0};
                    longjmp(vm->exit_jmp, 2);
                }

                char *raw_throw_msg = throw_msg ? throw_msg->buff : "";

                vmu_error(vm, raw_throw_msg);

                break;
            }case HLT_OPCODE:{
                return 0;
            }default:{
                assert("Illegal opcode");
            }
        }
    }

    return vm->exit_code;
}
//> PRIVATE IMPLEMENTATION
//> PUBLIC IMPLEMENTATION
VM *vm_create(Allocator *allocator){
    LZOHTable *runtime_strs = FACTORY_LZOHTABLE(NULL);
    DynArr *native_symbols = FACTORY_DYNARR_PTR(NULL);
    VM *vm = MEMORY_ALLOC(VM, 1, allocator);

    if(!runtime_strs || !native_symbols || !vm){
        LZOHTABLE_DESTROY(runtime_strs);
        dynarr_destroy(native_symbols);
        MEMORY_DEALLOC(VM, 1, vm, allocator);
        return NULL;
    }

    memset(vm, 0, sizeof(VM));
    vm->runtime_strs = runtime_strs;
    vm->native_symbols = native_symbols;
    vm->allocation_limit_size = ALLOCATE_START_LIMIT;
    vm->allocator = allocator;

    return vm;
}

void vm_destroy(VM *vm){
    if(!vm){
        return;
    }

    vmu_clean_up(vm);

    DynArr *native_symbols = vm->native_symbols;
    const size_t native_symbols_len = DYNARR_LEN(native_symbols);

    for (size_t i = 0; i < native_symbols_len; i++){
        LZOHTable *symbols = (LZOHTable *)dynarr_get_ptr(i, native_symbols);
        LZOHTABLE_DESTROY(symbols);
    }

    LZOHTABLE_DESTROY(vm->runtime_strs);
    dynarr_destroy(native_symbols);
    MEMORY_DEALLOC(VM, 1, vm, vm->allocator);
}

void vm_initialize(VM *vm){
    vm->while_objs = (ObjList){0};
    vm->gray_objs = (ObjList){0};
    vm->black_objs = (ObjList){0};
    MEMORY_INIT_ALLOCATOR(vm, vm_alloc, vm_realloc, vm_dealloc, &vm->fake_allocator);
}

int vm_execute(LZOHTable *native_fns, Module *module, VM *vm){
    switch (setjmp(vm->exit_jmp)){
        case 0:{
            module->submodule->resolved = 1;

            vm->exit_code = OK_VMRESULT;
            vm->stack_top = vm->stack;
            vm->frame_ptr = vm->frame_stack;

            vm->native_fns = native_fns;

            vm->module_ptr = 0;
            vm->main_module = module;
            vm->modules[vm->module_ptr++] = module;

            Fn *main_fn = (Fn *)get_symbol(0, FUNCTION_SUBMODULE_SYM_TYPE, vm->modules[0], vm);
            push_fn(main_fn, vm);

            Frame *frame = push_frame(0, vm);
            frame->fn = main_fn;

            return execute(vm);
        }case 1:{
            // In case of error
            return vm->exit_code;
        }case 2:{
            // In case of throws
            Exception *ex = vm->exception_stack;
            Value throw_value = ex->throw_value;
            Frame *frame = ex->frame;

            frame->ip = ex->catch_ip;
            vm->stack_top = ex->stack_top;
            vm->frame_ptr = frame + 1;
            vm->exception_stack = ex->prev;

            MEMORY_DEALLOC(Exception, 1, ex, vm->allocator);
            push(throw_value, vm);

            return execute(vm);
        }default:{
            assert(0 && "Illegal jump value");
        }
    }
}
//< PUBLIC IMPLEMENTATION