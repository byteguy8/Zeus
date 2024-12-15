#ifndef NATIVE_IO_H
#define NATIVE_IO_H

#include "types.h"
#include "value.h"
#include "vm_utils.h"
#include <stdio.h>

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

#endif
