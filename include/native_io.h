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
        vm_utils_error(vm, "Expect string at argument 1, but got something else");
    }

    Str *path = TO_STR(path_value);
    char *raw_path = path->buff;

    size_t content_len = 0;
    char *content = NULL;
    size_t err_len = 1024;
    char err_str[err_len];
    
    if(utils_read_file(raw_path, &content_len, &content, err_len, err_str, vm->allocator)){
        vm_utils_error(vm, err_str);
    }

    if(!content){
        vm_utils_error(vm, "Unexpected outcome");
    }

    Obj *content_obj = vm_utils_uncore_str_obj(content, vm);

    return OBJ_VALUE(content_obj);
}

Value native_fn_io_readln(uint8_t argsc, Value *values, void *target, VM *vm){
	size_t buff_len = 1024;
	char buff[buff_len];
	char *out_buff = fgets(buff, buff_len, stdin);

	if(!out_buff)
		vm_utils_error(vm, "Failed to read input");

    Value value = {0};
    size_t out_buff_len = strlen(out_buff);
    
    if(out_buff_len == 0){
        if(!vm_utils_empty_str_obj(&value, vm))
            vm_utils_error(vm, "Out of memory");
    }if(out_buff_len == 1 &&  out_buff[out_buff_len - 1] == '\n'){
        if(!vm_utils_empty_str_obj(&value, vm))
            vm_utils_error(vm, "Out of memory");
    }else{
        if(out_buff[out_buff_len - 1] == '\n'){
            if(!vm_utils_range_str_obj(0, out_buff_len - 2, buff, &value, vm))
                vm_utils_error(vm, "Out of memory");
        }else{
            if(!vm_utils_clone_str_obj(buff, &value, vm))
		        vm_utils_error(vm, "Out of memory");
        }
    }

	return value;
}

Value native_fn_io_prt(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *arg0 = &values[0];
    BStr *bstr = bstr_create_empty(NULL);
    
    if(!bstr || vm_utils_value_to_str(arg0, bstr)){
        bstr_destroy(bstr);
        vm_utils_error(vm, "Out of memory");
    }

    printf("%s", bstr->buff);

    bstr_destroy(bstr);
    
    return EMPTY_VALUE;
}

Value native_fn_io_prterr(uint8_t argsc, Value *values, void *target, VM *vm){
    Value *arg0 = &values[0];
    BStr *bstr = bstr_create_empty(NULL);
    
    if(!bstr || vm_utils_value_to_str(arg0, bstr)){
        bstr_destroy(bstr);
        vm_utils_error(vm, "Out of memory");
    }

    fprintf(stderr, "%s", bstr->buff);

    bstr_destroy(bstr);
    
    return EMPTY_VALUE;
}

void io_module_init(){
    io_module = runtime_native_module("io");
    runtime_add_native_fn("read_file", 1, native_fn_io_read_file, io_module);
    runtime_add_native_fn("readln", 0, native_fn_io_readln, io_module);
    runtime_add_native_fn("prt", 1, native_fn_io_prt, io_module);
    runtime_add_native_fn("prterr", 1, native_fn_io_prterr, io_module);
}

#endif
