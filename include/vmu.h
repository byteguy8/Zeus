#ifndef VM_UTILS_H
#define VM_UTILS_H

#include "vm.h"
#include "bstr.h"
#include <stdio.h>

#define ARRAY_LENGTH_TYPE int32_t
#define LIST_LENGTH_TYPE int32_t

#define VALIDATE_ARRAY_INDEX_ARG(param, name, value, vm){                                                   \
    if(TO_INT(value) < 0){                                                                                  \
        vmu_error((vm), "Illegal argument %d: '%s' cannot be less than 0", (param), (name));                \
    }                                                                                                       \
    if(TO_INT(value) > INT32_MAX){                                                                          \
        vmu_error((vm), "Illegal argument %d: '%s' cannot be greater than %d", (param), (name), INT32_MAX); \
    }                                                                                                       \
}

#define VALIDATE_VALUE_INT_ARG(value, param, name, vm){                                           \
    if(!IS_INT((value))){                                                                         \
        vmu_error(vm, "Illegal type of argument %d: expect '%s' of type 'int'", (param), (name)); \
    }                                                                                             \
}

#define VALIDATE_VALUE_INT(_value, _vm){                          \
    if(!IS_INT((_value))){                                        \
        vmu_error(vm, "Expect an 'int', but got something else"); \
    }                                                             \
}

#define VALIDATE_VALUE_ARRAY(_value, _vm){                          \
    if(!IS_ARRAY(_value)){                                          \
        vmu_error(vm, "Expect an 'array', but got something else"); \
    }                                                               \
}

#define VALIDATE_ARRAY_SIZE(_size){\
    if((_size) < 0){\
        vmu_error(vm, "Illegal size value for array: cannont be less than 0");                \
    }                                                                                         \
    if((_size) > INT32_MAX){                                                                   \
        vmu_error(vm, "Illegal size value for array: cannont be greater than %d", INT32_MAX); \
    }                                                                                         \
}

#define VALIDATE_ARRAY_INDEX(_size, _value, _vm){                                                                           \
    if(!IS_INT((_value))){                                                                                                  \
        vmu_error((_vm), "Illegal type for array index. Expect 'int', but got something else");                             \
    }                                                                                                                       \
    if(TO_INT(_value) < 0){                                                                                                 \
        vmu_error((_vm), "Illegal array index value: cannot be less than 0");                                               \
    }                                                                                                                       \
    if(TO_INT(_value) > INT32_MAX){                                                                                         \
        vmu_error((_vm), "Illegal array index value: cannot be greater than %d", INT32_MAX);                                \
    }                                                                                                                       \
    if((ARRAY_LENGTH_TYPE)TO_INT(_value) > (ARRAY_LENGTH_TYPE)(_size)){                                                     \
        vmu_error((_vm), "Index %d out of bounds for %d", (ARRAY_LENGTH_TYPE)(TO_INT(_value)), (ARRAY_LENGTH_TYPE)(_size)); \
    }                                                                                                                       \
}

#define VALIDATE_LIST_INDEX(_size, _value, _vm){                                                                          \
    if(!IS_INT((_value))){                                                                                                \
        vmu_error((_vm), "Illegal type for list index. Expect 'int', but got something else");                            \
    }                                                                                                                     \
    if(TO_INT((_value)) < 0){                                                                                             \
        vmu_error((_vm), "Illegal list index value: cannot be less than 0");                                              \
    }                                                                                                                     \
    if(TO_INT((_value)) > INT32_MAX){                                                                                     \
        vmu_error((_vm), "Illegal list index value: cannot be greater than %d", INT32_MAX);                               \
    }                                                                                                                     \
    if((LIST_LENGTH_TYPE)TO_INT((_value)) > (LIST_LENGTH_TYPE)(_size)){                                                   \
        vmu_error((_vm), "Index %d out of bounds for %d", (LIST_LENGTH_TYPE)TO_INT((_value)), (LIST_LENGTH_TYPE)(_size)); \
    }                                                                                                                     \
}

#define TO_ARRAY_LENGTH(_value) ((ARRAY_LENGTH_TYPE)((_value)->content.i64))
#define TO_ARRAY_INDEX(_value) ((ARRAY_LENGTH_TYPE)((_value)->content.i64))
#define TO_LIST_INDEX(_value) ((LIST_LENGTH_TYPE)((_value)->content.i64))

