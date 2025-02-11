#ifndef NATIVE_OS_H
#define NATIVE_OS_H

#include "types.h"
#include "value.h"
#include "vm_utils.h"
#include "utils.h"

NativeModule *os_module = NULL;

Value native_fn_os_name(uint8_t argsc, Value *values, void *target, VM *vm){
    char *name = utils_sysname();
    
    if(!name){
        vm_utils_error(vm, "Failed to retrieve system name");
    }

    Obj *str_name_obj = vm_utils_clone_str_obj(name, NULL, vm);
    
    free(name);

    if(!str_name_obj){    
        vm_utils_error(vm, "Out of memory");
    }
    
    return OBJ_VALUE(str_name_obj);
}

void os_module_init(){
    os_module = runtime_native_module("os");
    runtime_add_native_fn("name", 0, native_fn_os_name, os_module);
}

#endif