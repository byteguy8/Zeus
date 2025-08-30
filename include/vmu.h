#ifndef VM_UTILS_H
#define VM_UTILS_H

#include "vm.h"
#include "bstr.h"
#include "native_fn.h"
#include "tutils.h"
#include <stdio.h>

#define VMU_VM ((VM *)context)
#define VMU_ALLOCATOR (&(((VM *)context)->fake_allocator))

#define VALIDATE_VALUE_BOOL_ARG(_value, _param, _name, _vm){                                          \
    if(!IS_VALUE_BOOL((_value))){                                                                     \
        vmu_error(_vm, "Illegal type of argument %d: expect '%s' of type 'bool'", (_param), (_name)); \
    }                                                                                                 \
}

#define VALIDATE_VALUE_INT_ARG(_value, _param, _name, _vm){                                          \
    if(!IS_VALUE_INT((_value))){                                                                     \
        vmu_error(_vm, "Illegal type of argument %d: expect '%s' of type 'int'", (_param), (_name)); \
    }                                                                                                \
}

#define VALIDATE_VALUE_FLOAT_ARG(_value, _param, _name, _vm){                                          \
    if(!IS_VALUE_FLOAT((_value))){                                                                     \
        vmu_error(_vm, "Illegal type of argument %d: expect '%s' of type 'float'", (_param), (_name)); \
    }                                                                                                  \
}

#define VALIDATE_VALUE_STR_ARG(_value, _param, _name, _vm){                                          \
    if(!IS_VALUE_STR((_value))){                                                                     \
        vmu_error(_vm, "Illegal type of argument %d: expect '%s' of type 'str'", (_param), (_name)); \
    }                                                                                                \
}

#define VALIDATE_VALUE_STR_EMPTY_ARG(_value, _param, _name, _vm){                         \
    VALIDATE_VALUE_STR_ARG(_value, _param, _name, _vm);                                   \
    if(VALUE_TO_STR(_value)->len == 0){                                                   \
        vmu_error(_vm, "Illegal value of argument %d: '%s' is empty", (_param), (_name)); \
    }                                                                                     \
}

#define VALIDATE_VALUE_ARRAY_ARG(_value, _param, _name, _vm){                                          \
    if(!IS_VALUE_ARRAY((_value))){                                                                     \
        vmu_error(_vm, "Illegal type of argument %d: expect '%s' of type 'array'", (_param), (_name)); \
    }                                                                                                  \
}

#define VALIDATE_VALUE_LIST_ARG(_value, _param, _name, _vm){                                          \
    if(!IS_VALUE_LIST((_value))){                                                                     \
        vmu_error(_vm, "Illegal type of argument %d: expect '%s' of type 'list'", (_param), (_name)); \
    }                                                                                                 \
}

#define VALIDATE_VALUE_RECORD_ARG(_value, _param, _name, _vm){                                          \
    if(!IS_VALUE_RECORD((_value))){                                                                     \
        vmu_error(_vm, "Illegal type of argument %d: expect '%s' of type 'record'", (_param), (_name)); \
    }                                                                                                   \
}

#define VALIDATE_VALUE_INT_RANGE_ARG(_value, _param, _name, _min, _max, _vm){                         \
    if(VALUE_TO_INT((_value)) < (_min)){                                                              \
        vmu_error(_vm, "Illegal value of argument '%s': cannot be less than %ld", (_name), (_min));   \
    }                                                                                                 \
    if(VALUE_TO_INT((_value)) > (_max)){                                                              \
        vmu_error(_vm, "Illegal value of argument '%s': cannot be greater than %d", (_name), (_max)); \
    }                                                                                                 \
}

#define VALIDATE_VALUE_ARRAY_SIZE_ARG(_value, _param, _name, _vm){                                             \
    VALIDATE_VALUE_INT_ARG(_value, _param, _name, _vm)                                                         \
    if(VALUE_TO_INT(_value) < 0){                                                                              \
        vmu_error((_vm), "Illegal argument %d: '%s' cannot be less than 0", (_param), (_name));                \
    }                                                                                                          \
    if(VALUE_TO_INT(_value) > ARRAY_LENGTH_MAX){                                                               \
        vmu_error((_vm), "Illegal argument %d: '%s' cannot be greater than %d", (_param), (_name), INT32_MAX); \
    }                                                                                                          \
}

#define VALIDATE_VALUE_ARRAY_INDEX_ARG(_value, _param, _name, _vm){                                            \
    if(VALUE_TO_INT(_value) < 0){                                                                              \
        vmu_error((_vm), "Illegal argument %d: '%s' cannot be less than 0", (_param), (_name));                \
    }                                                                                                          \
    if(VALUE_TO_INT(_value) > ARRAY_LENGTH_MAX - 1){                                                           \
        vmu_error((_vm), "Illegal argument %d: '%s' cannot be greater than %d", (_param), (_name), INT32_MAX); \
    }                                                                                                          \
}

