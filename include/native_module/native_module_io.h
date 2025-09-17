#ifndef NATIVE_IO_H
#define NATIVE_IO_H

#include "vmu.h"
#include "utils.h"
#include <stdio.h>
#include <errno.h>

NativeModule *io_native_module = NULL;

Value native_fn_io_read_text(uint8_t argsc, Value *values, Value target, void *context){
    StrObj *pathname_str_obj = validate_value_str_arg(values[0], 1, "pathname", VMU_VM);
    char *pathname = pathname_str_obj->buff;

    if(!UTILS_FILES_CAN_READ(pathname)){
		fprintf(stderr, "File at '%s' do not exists or cannot be read\n", pathname);
		exit(EXIT_FAILURE);
	}

	if(!utils_files_is_regular(pathname)){
		fprintf(stderr, "File at '%s' is not a regular file\n", pathname);
		exit(EXIT_FAILURE);
	}

    size_t content_len;
    char *content_buff = utils_read_file_as_text(pathname, VMU_NATIVE_FRONT_ALLOCATOR, &content_len);
    StrObj *content_str_obj = NULL;

    if(vmu_create_str(1, content_len, content_buff, VMU_VM, &content_str_obj)){
        MEMORY_DEALLOC(char, content_len + 1, content_buff, VMU_NATIVE_FRONT_ALLOCATOR);
    }

    return OBJ_VALUE(content_str_obj);
}

void io_module_init(Allocator *allocator){
    io_native_module = factory_create_native_module("io", allocator);

    factory_add_native_fn("read_text", 1, native_fn_io_read_text, io_native_module);
}

#endif