#define CURRENT_FRAME(vm)(&(vm)->frame_stack[(vm)->frame_ptr - 1])
#define CURRENT_LOCALS(vm)(CURRENT_FRAME(vm)->locals)
#define CURRENT_FN(vm)(CURRENT_FRAME(vm)->fn)
#define CURRENT_CHUNKS(vm)(CURRENT_FN(vm)->chunks)
#define CURRENT_CONSTANTS(vm)(CURRENT_FN(vm)->integers)
#define CURRENT_FLOAT_VALUES(vm)(CURRENT_FN(vm)->floats)
#define CURRENT_MODULE(vm)((vm)->modules[(vm)->module_ptr - 1])

void vmu_error(VM *vm, char *msg, ...);

uint32_t vmu_hash_obj(Obj *obj);
uint32_t vmu_hash_value(Value *value);

int vmu_obj_to_str(Obj *obj, BStr *bstr);
int vmu_value_to_str(Value *value, BStr *bstr);

void vmu_print_obj(FILE *stream, Obj *obj);
void vmu_print_value(FILE *stream, Value *value);

void vmu_clean_up(VM *vm);

uint32_t vmu_raw_str_to_table(char **raw_str, VM *vm, char **out_raw_str);

Value *vmu_clone_value(Value *value, VM *vm);
void vmu_destroy_value(Value *value, VM *vm);

GlobalValue *vmu_global_value(VM *vm);
void vmu_destroy_global_value(GlobalValue *global_value, VM *vm);

Obj *vmu_create_obj(ObjType type, VM *vm);
void vmu_destroy_obj(Obj *obj, VM *vm);

Obj *vmu_str_obj(char **raw_str_ptr, VM *vm);
Obj *vmu_unchecked_str_obj(char *raw_str, VM *vm);
Obj *vmu_array_obj(int32_t len, VM *vm);
Obj *vmu_list_obj(VM *vm);
Obj *vmu_dict_obj(VM *vm);
Obj *vmu_record_obj(uint8_t length, VM *vm);
Obj *vmu_native_fn_obj(int arity, char *name, Value *target, RawNativeFn raw_native, VM *vm);
Obj *vmu_closure_obj(MetaClosure *meta, VM *vm);
Obj *vmu_foreign_lib_obj(void *handler, VM *vm);

#define VALIDATE_INDEX_ARG(name, value, index, len){                                                \
    if(!IS_INT((value))){                                                                           \
        vmu_error(vm, "Argument '%s' must be of type 'int'", (name));                               \
    }                                                                                               \
    (index) = TO_INT((value));                                                                      \
    if((index) < 0){                                                                                \
        vmu_error(vm, "Illegal value for argument '%s': indexes must be greater than 0", (name));   \
    }                                                                                               \
    if((index) >= (int64_t)(len)){                                                                  \
        vmu_error(vm, "Illegal value for argument '%s': cannot be greater than %d", (name), (len)); \
    }                                                                                               \
}

#define VALIDATE_INDEX(value, index, len){                                                        \
    if(!IS_INT((value))){                                                                         \
        vmu_error(vm, "Unexpected index type. Expect 'int', but got something else");             \
    }                                                                                             \
    index = TO_INT((value));                                                                      \
    if((index) < 0 || (index) >= (int64_t)(len)){                                                 \
        vmu_error(vm, "Index out of bounds. Must be 0 >= index(%ld) < len(%ld)", (index), (len)); \
    }                                                                                             \
}

#define VALIDATE_INDEX_NAME(value, index, len, name){                                                    \
    if(!IS_INT((value))){                                                                                \
        vmu_error(vm, "Unexpected type for '%s'. Expect 'int', but got something else", (name));         \
    }                                                                                                    \
    index = TO_INT((value));                                                                             \
    if((index) < 0 || (index) >= (int64_t)(len)){                                                        \
        vmu_error(vm, "'%s' out of bounds. Must be 0 >= index(%ld) < len(%ld)", (name), (index), (len)); \
    }                                                                                                    \
}

#endif
