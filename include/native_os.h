#ifndef NATIVE_OS_H
#define NATIVE_OS_H

#include "rtypes.h"
#include "vm_utils.h"
#include "utils.h"

NativeModule *os_module = NULL;

Value native_fn_os_name(uint8_t argsc, Value *values, void *target, VM *vm){
    Allocator *allocator = memory_allocator();
    char *name = utils_sysname(allocator);
    
    if(!name){
        vmu_error(vm, "Failed to retrieve system name");
    }

    size_t name_len = strlen(name);
    Obj *str_name_obj = vmu_clone_str_obj(name, NULL, vm);
    
    allocator->dealloc(name, name_len, allocator->ctx);

    if(!str_name_obj){    
        vmu_error(vm, "Out of memory");
    }
    
    return OBJ_VALUE(str_name_obj);
}

void os_module_init(){
    os_module = runtime_native_module("os");
    runtime_add_native_fn("name", 0, native_fn_os_name, os_module);
}

#endif