#define VALIDATE_VALUE_INT(_value, _vm){                           \
    if(!IS_VALUE_INT((_value))){                                   \
        vmu_error(_vm, "Expect an 'int', but got something else"); \
    }                                                              \
}

#define VALIDATE_VALUE_ARRAY(_value, _vm){                           \
    if(!IS_VALUE_ARRAY(_value)){                                     \
        vmu_error(_vm, "Expect an 'array', but got something else"); \
    }                                                                \
}

#define VALIDATE_ARRAY_SIZE(_size, _vm){                                                                \
    if((int64_t)(_size) < 0){                                                                           \
        vmu_error(_vm, "Illegal size value for 'array': cannont be less than 0");                       \
    }                                                                                                   \
    if((_size) > ARRAY_LENGTH_MAX){                                                                     \
        vmu_error(_vm, "Illegal size value for 'array': cannont be greater than %d", ARRAY_LENGTH_MAX); \
    }                                                                                                   \
}

#define VALIDATE_STR_INDEX(_size, _value, _vm){                                                                               \
    if(!IS_VALUE_INT((_value))){                                                                                              \
        vmu_error((_vm), "Illegal type for 'str' index. Expect 'int', but got something else");                               \
    }                                                                                                                         \
    if(VALUE_TO_INT(_value) < 0){                                                                                             \
        vmu_error((_vm), "Illegal 'str' index value: cannot be less than 0");                                                 \
    }                                                                                                                         \
    if(VALUE_TO_INT(_value) > SIDX_T_MAX){                                                                                \
        vmu_error((_vm), "Illegal 'str' index value: cannot be greater than %d", SIDX_T_MAX);                             \
    }                                                                                                                         \
    if((STR_LENGTH_TYPE)VALUE_TO_INT(_value) >= (STR_LENGTH_TYPE)(_size)){                                                    \
        vmu_error((_vm), "Index %d out of bounds for %d", (STR_LENGTH_TYPE)(VALUE_TO_INT(_value)), (STR_LENGTH_TYPE)(_size)); \
    }                                                                                                                         \
}

#define VALIDATE_ARRAY_INDEX(_size, _value, _vm){                                                                                 \
    if(!IS_VALUE_INT((_value))){                                                                                                  \
        vmu_error((_vm), "Illegal type for 'array' index. Expect 'int', but got something else");                                 \
    }                                                                                                                             \
    if(VALUE_TO_INT(_value) < 0){                                                                                                 \
        vmu_error((_vm), "Illegal 'array' index value: cannot be less than 0");                                                   \
    }                                                                                                                             \
    if(VALUE_TO_INT(_value) > ARRAY_LENGTH_MAX){                                                                                  \
        vmu_error((_vm), "Illegal 'array' index value: cannot be greater than %d", ARRAY_LENGTH_MAX);                             \
    }                                                                                                                             \
    if((ARRAY_LENGTH_TYPE)VALUE_TO_INT(_value) >= (ARRAY_LENGTH_TYPE)(_size)){                                                    \
        vmu_error((_vm), "Index %d out of bounds for %d", (ARRAY_LENGTH_TYPE)(VALUE_TO_INT(_value)), (ARRAY_LENGTH_TYPE)(_size)); \
    }                                                                                                                             \
}

#define VALIDATE_LIST_INDEX(_size, _value, _vm){                                                                                \
    if(!IS_VALUE_INT((_value))){                                                                                                \
        vmu_error((_vm), "Illegal type for 'list' index. Expect 'int', but got something else");                                \
    }                                                                                                                           \
    if(VALUE_TO_INT((_value)) < 0){                                                                                             \
        vmu_error((_vm), "Illegal 'list' index value: cannot be less than 0");                                                  \
    }                                                                                                                           \
    if(VALUE_TO_INT((_value)) > LIST_LENGTH_MAX){                                                                               \
        vmu_error((_vm), "Illegal 'list' index value: cannot be greater than %d", LIST_LENGTH_MAX);                             \
    }                                                                                                                           \
    if((LIST_LENGTH_TYPE)VALUE_TO_INT((_value)) >= (LIST_LENGTH_TYPE)(_size)){                                                  \
        vmu_error((_vm), "Index %d out of bounds for %d", (LIST_LENGTH_TYPE)VALUE_TO_INT((_value)), (LIST_LENGTH_TYPE)(_size)); \
    }                                                                                                                           \
}

