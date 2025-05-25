#include "vm.h"
#include "types.h"
#include "value.h"
#include "vmu.h"
#include "memory.h"
#include "utils.h"
#include "opcode.h"
#include "tutils.h"

#include "native_str.h"
#include "native_array.h"
#include "native_list.h"
#include "native_dict.h"

#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <setjmp.h>

void *vm_alloc(size_t size, void * ctx){
    VM *vm = (VM *)ctx;
    Allocator *allocator = vm->allocator;
    void *real_ctx = allocator->ctx;

    if(vm->allocated_size + size >= vm->allocated_limit){
        size_t before = vm->allocated_size;
        vmu_gc(vm);
        size_t after = vm->allocated_size;
        size_t freeded = before - after;

        if(freeded == 0){
            vm->allocated_limit *= GROW_ALLOCATE_LIMIT_FACTOR;
        }
    }

    vm->allocated_size += size;

    return allocator->alloc(size, real_ctx);
}

void *vm_realloc(void *ptr, size_t old_size, size_t new_size, void *ctx){
    VM *vm = (VM *)ctx;
    Allocator *allocator = vm->allocator;
    void *real_ctx = allocator->ctx;

    size_t new_allocated_size = vm->allocated_size - old_size + new_size;

    if(new_allocated_size < vm->allocated_limit / GROW_ALLOCATE_LIMIT_FACTOR){
        vm->allocated_limit /= GROW_ALLOCATE_LIMIT_FACTOR;
    }

    vm->allocated_size -= old_size;
    vm->allocated_size += new_size;

    return allocator->realloc(ptr, old_size, new_size, real_ctx);
}

void vm_dealloc(void *ptr, size_t size, void *ctx){
    VM *vm = (VM *)ctx;
    Allocator *allocator = vm->allocator;
    void *real_ctx = allocator->ctx;

    if(vm->allocated_size - size < vm->allocated_size / GROW_ALLOCATE_LIMIT_FACTOR){
        vm->allocated_limit /= GROW_ALLOCATE_LIMIT_FACTOR;
    }

    vm->allocated_size -= size;

    allocator->dealloc(ptr, size, real_ctx);
}

//> PRIVATE INTERFACE
// UTILS FUNCTIONS
static int16_t compose_i16(uint8_t *bytes);
static int32_t compose_i32(uint8_t *bytes);
static int16_t read_i16(VM *vm);
static int32_t read_i32(VM *vm);
static int64_t read_i64_const(VM *vm);
static double read_float_const(VM *vm);
static char *read_str(VM *vm, uint32_t *out_hash);
void *get_symbol(size_t index, SubModuleSymbolType type, Module *module, VM *vm);
// STACK FUNCTIONS
static Value *peek(VM *vm);
static Value *peek_at(int offset, VM *vm);

#define PUSH(_value, _vm) {                                   \
    uintptr_t top = (uintptr_t)((_vm)->stack_top);            \
    uintptr_t end = (uintptr_t)((_vm)->stack + STACK_LENGTH); \
    if(top + VALUE_SIZE > end){                               \
        vmu_error((_vm), "Stack over flow");                  \
    }                                                         \
    *((_vm)->stack_top++) = (_value);                         \
}

#define PUSH_EMPTY(vm) {             \
    Value empty_value = EMPTY_VALUE; \
    PUSH(empty_value, vm)            \
}

#define PUSH_BOOL(value, vm) {            \
    Value bool_value = BOOL_VALUE(value); \
    PUSH(bool_value, vm)                  \
}

#define PUSH_INT(_value, _vm) {          \
    Value int_value = INT_VALUE(_value); \
    PUSH(int_value, _vm)                 \
}

#define PUSH_FLOAT(value, vm) {             \
    Value float_value = FLOAT_VALUE(value); \
    PUSH(float_value, vm)                   \
}

#define PUSH_NATIVE_FN(fn, vm) {                    \
    Obj *obj = vmu_create_obj(NATIVE_FN_OTYPE, vm); \
    obj->content = fn;                              \
    Value fn_value = OBJ_VALUE(obj);                \
    PUSH(fn_value, vm)                              \
}

#define PUSH_NATIVE_MODULE(m, vm){                      \
	Obj *obj = vmu_create_obj(NATIVE_MODULE_OTYPE, vm); \
	obj->content = (m);                                 \
	PUSH(OBJ_VALUE(obj), vm);                           \
}

#define PUSH_MODULE(m, vm) {                     \
    Obj *obj = vmu_create_obj(MODULE_OTYPE, vm); \
	obj->content = m;                            \
	PUSH(OBJ_VALUE(obj), vm);                    \
}

static Obj *push_fn(Fn *fn, VM *vm);
static void push_closure(MetaClosure *meta, VM *vm);
static void push_module_symbol(int32_t index, Module *module, VM *vm);
static Value *pop(VM *vm);
// FRAMES FUNCTIONS
static void add_value_to_frame(OutValue *value, VM *vm);
static void remove_value_from_frame(OutValue *value, VM *vm);

#define IS_AT_END(vm)((vm)->frame_ptr == 0 || CURRENT_FRAME(vm)->ip >= CURRENT_CHUNKS(vm)->used)
#define ADVANCE(vm)(DYNARR_GET_AS(uint8_t, CURRENT_FRAME(vm)->ip++, CURRENT_CHUNKS(vm)))

static Frame *push_frame(int argsc, VM *vm);
static void pop_frame(VM *vm);
#define FRAME_LOCAL(idx, vm) (CURRENT_FRAME(vm)->locals + 1)[(idx)]
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
    LZHTable *strings = MODULE_STRINGS(CURRENT_FN(vm)->module);
    uint32_t hash = (uint32_t)read_i32(vm);
    if(out_hash){*out_hash = hash;}
    return lzhtable_hash_get(hash, strings);
}

