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
// UTILS FUNCTIONS
static int16_t compose_i16(uint8_t *bytes);
static int32_t compose_i32(uint8_t *bytes);
static int16_t read_i16(VM *vm);
static int32_t read_i32(VM *vm);
static int64_t read_i64_const(VM *vm);
static double read_float_const(VM *vm);
static char *read_str(VM *vm, uint32_t *out_hash);
// STACK FUNCTIONS
static Value *peek(VM *vm);
static Value *peek_at(int offset, VM *vm);

#define PUSH(value, vm) {                 \
    if(vm->stack_ptr >= STACK_LENGTH){    \
        vmu_error(vm, "Stack over flow"); \
    }                                     \
    vm->stack[vm->stack_ptr++] = value;   \
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

#define PUSH_CORE_STR(buff, vm){               \
    Obj *str_obj = vmu_core_str_obj(buff, vm); \
    Value obj_value = OBJ_VALUE(str_obj);      \
    PUSH(obj_value, vm);                       \
}

#define PUSH_UNCORE_STR(buff, vm){               \
    Obj *str_obj = vmu_uncore_str_obj(buff, vm); \
    Value obj_value = OBJ_VALUE(str_obj);        \
    PUSH(obj_value, vm);                         \
}

#define PUSH_NON_NATIVE_FN(fn, vm) {          \
    Obj *obj = vmu_obj(FN_OTYPE, vm);         \
    if(!obj){vmu_error(vm, "Out of memory");} \
    obj->value.f##n = fn;                     \
    Value fn_value = OBJ_VALUE(obj);          \
    PUSH(fn_value, vm)                        \
}

#define PUSH_NATIVE_FN(fn, vm) {              \
    Obj *obj = vmu_obj(NATIVE_FN_OTYPE, vm);  \
    if(!obj){vmu_error(vm, "Out of memory");} \
    obj->value.native_fn = fn;                \
    Value fn_value = OBJ_VALUE(obj);          \
    PUSH(fn_value, vm)                        \
}

#define PUSH_NATIVE_MODULE(m, vm){               \
	Obj *obj = vmu_obj(NATIVE_MODULE_OTYPE, vm); \
	if(!obj){vmu_error(vm, "out of memory");}    \
	obj->value.native_module = (m);              \
	PUSH(OBJ_VALUE(obj), vm);                    \
}

#define PUSH_MODULE(m, vm) {                  \
    Obj *obj = vmu_obj(MODULE_OTYPE, vm);     \
	if(!obj){vmu_error(vm, "Out of memory");} \
	obj->value.module = m;                    \
	PUSH(OBJ_VALUE(obj), vm);                 \
}

static void push_fn(Fn *fn, VM *vm);
static void push_closure(MetaClosure *meta, VM *vm);
static void push_native_module_symbol(char *name, NativeModule *module, VM *vm);
static void push_module_symbol(int32_t index, Module *module, VM *vm);
static Value *pop(VM *vm);
// FRAMES FUNCTIONS
static void add_value_to_frame(OutValue *value, VM *vm);
static void remove_value_from_frame(OutValue *value, VM *vm);
#define CURRENT_FRAME(vm)(&(vm)->frame_stack[(vm)->frame_ptr - 1])
#define CURRENT_LOCALS(vm)(CURRENT_FRAME(vm)->locals)
#define CURRENT_FN(vm)(CURRENT_FRAME(vm)->fn)
#define CURRENT_CHUNKS(vm)(CURRENT_FN(vm)->chunks)
#define CURRENT_CONSTANTS(vm)(CURRENT_FN(vm)->constants)
#define CURRENT_FLOAT_VALUES(vm)(CURRENT_FN(vm)->float_values)
#define CURRENT_MODULE(vm)((vm)->modules[(vm)->module_ptr - 1])

#define IS_AT_END(vm)((vm)->frame_ptr == 0 || CURRENT_FRAME(vm)->ip >= CURRENT_CHUNKS(vm)->used)
#define ADVANCE(vm)(DYNARR_GET_AS(uint8_t, CURRENT_FRAME(vm)->ip++, CURRENT_CHUNKS(vm)))

static Frame *frame_up_fn(Fn *fn, VM *vm);
static Frame *frame_up(int32_t index, VM *vm);
static void frame_down(VM *vm);
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

