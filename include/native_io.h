#ifndef NATIVE_IO_H
#define NATIVE_IO_H

#include "types.h"
#include "value.h"
#include "vm_utils.h"
#include <stdio.h>

NativeModule *io_module = NULL;

Value native_fn_io_read_file(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *path_value = &values[0];

    if(!IS_STR(path_value)){
        vmu_error(vm, "Expect string at argument 1, but got something else");
    }

    Str *path = TO_STR(path_value);
    char *raw_path = path->buff;

    size_t content_len = 0;
    char *content = NULL;
    size_t err_len = 1024;
    char err_str[err_len];
    
    if(utils_read_file(raw_path, &content_len, &content, err_len, err_str, memory_allocator())){
        vmu_error(vm, err_str);
    }

    if(!content){
        vmu_error(vm, "Unexpected outcome");
    }

    Obj *content_obj = vmu_uncore_str_obj(content, vm);

    return OBJ_VALUE(content_obj);
}

Value native_fn_io_readln(uint8_t argsc, Value *values, void *target, VM *vm){
	size_t buff_len = 1024;
	char buff[buff_len];
	char *out_buff = fgets(buff, buff_len, stdin);

	if(!out_buff)
		vmu_error(vm, "Failed to read input");

    Value value = {0};
    size_t out_buff_len = strlen(out_buff);
    
    if(out_buff_len == 0){
        if(!vmu_empty_str_obj(&value, vm))
            vmu_error(vm, "Out of memory");
    }if(out_buff_len == 1 &&  out_buff[out_buff_len - 1] == '\n'){
        if(!vmu_empty_str_obj(&value, vm))
            vmu_error(vm, "Out of memory");
    }else{
        if(out_buff[out_buff_len - 1] == '\n'){
            if(!vmu_range_str_obj(0, out_buff_len - 2, buff, &value, vm))
                vmu_error(vm, "Out of memory");
        }else{
            if(!vmu_clone_str_obj(buff, &value, vm))
		        vmu_error(vm, "Out of memory");
        }
    }

	return value;
}

void io_module_init(){
    io_module = runtime_native_module("io");
    runtime_add_native_fn("read_file", 1, native_fn_io_read_file, io_module);
    runtime_add_native_fn("readln", 0, native_fn_io_readln, io_module);
}

#endif
