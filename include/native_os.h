#ifndef NATIVE_OS_H
#define NATIVE_OS_H

#include "vmu.h"
#include "utils.h"

NativeModule *os_module = NULL;

Value native_fn_os_name(uint8_t argsc, Value *values, Value *target, VM *vm){
    char *name = utils_files_sysname(vm->fake_allocator);
    Obj *str_name_obj = vmu_str_obj(&name, vm);

    return OBJ_VALUE(str_name_obj);
}

void os_module_init(Allocator *allocator){
    os_module = factory_native_module("os", allocator);
    factory_add_native_fn("name", 0, native_fn_os_name, os_module, allocator);
}

#endif
