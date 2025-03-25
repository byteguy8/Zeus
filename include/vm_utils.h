#ifndef VM_UTILS_H
#define VM_UTILS_H

#include "vm.h"
#include "bstr.h"
#include <stdio.h>

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
Obj *vmu_array_obj(int32_t len, VM *vm);
Obj *vmu_list_obj(VM *vm);
Obj *vmu_dict_obj(VM *vm);
Obj *vmu_record_obj(uint8_t length, VM *vm);
Obj *vmu_native_fn_obj(int arity, char *name, void *target, RawNativeFn raw_native, VM *vm);
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