#define VALIDATE_I64_RANGE(_min, _max, _value, _param, _name, _vm){\
    if((int64_t)(_value) < (int64_t)(_min)){\
        vmu_error((_vm), "Illegal value for '%s': must be greater of equals to %ld, but got %ld", (_name), (int64_t)(_min), (int64_t)(_value));\
    }\
    if((_value) > (_max)){\
        vmu_error((_vm), "Illegal value for '%s': must be less of equals to %ld, but got %ld", (_name), (int64_t)(_max), (int64_t)(_value));\
    }\
}

#define TO_ARRAY_LENGTH(_value) ((ARRAY_LENGTH_TYPE)(VALUE_TO_INT((_value))))
#define TO_ARRAY_INDEX(_value) ((ARRAY_LENGTH_TYPE)(VALUE_TO_INT((_value))))
#define TO_LIST_INDEX(_value) ((LIST_LENGTH_TYPE)(VALUE_TO_INT((_value))))

#define CURRENT_FRAME(_vm)(&(_vm)->frame_stack[(_vm)->frame_ptr - 1])
#define CURRENT_LOCALS(_vm)(CURRENT_FRAME(_vm)->locals)
#define CURRENT_FN(_vm)(CURRENT_FRAME(_vm)->fn)
#define CURRENT_CHUNKS(_vm)(CURRENT_FN(_vm)->chunks)
#define CURRENT_LOCATIONS(vm)(CURRENT_FN(vm)->locations)
#define CURRENT_ICONSTS(_vm)(CURRENT_FN(_vm)->integers)
#define CURRENT_FCONSTS(_vm)(CURRENT_FN(_vm)->floats)

#define SELECTED_MODULE(_vm)((_vm)->modules[(_vm)->module_ptr - 1])

int vmu_error(VM *vm, char *msg, ...);
int vmu_internal_error(VM *vm, char *msg, ...);

static inline uint8_t validate_value_bool_arg(Value *value, uint8_t param, char *name, VM *vm){
    if(!IS_VALUE_BOOL(value)){
        vmu_error(vm, "Illegal type of argument %" PRIu8 ": expect '%s' of type 'bool'", param, name);
    }

    return VALUE_TO_BOOL(value);
}

static inline int64_t validate_value_int_arg(Value *value, uint8_t param, char *name, VM *vm){
    if(!IS_VALUE_INT(value)){
        vmu_error(vm, "Illegal type of argument %" PRIu8 ": expect '%s' of type 'int'", param, name);
    }

    return VALUE_TO_INT(value);
}

static inline double validate_value_float_arg(Value *value, uint8_t param, char *name, VM *vm){
    if(!IS_VALUE_FLOAT(value)){
        vmu_error(vm, "Illegal type of argument %" PRIu8 ": expect '%s' of type 'float'", param, name);
    }

    return VALUE_TO_FLOAT(value);
}

static inline double validate_value_ifloat_arg(Value *value, uint8_t param, char *name, VM *vm){
    if(IS_VALUE_INT(value)){
        return (double)VALUE_TO_INT(value);
    }

    if(IS_VALUE_FLOAT(value)){
        return VALUE_TO_FLOAT(value);
    }

    vmu_error(vm, "Illegal type of argument %" PRIu8 ": expect '%s' of type 'int' or 'float'", param, name);

    return -1;
}

static inline int64_t validate_value_int_range_arg(Value *value, uint8_t param, char *name, int64_t from, int64_t to, VM *vm){
    if(!IS_VALUE_INT(value)){
        vmu_error(vm, "Illegal type of argument %" PRIu8 ": expect '%s' of type 'int'", param, name);
    }

    int64_t i64_value = VALUE_TO_INT(value);

    if(i64_value < from){
        vmu_error(vm, "Illegal value of argument %" PRIu8 ": expect '%s' be greater or equals to %" PRId64 ", but got %" PRId64, param, name, from, i64_value);
    }

    if(i64_value > to){
        vmu_error(vm, "Illegal value of argument %" PRIu8 ": expect '%s' be less or equals to %" PRId64 ", but got %" PRId64, param, name, to, i64_value);
    }

    return i64_value;
}

static inline StrObj *validate_value_str_arg(Value *value, uint8_t param, char *name, VM *vm){
    if(!IS_VALUE_STR(value)){
        vmu_error(vm, "Illegal type of argument %" PRIu8 ": expect '%s' of type 'str'", param, name);
    }                                                                                                \

    return VALUE_TO_STR(value);
}

void vmu_clean_up(VM *vm);
void vmu_gc(VM *vm);

Frame *vmu_current_frame(VM *vm);
#define VMU_CURRENT_FN(_vm)(vmu_current_frame(_vm)->fn)
#define VMU_CURRENT_MODULE(_vm)(VMU_CURRENT_FN(_vm)->module)

uint32_t vmu_hash_obj(Obj *obj);
uint32_t vmu_hash_value(Value *value);
char *vmu_value_to_str(Value value, VM *vm, size_t *out_len);
void vmu_print_value(FILE *stream, Value *value);

