#ifndef NATIVE_OS_H
#define NATIVE_OS_H

#include "utils.h"

#include "vm/vm_factory.h"
#include "vm/vmu.h"

static char *os_name = OS_NAME;
static char *path_separator = (char[]){OS_PATH_SEPARATOR, 0};

NativeModule *os_native_module = NULL;

Value native_fn_os_name(uint8_t argsc, Value *values, Value target, void *context){
    StrObj *str_obj = NULL;

    vmu_create_str(
        0,
        strlen(os_name),
        os_name,
        VMU_VM,
        &str_obj
    );

    return OBJ_VALUE(str_obj);
}

Value native_fn_os_path_separator(uint8_t argsc, Value *values, Value target, void *context){
    StrObj *str_obj = NULL;

    vmu_create_str(
        0,
        1,
        path_separator,
        VMU_VM,
        &str_obj
    );

    return OBJ_VALUE(str_obj);
}

void os_module_init(const Allocator *allocator){
    os_native_module = vm_factory_native_module_create(allocator, "os");

    vm_factory_native_module_add_native_fn(os_native_module, "name", 0, native_fn_os_name);
    vm_factory_native_module_add_native_fn(os_native_module, "path_separator", 0, native_fn_os_path_separator);
}

#endif
