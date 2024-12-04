#ifndef NATIVE_IO_H
#define NATIVE_IO_H

#include "types.h"
#include "value.h"
#include "vm_utils.h"
#include <stdio.h>

Value native_readln(uint8_t argc, Value *values, void *target, VM *vm){
	size_t buff_len = 1024;
	char buff[buff_len];
	char *out_buff = fgets(buff, buff_len, stdin);

	if(!out_buff)
		vm_utils_error(vm, "Failed to read input");

	Value value = {0};

	if(!vm_utils_clone_str_obj(buff, &value, vm))
		vm_utils_error(vm, "Out of memory");

	return value;
}

#endif