void *get_symbol(size_t index, SubModuleSymbolType type, Module *module, VM *vm){
    DynArr *symbols = MODULE_SYMBOLS(module);

    if(index >= DYNARR_LEN(symbols)){
        vmu_error(vm, "Failed to get module symbol: index out of bounds");
    }

    SubModuleSymbol *symbol = &DYNARR_GET_AS(SubModuleSymbol, index, symbols);

    if(symbol->type != type){
        vmu_error(vm, "Failed to get module symbol: mismatch types");
    }

    switch (symbol->type){
        case FUNCTION_MSYMTYPE:{
            return (Fn *)symbol->value;
        }case CLOSURE_MSYMTYPE:{
            return (MetaClosure *)symbol->value;
        }case NATIVE_MODULE_MSYMTYPE:{
            return (NativeModule *)symbol->value;
        }case MODULE_MSYMTYPE:{
            return (Module *)symbol->value;
        }default:{
            vmu_error(vm, "Failed to get module symbol: unexpected symbol type");
        }
    }

    return NULL;
}

Value *peek(VM *vm){
    if(vm->stack_top == vm->stack){
        vmu_error(vm, "Stack is empty");
    }
    return vm->stack_top - 1;
}

Value *peek_at(int offset, VM *vm){
    uintptr_t start = (uintptr_t)vm->stack;
    uintptr_t top = (uintptr_t)vm->stack_top;
    uintptr_t at = top - (VALUE_SIZE * (offset + 1));

    if(at < start){
        vmu_error(vm, "Illegal offset value to peek at stack");
    }

    return (Value *)at;
}

Obj *push_fn(Fn *fn, VM *vm){
	Obj *fn_obj = vmu_create_obj(FN_OTYPE, vm);
	fn_obj->content = fn;
	PUSH(OBJ_VALUE(fn_obj), vm);
    return fn_obj;
}

void push_closure(MetaClosure *meta, VM *vm){
    Obj *closure_obj = vmu_closure_obj(meta, vm);
    Closure *closure = OBJ_TO_CLOSURE(closure_obj);

    for (int i = 0; i < meta->values_len; i++){
        OutValue *out_value = &closure->out_values[i];
        MetaOutValue *meta_out_value = meta->values + i;

        out_value->linked = 1;
        out_value->at = meta_out_value->at;
        out_value->value = &(FRAME_LOCAL(meta_out_value->at, vm));
        out_value->prev = NULL;
        out_value->next = NULL;

        add_value_to_frame(out_value, vm);
    }

    PUSH(OBJ_VALUE(closure_obj), vm);
}

void push_module_symbol(int32_t index, Module *module, VM *vm){
   	DynArr *symbols = MODULE_SYMBOLS(module);

    if((size_t)index >= DYNARR_LEN(symbols)){
        vmu_error(vm, "Failed to push module symbol: index out of bounds");
    }

    SubModuleSymbol *module_symbol = &(DYNARR_GET_AS(SubModuleSymbol, (size_t)index, symbols));

	if(module_symbol->type == FUNCTION_MSYMTYPE){
        push_fn((Fn *)module_symbol->value, vm);
    }else if(module_symbol->type == CLOSURE_MSYMTYPE){
        push_closure((MetaClosure *)module_symbol->value, vm);
    }else if(module_symbol->type == NATIVE_MODULE_MSYMTYPE){
        PUSH_NATIVE_MODULE((NativeModule *)module_symbol->value, vm);
    }else if(module_symbol->type == MODULE_MSYMTYPE){
        PUSH_MODULE((Module *)module_symbol->value, vm);
    }
}

Value *pop(VM *vm){
    if(vm->stack_top == vm->stack){
        vmu_error(vm, "Stack under flow");
    }

    Value *value = --vm->stack_top;

    return value;
}

void add_value_to_frame(OutValue *value, VM *vm){
    Frame *frame = CURRENT_FRAME(vm);

    if(frame->outs_tail){
        frame->outs_tail->next = value;
        value->prev = frame->outs_tail;
    }else{
        frame->outs_head = value;
    }

    frame->outs_tail = value;
}

void remove_value_from_frame(OutValue *value, VM *vm){
    Frame *frame = CURRENT_FRAME(vm);

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

    Value *cloned_value = vmu_clone_value(value->value, vm);

    value->value = cloned_value;
}

Frame *push_frame(int argsc, VM *vm){
    if(vm->frame_ptr >= FRAME_LENGTH){
        vmu_error(vm, "Call stack overflow");
    }

    Frame *frame = &vm->frame_stack[vm->frame_ptr++];

    frame->ip = 0;
    frame->last_offset = 0;
    frame->fn = NULL;
    frame->closure = NULL;
    frame->locals = vm->stack_top - argsc - 1;
    frame->outs_head = NULL;
    frame->outs_tail = NULL;

    return frame;
}

void pop_frame(VM *vm){
    if(vm->frame_ptr == 0){
        vmu_error(vm, "Call stack under flow");
    }

    vm->frame_ptr--;
}

