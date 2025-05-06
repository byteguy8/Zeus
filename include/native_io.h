#ifndef NATIVE_IO_H
#define NATIVE_IO_H

#include "vmu.h"
#include <stdio.h>

NativeModule *io_module = NULL;

Value native_fn_io_read_file(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *path_value = &values[0];

    if(!IS_VALUE_STR(path_value)){
        vmu_error(vm, "Expect string at argument 1, but got something else");
    }

    Str *path = VALUE_TO_STR(path_value);
    char *raw_path = path->buff;

    size_t content_len = 0;
    char *content = NULL;
    size_t err_len = 1024;
    char err_str[err_len];

    if(utils_read_file(raw_path, &content_len, &content, err_len, err_str, vm->fake_allocator)){
        vmu_error(vm, err_str);
    }

    if(!content){
        vmu_error(vm, "Unexpected outcome");
    }

    Obj *content_obj = vmu_str_obj(&content, vm);

    return OBJ_VALUE(content_obj);
}

Value native_fn_io_readln(uint8_t argsc, Value *values, Value *target, VM *vm){
	size_t buff_len = 1024;
	char buff[buff_len];
	char *out_buff = fgets(buff, buff_len, stdin);

	if(!out_buff){
        vmu_error(vm, "Failed to read input");
    }

    size_t out_buff_len = strlen(out_buff);

    char *raw_str = factory_clone_raw_str_range(0, out_buff_len - 1, buff, vm->fake_allocator);
    Obj *str_obj = vmu_str_obj(&raw_str, vm);

	return OBJ_VALUE(str_obj);
}

void io_module_init(Allocator *allocator){
    io_module = factory_native_module("io", allocator);
    factory_add_native_fn("read_file", 1, native_fn_io_read_file, io_module, allocator);
    factory_add_native_fn("readln", 0, native_fn_io_readln, io_module, allocator);
}

#endif
