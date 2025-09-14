#ifndef NATIVE_OS_H
#define NATIVE_OS_H

#include "vmu.h"
#include "utils.h"

NativeModule *os_native_module = NULL;

Value native_fn_os_name(uint8_t argsc, Value *values, Value *target, void *context){
    StrObj *str_obj = NULL;
    vmu_create_str(1, strlen(OS_NAME), OS_NAME, VMU_VM, &str_obj);
    return OBJ_VALUE(str_obj);
}

Value native_fn_os_path_separator(uint8_t argsc, Value *values, Value *target, void *context){
    StrObj *str_obj = NULL;
    vmu_create_str(1, 1, (char[]){OS_PATH_SEPARATOR, 0}, VMU_VM, &str_obj);
    return OBJ_VALUE(str_obj);
}

void os_module_init(Allocator *allocator){
    os_native_module = factory_create_native_module("os", allocator);

    factory_add_native_fn("name", 0, native_fn_os_name, os_native_module, allocator);
    factory_add_native_fn("path_separator", 0, native_fn_os_path_separator, os_native_module, allocator);
}

#endif