static int execute(VM *vm){
    Fn *main_fn = (Fn *)get_symbol(0, FUNCTION_MSYMTYPE, vm->modules[0], vm);
    push_fn(main_fn, vm);

    Frame *frame = push_frame(0, vm);
    frame->fn = main_fn;

    for (;;){
        if(vm->halt){break;}
        if(IS_AT_END(vm)){
            // When resolving modules, 'module_ptr' is greater than 1.
            // In that case, is expected that the current frame contains
            // the 'import' function which must be remove from the call stack
            // in order to the normal execution of the previous frame continues
            if(vm->module_ptr > 1){
                CURRENT_MODULE(vm)->submodule->resolved = 1;
                CURRENT_MODULE(vm) = NULL;
                vm->module_ptr--;
                pop_frame(vm);
            }else{
                vmu_error(vm, "Unexpected frame at end of execution");
                break;
            }
        }

        uint8_t chunk = ADVANCE(vm);
        CURRENT_FRAME(vm)->last_offset = CURRENT_FRAME(vm)->ip - 1;

        switch (chunk){
            case EMPTY_OPCODE:{
                PUSH_EMPTY(vm)
                break;
            }case TRUE_OPCODE:{
                PUSH_BOOL(1, vm);
                break;
            }case FALSE_OPCODE:{
                PUSH_BOOL(0, vm);
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
                char *raw_str = read_str(vm, NULL);
                Obj *str_obj = vmu_unchecked_str_obj(raw_str, vm);
                Str *str = OBJ_TO_STR(str_obj);

                str->runtime = 0;

                ACKNOWLEDGE_OBJ(str_obj, vm);
                PUSH(OBJ_VALUE(str_obj), vm)

                break;
            }case TEMPLATE_OPCODE:{
                int16_t len = read_i16(vm);
                BStr *bstr = FACTORY_BSTR(vm->fake_allocator);

                for (int16_t i = 0; i < len; i++){
                    Value *value = peek_at(i, vm);
                    vmu_value_to_str(value, bstr);
                }

                char *raw_str = factory_clone_raw_str((char *)bstr->buff, vm->fake_allocator);
                Obj *str_obj = vmu_str_obj(&raw_str, vm);

                bstr_destroy(bstr);

                vm->stack_top = peek_at(len - 1, vm);

                ACKNOWLEDGE_OBJ(str_obj, vm);
                PUSH(OBJ_VALUE(str_obj), vm)

                break;
            }case ARRAY_OPCODE:{
                uint8_t parameter = ADVANCE(vm);
                int32_t index = read_i32(vm);

                if(parameter == 1){
                    Value *length_value = pop(vm);

                    VALIDATE_VALUE_INT(length_value, vm)
                    VALIDATE_ARRAY_SIZE(TO_ARRAY_LENGTH(length_value), vm)

                    Obj *array_obj = vmu_array_obj(TO_ARRAY_LENGTH(length_value), vm);

                    ACKNOWLEDGE_OBJ(array_obj, vm);
                    PUSH(OBJ_VALUE(array_obj), vm)
                }else if(parameter == 2){
                    Value *value = pop(vm);
                    Value *array_value = peek(vm);

                    VALIDATE_VALUE_ARRAY(array_value, vm)

                    Array *array = VALUE_TO_ARRAY(array_value);

                    VALIDATE_ARRAY_INDEX(array->len, &INT_VALUE(index), vm)

                    array->values[index] = *value;
                }else{
                    vmu_error(vm, "Illegal ARRAY opcode parameter: %d", parameter);
                }

                break;
            }case LIST_OPCODE:{
                int16_t len = read_i16(vm);
                Obj *list_obj = vmu_list_obj(vm);
                DynArr *list = OBJ_TO_LIST(list_obj);

                for(int32_t i = 0; i < len; i++){
                    Value *value = peek_at(i, vm);
                    dynarr_insert(value, list);
                }

                vm->stack_top = peek_at(len - 1, vm);

                ACKNOWLEDGE_OBJ(list_obj, vm);
                PUSH(OBJ_VALUE(list_obj), vm)

                break;
            }case DICT_OPCODE:{
                int16_t len = read_i16(vm);
                Obj *dict_obj = vmu_dict_obj(vm);
                LZHTable *dict = OBJ_TO_DICT(dict_obj);

                for (int16_t i = 0; i < len; i++){
                    Value *value = peek_at(i * 2, vm);
                    Value *key = peek_at(i * 2 + 1, vm);

                    if(IS_VALUE_EMPTY(key)){
                        vmu_error(vm, "'key' cannot be 'empty'");
                    }

                    Value *key_clone = vmu_clone_value(key, vm);
                    Value *value_clone = vmu_clone_value(value, vm);
                    uint32_t hash = vmu_hash_value(key_clone);

                    lzhtable_hash_put_key(key_clone, hash, value_clone, dict);
                }

                vm->stack_top = peek_at(len * 2 - 1, vm);

                ACKNOWLEDGE_OBJ(dict_obj, vm);
                PUSH(OBJ_VALUE(dict_obj), vm)

                break;
            }case RECORD_OPCODE:{
                uint8_t len = ADVANCE(vm);
                Obj *record_obj = vmu_record_obj(len, vm);

                if(len == 0){
                    PUSH(OBJ_VALUE(record_obj), vm);
                    break;
                }

                Record *record = OBJ_TO_RECORD(record_obj);
                LZHTable *attributes = record->attributes;

                for(size_t i = 0; i < len; i++){
                    char *key = read_str(vm, NULL);
                    Value *value = peek_at(i, vm);
                    size_t key_size = strlen(key);
                    uint32_t hash = lzhtable_hash((uint8_t *)key, key_size);

                    if(lzhtable_hash_contains(hash, attributes, NULL)){
                        vmu_error(vm, "Record already contains attribute '%s'", key);
                    }

                    char *cloned_key = factory_clone_raw_str(key, vm->fake_allocator);
                    Value *cloned_value = vmu_clone_value(value, vm);

                    lzhtable_hash_put_key(cloned_key, hash, cloned_value, record->attributes);
                }

                vm->stack_top = peek_at(len - 1, vm);

                ACKNOWLEDGE_OBJ(record_obj, vm);
                PUSH(OBJ_VALUE(record_obj), vm)

                break;
            }case ADD_OPCODE:{
                Value *vb = peek_at(0, vm);
                Value *va = peek_at(1, vm);

                if(IS_VALUE_INT(va) && IS_VALUE_INT(vb)){
                    int64_t left = VALUE_TO_INT(va);
                    int64_t right = VALUE_TO_INT(vb);

                    pop(vm);
                    pop(vm);
                    PUSH_INT(left + right, vm)

                    break;
                }

                if(IS_VALUE_FLOAT(va) && IS_VALUE_FLOAT(vb)){
                    double left = VALUE_TO_FLOAT(va);
                    double right = VALUE_TO_FLOAT(vb);

                    pop(vm);
                    pop(vm);
                    PUSH_FLOAT(left + right, vm);

                    break;
                }

                if(IS_VALUE_STR(va) && IS_VALUE_STR(vb)){
                    Str *astr = VALUE_TO_STR(va) ;
                    Str *bstr = VALUE_TO_STR(vb);

                    char *raw_str = utils_join_raw_strs(
                        astr->len,
                        astr->buff,
                        bstr->len,
                        bstr->buff,
                        vm->fake_allocator
                    );
                    Obj *str_obj = vmu_str_obj(&raw_str, vm);

                    pop(vm);
                    pop(vm);
                    PUSH(OBJ_VALUE(str_obj), vm)

                    break;
                }

                vmu_error(vm, "Unsuported types using + operator");

                break;
            }case SUB_OPCODE:{
                Value *vb = pop(vm);
                Value *va = pop(vm);

                if(IS_VALUE_INT(va) && IS_VALUE_INT(vb)){
                    int64_t left = VALUE_TO_INT(va);
                    int64_t right = VALUE_TO_INT(vb);

                    PUSH_INT(left - right, vm)

                    break;
                }

                if(IS_VALUE_FLOAT(va) && IS_VALUE_FLOAT(vb)){
                    double left = VALUE_TO_FLOAT(va);
                    double right = VALUE_TO_FLOAT(vb);

                    PUSH_FLOAT(left - right, vm);

                    break;
                }

                vmu_error(vm, "Unsuported types using - operator");

                break;
            }case MUL_OPCODE:{
                Value *vb = peek_at(0, vm);
                Value *va = peek_at(1, vm);

                if(IS_VALUE_INT(va) && IS_VALUE_INT(vb)){
                    int64_t left = VALUE_TO_INT(va);
                    int64_t right = VALUE_TO_INT(vb);

                    pop(vm);
                    pop(vm);
                    PUSH_INT(left * right, vm);

                    break;
                }

                if(IS_VALUE_FLOAT(va) && IS_VALUE_FLOAT(vb)){
                    double left = VALUE_TO_FLOAT(va);
                    double right = VALUE_TO_FLOAT(vb);

                    pop(vm);
                    pop(vm);
                    PUSH_FLOAT(left * right, vm);

                    break;
                }

                if(IS_VALUE_STR(va) && IS_VALUE_INT(vb)){
                    Str *str = VALUE_TO_STR(va);
                    int64_t by = VALUE_TO_INT(vb);

                    char *raw_str = utils_multiply_raw_str((size_t)by, str->len, str->buff, vm->fake_allocator);
                    Obj *str_obj = vmu_str_obj(&raw_str, vm);

                    pop(vm);
                    pop(vm);
                    PUSH(OBJ_VALUE(str_obj), vm);

                    break;
                }

                if(IS_VALUE_INT(va) && IS_VALUE_STR(vb)){
                    Str *str = VALUE_TO_STR(vb);
                    int64_t by = VALUE_TO_INT(va);

                    char *raw_str = utils_multiply_raw_str((size_t)by, str->len, str->buff, vm->fake_allocator);
                    Obj *str_obj = vmu_str_obj(&raw_str, vm);

                    pop(vm);
                    pop(vm);
                    PUSH(OBJ_VALUE(str_obj), vm);

                    break;
                }

                vmu_error(vm, "Unsuported types using * operator");

                break;
            }case DIV_OPCODE:{
                Value *vb = pop(vm);
                Value *va = pop(vm);

                if(IS_VALUE_INT(va) && IS_VALUE_INT(vb)){
                    int64_t left = VALUE_TO_INT(va);
                    int64_t right = VALUE_TO_INT(vb);

                    if(right == 0){
                        vmu_error(vm, "Division by zero");
                    }

                    PUSH_INT(left / right, vm)

                    break;
                }

                if(IS_VALUE_FLOAT(va) && IS_VALUE_FLOAT(vb)){
                    double left = VALUE_TO_FLOAT(va);
                    double right = VALUE_TO_FLOAT(vb);

                    if(right == 0.0){
                        vmu_error(vm, "Division by zero");
                    }

                    PUSH_FLOAT(left / right, vm)

                    break;
                }

                vmu_error(vm, "Unsuported types using / operator");

                break;
            }case MOD_OPCODE:{
                Value *vb = pop(vm);
                Value *va = pop(vm);

                if(IS_VALUE_INT(va) && IS_VALUE_INT(vb)){
                    int64_t left = VALUE_TO_INT(va);
                    int64_t right = VALUE_TO_INT(vb);

                    PUSH_INT(left % right, vm)

                    break;
                }

                vmu_error(vm, "Unsuported types using 'mod' operator");

                break;
            }case BNOT_OPCODE:{
                Value *value = pop(vm);

                if(IS_VALUE_INT(value)){
                    PUSH(INT_VALUE(~VALUE_TO_INT(value)), vm)
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

                    PUSH(INT_VALUE(left << right), vm)

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

                    PUSH(INT_VALUE(left >> right), vm)

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

                    PUSH(INT_VALUE(left & right), vm)

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

                    PUSH(INT_VALUE(left ^ right), vm)

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

                    PUSH(INT_VALUE(left | right), vm)

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

                    PUSH_BOOL(left < right, vm)

                    break;
                }

                if(IS_VALUE_FLOAT(va) && IS_VALUE_FLOAT(vb)){
                    double left = VALUE_TO_FLOAT(va);
                    double right = VALUE_TO_FLOAT(vb);

                    PUSH_BOOL(left < right, vm)

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

                    PUSH_BOOL(left > right, vm)

                    break;
                }

                if(IS_VALUE_FLOAT(va) && IS_VALUE_FLOAT(vb)){
                    double left = VALUE_TO_FLOAT(va);
                    double right = VALUE_TO_FLOAT(vb);

                    PUSH_BOOL(left > right, vm)

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

                    PUSH_BOOL(left <= right, vm)

                    break;
                }

                if(IS_VALUE_FLOAT(va) && IS_VALUE_FLOAT(vb)){
                    double left = VALUE_TO_FLOAT(va);
                    double right = VALUE_TO_FLOAT(vb);

                    PUSH_BOOL(left <= right, vm)

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

                    PUSH_BOOL(left >= right, vm)

                    break;
                }

                if(IS_VALUE_FLOAT(va) && IS_VALUE_FLOAT(vb)){
                    double left = VALUE_TO_FLOAT(va);
                    double right = VALUE_TO_FLOAT(vb);

                    PUSH_BOOL(left >= right, vm)

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

                    PUSH_BOOL(left == right, vm)

                    break;
                }

                if(IS_VALUE_FLOAT(va) && IS_VALUE_FLOAT(vb)){
                    double left = VALUE_TO_FLOAT(va);
                    double right = VALUE_TO_FLOAT(vb);

                    PUSH_BOOL(left == right, vm)

                    break;
                }

                vmu_error(vm, "Unsuported types using == operator");

                break;
            }case NE_OPCODE:{
                Value *vb = pop(vm);
                Value *va = pop(vm);

                if(IS_VALUE_BOOL(va) && IS_VALUE_BOOL(vb)){
                    uint8_t left = VALUE_TO_BOOL(va);
                    uint8_t right = VALUE_TO_BOOL(vb);

                    PUSH_BOOL(left != right, vm);

                    break;
                }

                if(IS_VALUE_INT(va) && IS_VALUE_INT(vb)){
                    int64_t left = VALUE_TO_INT(va);
                    int64_t right = VALUE_TO_INT(vb);

                    PUSH_BOOL(left != right, vm)

                    break;
                }

                if(IS_VALUE_FLOAT(va) && IS_VALUE_FLOAT(vb)){
                    double left = VALUE_TO_FLOAT(va);
                    double right = VALUE_TO_FLOAT(vb);

                    PUSH_BOOL(left != right, vm)

                    break;
                }

                vmu_error(vm, "Unsuported types using != operator");

                break;
            }case OR_OPCODE:{
                Value *vb = pop(vm);
                Value *va = pop(vm);

                if(IS_VALUE_BOOL(va) && IS_VALUE_BOOL(vb)){
                    PUSH_BOOL(VALUE_TO_BOOL(va) || VALUE_TO_BOOL(vb), vm);
                    break;
                }

                vmu_error(vm, "Unsupported types using 'or' operator");

                break;
            }case AND_OPCODE:{
                Value *vb = pop(vm);
                Value *va = pop(vm);

                if(IS_VALUE_BOOL(va) && IS_VALUE_BOOL(vb)){
                    PUSH_BOOL(VALUE_TO_BOOL(va) && VALUE_TO_BOOL(vb), vm);
                    break;
                }

                vmu_error(vm, "Unsupported types using 'and' operator");

                break;
            }case NOT_OPCODE:{
                Value *vb = pop(vm);

                if(!IS_VALUE_BOOL(vb)){
                    vmu_error(vm, "Expect boolean at right side");
                }

                uint8_t right = VALUE_TO_BOOL(vb);
                PUSH_BOOL(!right, vm)

                break;
            }case NNOT_OPCODE:{
                Value *vb = pop(vm);

                if(IS_VALUE_INT(vb)){
                    int64_t right = -VALUE_TO_INT(vb);
                    PUSH_INT(right, vm)
                    break;
                }

                if(IS_VALUE_FLOAT(vb)){
                    double right = VALUE_TO_FLOAT(vb);
                    PUSH_FLOAT(-right, vm)
                    break;
                }

                vmu_error(vm, "Expect integer or float at right side");

                break;
            }case LSET_OPCODE:{
                Value value = *peek(vm);
                uint8_t index = ADVANCE(vm);
                FRAME_LOCAL(index, vm) = value;
                break;
            }case LGET_OPCODE:{
                uint8_t index = ADVANCE(vm);
                Value value = FRAME_LOCAL(index, vm);
                PUSH(value, vm);
                break;
            }case OSET_OPCODE:{
                Value *value = peek(vm);
                uint8_t index = ADVANCE(vm);
                Closure *closure = CURRENT_FRAME(vm)->closure;
                MetaClosure *meta = closure->meta;

                for (int i = 0; i < meta->values_len; i++){
                    OutValue *closure_value = &closure->out_values[i];

                    if(closure_value->at == index){
                        *closure_value->value = *value;
                        break;
                    }
                }

                break;
            }case OGET_OPCODE:{
                uint8_t index = ADVANCE(vm);
                Frame *current = CURRENT_FRAME(vm);
                Closure *closure = current->closure;
                MetaClosure *meta = closure->meta;

                for (int i = 0; i < meta->values_len; i++){
                    OutValue *value = &closure->out_values[i];

                    if(value->at == index){
                        PUSH(*value->value, vm);
                        break;
                    }
                }

                break;
            }case GDEF_OPCODE:{
                Value *value = pop(vm);

                uint32_t hash = 0;
                char *name = read_str(vm, &hash);

                Module *module = CURRENT_FRAME(vm)->fn->module;
                LZHTable *globals = MODULE_GLOBALS(module);

                if(lzhtable_hash_contains(hash, globals, NULL)){
                    vmu_error(vm, "Already exists a global variable name as '%s'", name);
                }

                Value *new_value = vmu_clone_value(value, vm);
                GlobalValue *global_value = vmu_global_value(vm);

                global_value->value = new_value;

                lzhtable_hash_put(hash, global_value, globals);

                break;
            }case GSET_OPCODE:{
                Value *value = peek(vm);

                uint32_t hash = 0;
                char *name = read_str(vm, &hash);

                Module *module = CURRENT_FRAME(vm)->fn->module;
                LZHTable *globals = MODULE_GLOBALS(module);

                LZHTableNode *value_node = NULL;

                if(lzhtable_hash_contains(hash, globals, &value_node)){
                    GlobalValue *global_value = (GlobalValue *)value_node->value;
                    *global_value->value = *value;
                }else{
                    vmu_error(vm, "Global '%s' does not exists", name);
                }

                break;
            }case GGET_OPCODE:{
                uint32_t hash = 0;
                char *symbol = read_str(vm, &hash);

                Module *module = CURRENT_FRAME(vm)->fn->module;
                LZHTable *globals = MODULE_GLOBALS(module);

                LZHTableNode *value_node = NULL;

                if(!lzhtable_hash_contains(hash, globals, &value_node)){
                    vmu_error(vm, "Global symbol '%s' does not exists", symbol);
                }

                GlobalValue *global_value = (GlobalValue *)value_node->value;
                Value *value = global_value->value;

                if(IS_VALUE_MODULE(value) && !(VALUE_TO_MODULE(value)->submodule->resolved)){
                    if(vm->module_ptr >= MODULES_LENGTH){
                        vmu_error(vm, "Cannot resolve module '%s'. No space available", module->name);
                    }

                    CURRENT_FRAME(vm)->ip = CURRENT_FRAME(vm)->last_offset;

                    Module *module = VALUE_TO_MODULE(value);
                    Fn *import_fn = (Fn *)get_symbol(0, FUNCTION_MSYMTYPE, module, vm);
                    push_fn(import_fn, vm);
                    Frame *frame = push_frame(0, vm);

                    frame->fn = import_fn;
                    vm->modules[vm->module_ptr++] = module;

                    break;
                }

                PUSH(*value, vm);

                break;
            }case GASET_OPCODE:{
                Module *module = CURRENT_FRAME(vm)->fn->module;
                LZHTable *globals = MODULE_GLOBALS(module);

                uint32_t hash = 0;
                char *symbol = read_str(vm, &hash);

                LZHTableNode *value_node = NULL;

                if(!lzhtable_hash_contains(hash, globals, &value_node)){
                    vmu_error(vm, "Global symbol '%s' does not exists", symbol);
                }

                GlobalValue *global_value = (GlobalValue *)value_node->value;
                Value *value = global_value->value;
                uint8_t access_type = ADVANCE(vm);

                if(IS_VALUE_NATIVE_MODULE(value) || IS_VALUE_MODULE(value)){
                    vmu_error(vm, "Modules cannot modify its access");
                }

                if(access_type == 0){
                    global_value->access = PRIVATE_GVATYPE;
                }else if(access_type == 1){
                    global_value->access = PUBLIC_GVATYPE;
                }else{
                    vmu_error(vm, "Illegal access type: %d", access_type);
                }

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
                int32_t index = read_i32(vm);
                Module *module = CURRENT_FRAME(vm)->fn->module;
                push_module_symbol(index, module, vm);
                break;
            }case ASET_OPCODE:{
                Value *target_value =  pop(vm);
                Value *index_value =  pop(vm);
                Value *value =  peek(vm);

                VALIDATE_VALUE_ARRAY(target_value, vm)

                Array *array = VALUE_TO_ARRAY(target_value);

                VALIDATE_ARRAY_INDEX(array->len, index_value, vm)

                array->values[TO_ARRAY_INDEX(index_value)] = *value;

                break;
            }case PUT_OPCODE:{
                Value *target = pop(vm);
                Value *value = peek(vm);
                char *symbol = read_str(vm, NULL);
                Record *record = NULL;

                if(!IS_VALUE_RECORD(target)){
                    vmu_error(vm, "Expect a record, but got something else");
                }

                record = VALUE_TO_RECORD(target);

                uint8_t *key = (uint8_t *)symbol;
                size_t key_size = strlen(symbol);
                uint32_t hash = lzhtable_hash(key, key_size);
                LZHTableNode *node = NULL;

                if(!lzhtable_hash_contains(hash, record->attributes, &node)){
                    vmu_error(vm, "Record do not contains key '%s'", symbol);
                }

                *(Value *)node->value = *value;

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

                if(!IS_VALUE_BOOL(value)){
                    vmu_error(vm, "Expect boolean as conditional value");
                }

                uint8_t condition = VALUE_TO_BOOL(value);
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

                if(!IS_VALUE_BOOL(value)){
                    vmu_error(vm, "Expect boolean as conditional value");
                }

                uint8_t condition = VALUE_TO_BOOL(value);
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
                Value *callable_value = peek_at(args_count, vm);

                if(IS_VALUE_FN(callable_value)){
                    Fn *fn = VALUE_TO_FN(callable_value);
                    uint8_t params_count = fn->params ? DYNARR_LEN(fn->params) : 0;

                    if(params_count != args_count){
                        vmu_error(vm, "Failed to call function '%s'. Declared with %d parameter(s), but got %d argument(s)", fn->name, params_count, args_count);
                    }

                    Frame *frame = push_frame(args_count, vm);

                    frame->fn = fn;

                    break;
                }

                if(IS_VALUE_CLOSURE(callable_value)){
                    Closure *closure = VALUE_TO_CLOSURE(callable_value);
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
                    NativeFn *native_fn = VALUE_TO_NATIVE_FN(callable_value);
                    Value *target = &native_fn->target;
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

                    PUSH(out_value, vm);

                    break;
                }

                vmu_error(vm, "Expect function after %d parameters count", args_count);

                break;
            }case ACCESS_OPCODE:{
                uint32_t hash = 0;
                char *symbol = read_str(vm, &hash);
                Value *value = peek(vm);

                if(IS_VALUE_STR(value)){
                    NativeFnInfo *info = native_str_get(symbol, vm);

                    if(!info){
                        vmu_error(vm, "String does not have symbol '%s'", symbol);
                    }

                    Obj *native_fn_obj = vmu_native_fn_obj(info->arity, symbol, value, info->raw_native, vm);

                    pop(vm);
                    PUSH(OBJ_VALUE(native_fn_obj), vm);

                    break;
                }

                if(IS_VALUE_ARRAY(value)){
                    NativeFnInfo *info = native_array_get(symbol, vm);

                    if(!info){
                        vmu_error(vm, "Array does not have symbol '%s'", symbol);
                    }

                    Obj *native_fn_obj = vmu_native_fn_obj(info->arity, symbol, value, info->raw_native, vm);

                    pop(vm);
                    PUSH(OBJ_VALUE(native_fn_obj), vm);

                    break;
                }

                if(IS_VALUE_LIST(value)){
                    NativeFnInfo *info = native_list_get(symbol, vm);

                    if(!info){
                        vmu_error(vm, "List does not have symbol '%s'", symbol);
                    }

                    Obj *native_fn_obj = vmu_native_fn_obj(info->arity, symbol, value, info->raw_native, vm);

                    pop(vm);

                    ACKNOWLEDGE_OBJ(native_fn_obj, vm);
                    PUSH(OBJ_VALUE(native_fn_obj), vm);

                    break;
                }

                if(IS_VALUE_DICT(value)){
                    NativeFnInfo *info = native_dict_get(symbol, vm);

                    if(!info){
                        vmu_error(vm, "List does not have symbol '%s'", symbol);
                    }

                    Obj *native_fn_obj = vmu_native_fn_obj(info->arity, symbol, value, info->raw_native, vm);

                    pop(vm);
                    PUSH(OBJ_VALUE(native_fn_obj), vm);

                    break;
                }

                if(IS_VALUE_RECORD(value)){
                    Record *record = VALUE_TO_RECORD(value);
                    LZHTable *key_values = record->attributes;

                    if(!key_values){
                        vmu_error(vm, "Record has no attributes");
                    }

                    uint8_t *key = (uint8_t *)symbol;
                    size_t key_size = strlen(symbol);
                    Value *value = (Value *)lzhtable_get(key, key_size, key_values);

                    if(!value){
                        vmu_error(vm, "Record do not constains key '%s'", symbol);
                    }

                    pop(vm);
                    PUSH(*value, vm);

                    break;
                }

                if(IS_VALUE_NATIVE_MODULE(value)){
                    NativeModule *module = VALUE_TO_NATIVE_MODULE(value);
                    uint8_t *key = (uint8_t *)symbol;
                    size_t key_size = strlen(symbol);
                    LZHTable *symbols = module->symbols;
                    LZHTableNode *symbol_node = NULL;

                    if(!lzhtable_contains(key, key_size, symbols, &symbol_node)){
                        vmu_error(vm, "Module '%s' do not contains a symbol '%s'", module->name, symbol);
                    }

                    NativeModuleSymbol *symbol = (NativeModuleSymbol *)symbol_node->value;

                    if(symbol->type == NATIVE_FUNCTION_NMSYMTYPE){
                        NativeFn native_fn = *symbol->value.fn;
                        Obj *native_fn_obj = vmu_native_fn_obj(native_fn.arity, native_fn.name, NULL, native_fn.raw_fn, vm);

                        pop(vm);
                        PUSH(OBJ_VALUE(native_fn_obj), vm);

                        break;
                    }
                }else if(IS_VALUE_MODULE(value)){
                    Module *module = VALUE_TO_MODULE(value);
                    LZHTable *globals = MODULE_GLOBALS(module);
                    LZHTableNode *value_node = NULL;

                    if(lzhtable_hash_contains(hash, globals, &value_node)){
                        GlobalValue *global_value = (GlobalValue *)value_node->value;
                        Value *value = global_value->value;

                        if(global_value->access == PRIVATE_GVATYPE){
                            vmu_error(vm, "Symbol '%s' has private access in module '%s'", symbol, module->name);
                        }

                        pop(vm);
                        PUSH(*value, vm);

                        break;
                    }else{
                        vmu_error(vm, "Module '%s' does not contains symbol '%s'", module->name, symbol);
                    }
                }

                vmu_error(vm, "Illegal target to access");

                break;
            }case INDEX_OPCODE:{
                Value *target_value = pop(vm);
                Value *index_value = pop(vm);

                if(IS_VALUE_ARRAY(target_value)){
                    Array *array = VALUE_TO_ARRAY(target_value);

                    VALIDATE_ARRAY_INDEX(array->len, index_value, vm)

                    Value value = array->values[TO_ARRAY_INDEX(index_value)];

                    PUSH(value, vm);

                    break;
                }

                if(IS_VALUE_LIST(target_value)){
                    Value value = native_fn_list_get(1, index_value, target_value, vm);
                    PUSH(value, vm);
                    break;
                }

                if(IS_VALUE_DICT(target_value)){
                    LZHTable *dict = VALUE_TO_DICT(target_value);
                    LZHTableNode *value_node = NULL;
                    uint32_t hash = vmu_hash_value(index_value);

                    if(lzhtable_hash_contains(hash, dict, &value_node)){
                        Value *value = (Value *)value_node->value;
                        PUSH(*value, vm);
                        break;
                    }

                    vmu_error(vm, "Unknown key");

                    break;
                }

                if(IS_VALUE_STR(target_value)){
                    Value value = native_fn_str_char_at(1, index_value, target_value, vm);
                    PUSH(value, vm)
                    break;
                }

                vmu_error(vm, "Illegal target");

                break;
            }case RET_OPCODE:{
                Value *result_value = pop(vm);
                Frame *current = CURRENT_FRAME(vm);

                for (OutValue *out_value = current->outs_head; out_value; out_value = out_value->next){
                    out_value->linked = 0;
                    remove_value_from_frame(out_value, vm);
                }

                vm->stack_top = current->locals;

                if(vm->module_ptr > 1){
                    CURRENT_MODULE(vm)->submodule->resolved = 1;
                    CURRENT_MODULE(vm) = NULL;
                    vm->module_ptr--;

                    pop_frame(vm);

                    break;
                }

                pop_frame(vm);

                if(vm->frame_ptr == 0){
                    return vm->exit_code;
                }

                PUSH(*result_value, vm)

                break;
            }case IS_OPCODE:{
                Value *value = pop(vm);
                uint8_t type = ADVANCE(vm);

                if(IS_VALUE_OBJ(value)){
                    Obj *obj = VALUE_TO_OBJ(value);

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
                        }case FN_OTYPE:
                        case CLOSURE_OTYPE:
                        case NATIVE_FN_OTYPE:{
                            PUSH(BOOL_VALUE(type == 9), vm);
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

                if(IS_VALUE_STR(value)){
                    throw_message = VALUE_TO_STR(value);
                }else if(IS_VALUE_RECORD(value)){
                    Record *record = VALUE_TO_RECORD(value);

                    if(record->attributes){
                        char *raw_key = "message";

                        uint8_t *key = (uint8_t *)raw_key;
                        size_t key_size = strlen(raw_key);

                        LZHTableNode *message_node = NULL;

                        if(lzhtable_contains(key, key_size, record->attributes, &message_node)){
                            Value *value = (Value *)message_node->value;

                            if(!IS_VALUE_STR(value)){
                                vmu_error(vm, "Expect record attribute 'message' to be of type string");
                            }

                            throw_message = VALUE_TO_STR(value);
                        }
                    }
                }

                char throw = 1;
                int frame_ptr = (int)vm->frame_ptr;

                while(frame_ptr > 0){
                    Frame *frame = &vm->frame_stack[--frame_ptr];
                    Fn *fn = frame->fn;
                    Module *module = fn->module;
                    SubModule *submodule = module->submodule;
                    LZHTable *tries = submodule->tries;

                    uint8_t *key = (uint8_t *)fn;
                    size_t key_size = sizeof(Fn);

                    LZHTableNode *tries_node = NULL;

                    if(lzhtable_contains(key, key_size, tries, &tries_node)){
                        TryBlock *selected_try_block = NULL;
                        DynArr *tries = (DynArr *)tries_node->value;

                        for(size_t i = 0; i < tries->used; i++){
                            TryBlock *try_block = (TryBlock *)dynarr_get_ptr(i, tries);
                            size_t start = try_block->try;
                            size_t end = try_block->catch;
                            size_t ip = frame->ip;

                            if(ip < start || ip > end){
                                continue;
                            }

                            selected_try_block = try_block;

                            break;
                        }

                        if(selected_try_block){
                            throw = 0;
                            frame->ip = selected_try_block->catch;
                            vm->frame_ptr = frame_ptr + 1;
                            FRAME_LOCAL(selected_try_block->local, vm) = *value;

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
                break;
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
    Allocator *fake_allocator = MEMORY_ALLOC(Allocator, 1, allocator);
    VM *vm = MEMORY_ALLOC(VM, 1, allocator);

    if(!fake_allocator || !vm){
        MEMORY_DEALLOC(Allocator, 1, fake_allocator, allocator);
        MEMORY_DEALLOC(VM, 1, vm, allocator);

        return NULL;
    }

    MEMORY_INIT_ALLOCATOR(vm, vm_alloc, vm_realloc, vm_dealloc, fake_allocator);

    memset(vm, 0, sizeof(VM));

    vm->allocated_limit = ALLOCATE_START_LIMIT;
    vm->allocator = allocator;
    vm->fake_allocator = fake_allocator;

    return vm;
}

int vm_execute(LZHTable *natives, Module *module, VM *vm){
    if(setjmp(vm->err_jmp) == 1){
        return vm->exit_code;
    }else{
        module->submodule->resolved = 1;

        vm->halt = 0;
        vm->exit_code = OK_VMRESULT;
        vm->stack_top = vm->stack;
        vm->frame_ptr = 0;
        vm->module_ptr = 0;
        vm->natives = natives;
        vm->modules[vm->module_ptr++] = module;

        int result = execute(vm);
        vmu_clean_up(vm);

        return result;
    }
}
//< PUBLIC IMPLEMENTATION