Value *vmu_clone_value(Value value, VM *vm);
void vmu_destroy_value(Value *value, VM *vm);
//------------------    STRING    --------------------------//
int vmu_create_str(char runtime, size_t raw_str_len, char *raw_str, VM *vm, StrObj **out_str_obj);
void vmu_destroy_str(StrObj *str_obj, VM *vm);
int vmu_str_is_int(StrObj *str_obj);
int vmu_str_is_float(StrObj *str_obj);
int64_t vmu_str_len(StrObj *str_obj);
StrObj *vmu_str_char(int64_t idx, StrObj *str_obj, VM *vm);
int64_t vmu_str_code(int64_t idx, StrObj *str_obj, VM *vm);
StrObj *vmu_str_concat(StrObj *a_str_obj, StrObj *b_str_obj, VM *vm);
StrObj *vmu_str_mul(int64_t by, StrObj *str_obj, VM *vm);
StrObj *vmu_str_insert_at(int64_t idx, StrObj *a, StrObj *b, VM *vm);
StrObj *vmu_str_remove(int64_t from, int64_t to, StrObj *str_obj, VM *vm);
StrObj *vmu_str_sub_str(int64_t from, int64_t to, StrObj *str_obj, VM *vm);
//------------------    ARRAY    ---------------------------//
ArrayObj *vmu_create_array(int64_t len, VM *vm);
void vmu_destroy_array(ArrayObj *array_obj, VM *vm);
int64_t vmu_array_len(ArrayObj *array_obj);
Value vmu_array_get_at(int64_t idx, ArrayObj *array_obj, VM *vm);
void vmu_array_set_at(int64_t idx, Value value, ArrayObj *array_obj, VM *vm);
Value vmu_array_first(ArrayObj *array_obj, VM *vm);
Value vmu_array_last(ArrayObj *array_obj, VM *vm);
ArrayObj *vmu_array_grow(int64_t by, ArrayObj *array_obj, VM *vm);
ArrayObj *vmu_array_join(ArrayObj *a_array_obj, ArrayObj *b_array_obj, VM *vm);
//-------------------    LIST    ---------------------------//
ListObj *vmu_create_list(VM *vm);
void vmu_destroy_list(ListObj *list_obj, VM *vm);
int64_t vmu_list_len(ListObj *list_obj);
int64_t vmu_list_clear(ListObj *list_obj);
Value vmu_list_get_at(int64_t idx, ListObj *list_obj, VM *vm);
void vmu_list_insert(Value value, ListObj *list_obj, VM *vm);
Value vmu_list_set_at(int64_t idx, Value value, ListObj *list_obj, VM *vm);
Value vmu_list_remove(int64_t idx, ListObj *list_obj, VM *vm);
//-------------------    DICT    ---------------------------//
DictObj *vmu_create_dict(VM *vm);
void vmu_destroy_dict(DictObj *dict_obj, VM *vm);
void vmu_dict_put(Value key, Value value, DictObj *dict_obj, VM *vm);
void vmu_dict_raw_put_str_value(char *str, Value value, DictObj *dict_obj, VM *vm);
Value vmu_dict_get(Value key, DictObj *dict_obj, VM *vm);
//------------------    RECORD    --------------------------//
RecordObj *vmu_create_record(uint16_t length, VM *vm);
void vmu_destroy_record(RecordObj *record_obj, VM *vm);
void vmu_record_insert_attr(size_t key_size, char *key, Value value, RecordObj *record_obj, VM *vm);
void vmu_record_set_attr(size_t key_size, char *key, Value value, RecordObj *record_obj, VM *vm);
Value vmu_record_get_attr(size_t key_size, char *key, RecordObj *record_obj, VM *vm);
//----------------    NATIVE FN    -------------------------//
NativeFnObj *vmu_create_native_fn(Value target, NativeFn *native_fn, VM *vm);
void vmu_destroy_native_fn(NativeFnObj *native_fn_obj, VM *vm);
//--------------------    FN    ----------------------------//
FnObj *vmu_create_fn(Fn *fn, VM *vm);
void vmu_destroy_fn(FnObj *fn_obj, VM *vm);
//-----------------    CLOSURE    --------------------------//
ClosureObj *vmu_create_closure(MetaClosure *meta, VM *vm);
void vmu_destroy_closure(ClosureObj *closure_obj, VM *vm);
//--------------    NATIVE MODULE    -----------------------//
NativeModuleObj *vmu_create_native_module(NativeModule *native_module, VM *vm);
void vmu_destroy_native_module_obj(NativeModuleObj *native_module_obj, VM *vm);
//------------------    MODULE    --------------------------//
Obj *vmu_create_module_obj(Module *module, VM *vm);
void vmu_destroy_module_obj(ModuleObj *module_obj, VM *vm);

#endif