void push_closure(MetaClosure *meta, VM *vm){
    OutValue *values = (OutValue *)vmu_alloc(sizeof(OutValue) * meta->values_len);
    Closure *closure = (Closure *)vmu_alloc(sizeof(Closure));
    Obj *obj = vmu_obj(CLOSURE_OTYPE, vm);

    if(!values || !closure || !obj){
        memory_dealloc(values);
        memory_dealloc(closure);
        vmu_error(vm, "Out of memory");
    }

    for (int i = 0; i < meta->values_len; i++){
        OutValue *value = &values[i];
        MetaOutValue *out_value = meta->values + i;

        value->linked = 1;
        value->at = out_value->at;
        value->value = CURRENT_LOCALS(vm) + out_value->at;
        value->prev = NULL;
        value->next = NULL;

        add_value_to_frame(value, vm);
    }

    closure->values_len = meta->values_len;
    closure->values = values;
    closure->meta = meta;

    obj->value.closure = closure;

    PUSH(OBJ_VALUE(obj), vm);
}

void push_native_module_symbol(char *name, NativeModule *module, VM *vm){
    size_t key_size = strlen(name);

    LZHTable *symbols = module->symbols;
    LZHTableNode *symbol_node = NULL;

    if(!lzhtable_contains((uint8_t *)name, key_size, symbols, &symbol_node)){
        vmu_error(vm, "Module '%s' do not contains a symbol '%s'", module->name, name);
    }

    NativeModuleSymbol *symbol = (NativeModuleSymbol *)symbol_node->value;

    if(symbol->type == NATIVE_FUNCTION_NMSYMTYPE){
        PUSH_NATIVE_FN(symbol->value.fn, vm);
    }
}

void push_module_symbol(int32_t index, Module *module, VM *vm){
   	DynArr *symbols = MODULE_SYMBOLS(module);

    if((size_t)index >= DYNARR_LEN(symbols)){
        vmu_error(vm, "Failed to push module symbol: index out of bounds");
    }

    ModuleSymbol *module_symbol = &(DYNARR_GET_AS(ModuleSymbol, (size_t)index, symbols));

	if(module_symbol->type == NATIVE_MODULE_MSYMTYPE){
        PUSH_NATIVE_MODULE(module_symbol->value.native_module, vm);
    }else if(module_symbol->type == FUNCTION_MSYMTYPE){
        Fn *fn = module_symbol->value.fn;
        push_fn(fn, vm);
    }else if(module_symbol->type == CLOSURE_MSYMTYPE){
        MetaClosure *meta_closure = module_symbol->value.meta_closure;
        push_closure(meta_closure, vm);
    }else if(module_symbol->type == MODULE_MSYMTYPE){
        PUSH_MODULE(module_symbol->value.module, vm);
    }
}

Value *pop(VM *vm){
    if(vm->stack_ptr == 0){vmu_error(vm, "Stack under flow");}
    return &vm->stack[--vm->stack_ptr];
}

void add_value_to_frame(OutValue *value, VM *vm){
    Frame *frame = CURRENT_FRAME(vm);

    if(frame->values_tail){
        value->prev = frame->values_tail;
        frame->values_tail->next = value;
    }else{
        frame->values_head = value;
    }

    frame->values_tail = value;
}

void remove_value_from_frame(OutValue *value, VM *vm){
    Frame *frame = CURRENT_FRAME(vm);

    if(value == frame->values_head){
        frame->values_head = value->next;
    }
    if(value == frame->values_tail){
        frame->values_tail = value->prev;
    }

    if(value->prev){
        value->prev->next = value->next;
    }
    if(value->next){
        value->next->prev = value->prev;
    }

    Value *v = vmu_clone_value(value->value, vm);
    if(!v){vmu_error(vm, "Out of memory");}

    value->value = v;
}

Frame *frame_up_fn(Fn *fn, VM *vm){
    if(vm->frame_ptr >= FRAME_LENGTH){
        vmu_error(vm, "Frame stack over flow");
    }

    Frame *frame = &vm->frame_stack[vm->frame_ptr++];

    frame->ip = 0;
    frame->last_offset = 0;
    frame->fn = fn;
    frame->closure = NULL;
    frame->values_head = NULL;
    frame->values_tail = NULL;

    return frame;
}

Frame *frame_up(int32_t index, VM *vm){
    if(vm->frame_ptr >= FRAME_LENGTH){
        vmu_error(vm, "Frame stack over flow");
    }

    Module *module = CURRENT_MODULE(vm);
	DynArr *symbols = MODULE_SYMBOLS(module);

    if((size_t)index >= DYNARR_LEN(symbols)){
        vmu_error(vm, "Failed to push frame: illegal index");
    }

	ModuleSymbol *symbol = &(DYNARR_GET_AS(ModuleSymbol, (size_t)index, symbols));

	if(symbol->type != FUNCTION_MSYMTYPE){
        vmu_error(vm, "Expect symbol of type 'function', but got something else");
    }

    Fn *fn = symbol->value.fn;

    return frame_up_fn(fn, vm);
}

