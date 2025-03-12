#ifndef VM_UTILS_H
#define VM_UTILS_H

#include "vm.h"
#include "bstr.h"
#include <stdio.h>

void vmu_error(VM *vm, char *msg, ...);

void *vmu_alloc(size_t size);
void *vmu_realloc(void *ptr, size_t new_size);
void vmu_dealloc(void *ptr);

uint32_t vmu_hash_obj(Obj *obj);
uint32_t vmu_hash_value(Value *value);

int vmu_obj_to_str(Obj *obj, BStr *bstr);
int vmu_value_to_str(Value *value, BStr *bstr);

void vmu_print_obj(FILE *stream, Obj *obj);
void vmu_print_value(FILE *stream, Value *value);

void vmu_clean_up(VM *vm);

char *vmu_clone_buff(char *buff, VM *vm);
char *vmu_join_buff(char *buffa, size_t sza, char *buffb, size_t szb, VM *vm);
char *vmu_multiply_buff(char *buff, size_t szbuff, size_t by, VM *vm);
Value *vmu_clone_value(Value *value, VM *vm);

Obj *vmu_obj(ObjType type, VM *vm);
Obj *vmu_core_str_obj(char *buff, VM *vm);
Obj *vmu_uncore_str_obj(char *buff, VM *vm);
Obj *vmu_empty_str_obj(Value *out_value, VM *vm);
Obj *vmu_clone_str_obj(char *buff, Value *out_value, VM *vm);
Obj *vmu_range_str_obj(size_t from, size_t to, char *buff, Value *out_value, VM *vm);
Obj *vmu_array_obj(int32_t len, VM *vm);
Obj *vmu_list_obj(VM *vm);
Obj *vmu_dict_obj(VM *vm);
Obj *vmu_record_obj(uint8_t length, VM *vm);
Obj *vmu_native_fn_obj(
    int arity,
    char *name,
    void *target,
    RawNativeFn raw_native,
    VM *vm
);
Obj *vmu_native_lib_obj(void *handler, VM *vm);

NativeFn *vmu_native_function(
    int arity,
    char *name,
    void *target,
    RawNativeFn native,
    VM *vm
);

#define VALIDATE_INDEX(value, index, len){                                                         \
    if(!IS_INT((value))){                                                                          \
        vmu_error(vm, "Unexpected index value. Expect int, but got something else");         \
    }                                                                                              \
    index = TO_INT((value));                                                                       \
    if((index) < 0 || (index) >= (int64_t)(len)){                                                        \
        vmu_error(vm, "Index out of bounds. Must be 0 >= index(%ld) < len(%ld)", (index), (len)); \
    }                                                                                              \
}
    
#define VALIDATE_INDEX_NAME(value, index, len, name){                                                   \
    if(!IS_INT((value))){                                                                               \
        vmu_error(vm, "Unexpected value for '%s'. Expect int, but got something else", (name));      \
    }                                                                                                   \
    index = TO_INT((value));                                                                            \
    if((index) < 0 || (index) >= (int64_t)(len)){                                                             \
        vmu_error(vm, "'%s' out of bounds. Must be 0 >= index(%ld) < len(%ld)", (name), (index), (len)); \
    }                                                                                                   \
}

#endif
