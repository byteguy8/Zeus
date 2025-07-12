#ifndef VM_UTILS_H
#define VM_UTILS_H

#include "vm.h"
#include "bstr.h"
#include "native_fn.h"
#include "tutils.h"
#include <stdio.h>

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
    if(VALUE_TO_INT(_value) > STR_LENGTH_MAX){                                                                                \
        vmu_error((_vm), "Illegal 'str' index value: cannot be greater than %d", STR_LENGTH_MAX);                             \
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

#define CURRENT_FRAME(vm)(&(vm)->frame_stack[(vm)->frame_ptr - 1])
#define CURRENT_LOCALS(vm)(CURRENT_FRAME(vm)->locals)
#define CURRENT_FN(vm)(CURRENT_FRAME(vm)->fn)
#define CURRENT_CHUNKS(vm)(CURRENT_FN(vm)->chunks)
#define CURRENT_CONSTANTS(vm)(CURRENT_FN(vm)->integers)
#define CURRENT_FLOAT_VALUES(vm)(CURRENT_FN(vm)->floats)
#define CURRENT_MODULE(vm)((vm)->modules[(vm)->module_ptr - 1])

void vmu_error(VM *vm, char *msg, ...);

uint32_t vmu_hash_obj(ObjHeader *obj);
uint32_t vmu_hash_value(Value *value);

int vmu_obj_to_str(ObjHeader *obj, BStr *bstr);
int vmu_value_to_str(Value *value, BStr *bstr);

void vmu_print_obj(FILE *stream, ObjHeader *obj);
void vmu_print_value(FILE *stream, Value *value);

void vmu_clean_up(VM *vm);
void vmu_gc(VM *vm);

uint32_t vmu_raw_str_to_table(char **raw_str, VM *vm, char **out_raw_str);

Value *vmu_clone_value(Value *value, VM *vm);
void vmu_destroy_value(Value *value, VM *vm);

GlobalValue *vmu_global_value(VM *vm);
void vmu_destroy_global_value(GlobalValue *global_value, VM *vm);

ObjHeader *vmu_create_str_obj(char **raw_str_ptr, VM *vm);
ObjHeader *vmu_create_str_cpy_obj(char *raw_str, VM *vm);
ObjHeader *vmu_create_unchecked_str_obj(char *raw_str, VM *vm);
void vmu_destroy_str_obj(StrObj *str_obj, VM *vm);

ObjHeader *vmu_create_array_obj(aidx_t len, VM *vm);
void vmu_destroy_array_obj(ArrayObj *array_obj, VM *vm);

ObjHeader *vmu_create_list_obj(VM *vm);
void vmu_destroy_list_obj(ListObj *list_obj, VM *vm);

ObjHeader *vmu_create_dict_obj(VM *vm);
void vmu_destroy_dict_obj(DictObj *dict_obj, VM *vm);

ObjHeader *vmu_create_record_obj(uint8_t length, VM *vm);
RecordRandom *vmu_create_record_random(VM *vm);
RecordFile *vmu_create_record_file(char *raw_mode, char mode, char *pathname, VM *vm);
ObjHeader *vmu_create_record_random_obj(VM *vm);
ObjHeader *vmu_create_record_file_obj(char *raw_mode, char mode, char *pathname, VM *vm);
void vmu_destroy_record_obj(RecordObj *record_obj, VM *vm);
void vmu_destroy_record_random(RecordRandom *record_random, VM *vm);
void vmu_destroy_record_file(RecordFile *record_file, VM *vm);

ObjHeader *vmu_create_raw_native_fn_obj(int arity, char *name, Value *target, RawNativeFn raw_native, VM *vm);
ObjHeader *vmu_create_native_fn_obj(NativeFn *native_fn, VM *vm);
void vmu_destroy_native_fn_obj(NativeFnObj *native_fn_obj, VM *vm);

ObjHeader *vmu_create_fn_obj(Fn *fn, VM *vm);
void vmu_destroy_fn_obj(FnObj *fn_obj, VM *vm);

ObjHeader *vmu_closure_obj(MetaClosure *meta, VM *vm);
void vmu_destroy_closure_obj(ClosureObj *closure_obj, VM *vm);

ObjHeader *vmu_create_native_module_obj(NativeModule *native_module, VM *vm);
void vmu_destroy_native_module_obj(NativeModuleObj *native_module_obj, VM *vm);

ObjHeader *vmu_create_module_obj(Module *module, VM *vm);
void vmu_destroy_module_obj(ModuleObj *module_obj, VM *vm);

void vmu_insert_obj(ObjHeader *obj, ObjHeader **raw_head, ObjHeader **raw_tail);
void vmu_remove_obj(ObjHeader *obj, ObjHeader **raw_head, ObjHeader **raw_tail);

#define ACKNOWLEDGE_OBJ(_obj, _vm)(vmu_insert_obj((_obj), (ObjHeader **)&(_vm)->red_head, (ObjHeader **)&(_vm)->red_tail))

#endif