void frame_down(VM *vm){
    if(vm->frame_ptr == 0){vmu_error(vm, "Frame under flow");}

    Frame *current = CURRENT_FRAME(vm);

    for (OutValue *closure_value = current->values_head; closure_value; closure_value = closure_value->next){
        closure_value->linked = 0;
        remove_value_from_frame(closure_value, vm);
    }

    vm->frame_ptr--;
}

static int execute(VM *vm){
    frame_up(0, vm);

    for (;;){
        if(vm->halt){break;}
        if(IS_AT_END(vm)){
            // When resolving modules, 'module_ptr' is greater than 1.
            // In that case, is expected that the current frame contains
            // the 'import' function which must be remove from the call stack
            // in order to the normal execution of the previous frame continues
            if(vm->module_ptr > 1){
                CURRENT_MODULE(vm)->submodule->resolve = 1;
                CURRENT_MODULE(vm) = NULL;
                vm->module_ptr--;
                frame_down(vm);
            }else{
                // The only frame that is allowed to not contains a return
                // instruction at the end is the one which contains the main function
                if(vm->frame_ptr > 1){
                    vmu_error(vm, "Unexpected frame at end of execution");
                }

                break;
            }
        }

        uint8_t chunk = ADVANCE(vm);
        CURRENT_FRAME(vm)->last_offset = CURRENT_FRAME(vm)->ip - 1;

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
            }case OSET_OPCODE:{
                Value *value = peek(vm);
                uint8_t index = ADVANCE(vm);
                Frame *current = CURRENT_FRAME(vm);
                Closure *closure = current->closure;

                for (int i = 0; i < closure->values_len; i++){
                    OutValue *closure_value = &closure->values[i];

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

                for (int i = 0; i < closure->values_len; i++){
                    OutValue *value = &closure->values[i];

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
                GlobalValue *global_value = (GlobalValue *)vmu_alloc(sizeof(GlobalValue));

                if(!new_value || !global_value){
                    vmu_dealloc(new_value);
                    vmu_dealloc(global_value);
                    vmu_error(vm, "Out of memory");
                }

                global_value->access = PRIVATE_GVATYPE;
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

                if(IS_MODULE(value) && !(TO_MODULE(value)->submodule->resolve)){
                    Module *module = TO_MODULE(value);

                    if(vm->module_ptr >= MODULES_LENGTH){
                        vmu_error(vm, "Cannot resolve module '%s'. No space available", module->name);
                    }

                    CURRENT_FRAME(vm)->ip = CURRENT_FRAME(vm)->last_offset;
                    vm->modules[vm->module_ptr++] = module;
                    frame_up(0, vm);

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

                if(IS_NATIVE_MODULE(value) || IS_MODULE(value)){
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
                }else if(IS_CLOSURE(fn_value)){
                    Closure *closure = TO_CLOSURE(fn_value);
                    Fn *fn = closure->meta->fn;
                    uint8_t params_count = fn->params ? fn->params->used : 0;

                    if(params_count != args_count){
                        vmu_error(vm, "Failed to call function '%s'. Declared with %d parameter(s), but got %d argument(s)", fn->name, params_count, args_count);
                    }

                    Frame *frame = frame_up_fn(fn, vm);
                    frame->closure = closure;

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
                        args[i - 1] = *pop(vm);
                    }

                    pop(vm);

                    Value out_value = raw_fn(args_count, args, target, vm);

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
                uint32_t hash = 0;
                char *symbol = read_str(vm, &hash);
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
                    push_native_module_symbol(symbol, module, vm);
                    break;
                }

                if(IS_MODULE(value)){
                    Module *module = TO_MODULE(value);

                    LZHTable *globals = MODULE_GLOBALS(module);
                    LZHTableNode *value_node = NULL;

                    if(lzhtable_hash_contains(hash, globals, &value_node)){
                        GlobalValue *global_value = (GlobalValue *)value_node->value;
                        Value *value = global_value->value;

                        if(global_value->access == PRIVATE_GVATYPE){
                            vmu_error(vm, "Symbol '%s' has private access in module '%s'", symbol, module->name);
                        }

                        PUSH(*value, vm);
                    }else{
                        vmu_error(vm, "Module '%s' does not contains symbol '%s'", module->name, symbol);
                    }

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

    frame_down(vm);

    return vm->exit_code;
}
//> PRIVATE IMPLEMENTATION
//> PUBLIC IMPLEMENTATION
VM *vm_create(){
    VM *vm = (VM *)A_RUNTIME_ALLOC(sizeof(VM));
    memset(vm, 0, sizeof(VM));
    return vm;
}

int vm_execute(LZHTable *natives, Module *module, VM *vm){
    if(setjmp(vm->err_jmp) == 1){
        return vm->exit_code;
    }else{
        module->submodule->resolve = 1;

        vm->halt = 0;
        vm->exit_code = OK_VMRESULT;
        vm->stack_ptr = 0;
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