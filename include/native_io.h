#ifndef NATIVE_IO_H
#define NATIVE_IO_H

#include "vmu.h"
#include "utils.h"
#include <stdio.h>
#include <errno.h>

NativeModule *io_module = NULL;

#define VALIDATE_VALUE_FILE_ARG(_value, _param, _name, _vm){                                                    \
    if(!IS_VALUE_RECORD((_value))){                                                                             \
        vmu_error(_vm, "Illegal type of argument %d: expect '%s' type to represent 'file'", (_param), (_name)); \
    }                                                                                                           \
    if(VALUE_TO_RECORD(_value)->type != FILE_RTYPE){                                                            \
        vmu_error(_vm, "Illegal type of argument %d: expect '%s' type to represent 'file'", (_param), (_name)); \
    }                                                                                                           \
}

#define VALUE_TO_FILE_MODE(_value)(VALUE_TO_RECORD(_value)->content.file.mode)
#define VALUE_TO_FILE_PATHNAME(_value)(VALUE_TO_RECORD(_value)->content.file.pathname)
#define VALUE_TO_FILE_HANDLER(_value)(VALUE_TO_RECORD(_value)->content.file.handler)

Value native_fn_io_can_read(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *file_value = &values[0];

    VALIDATE_VALUE_FILE_ARG(file_value, 1, "file", vm)

    char mode = VALUE_TO_FILE_MODE(file_value);
    FILE *handler = VALUE_TO_FILE_HANDLER(file_value);

    if(!handler){
        vmu_error(vm, "File is closed");
    }

    return BOOL_VALUE(FILE_CAN_READ(mode));
}

Value native_fn_io_can_write(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *file_value = &values[0];

    VALIDATE_VALUE_FILE_ARG(file_value, 1, "file", vm)

    char mode = VALUE_TO_FILE_MODE(file_value);
    FILE *handler = VALUE_TO_FILE_HANDLER(file_value);

    if(!handler){
        vmu_error(vm, "File is closed");
    }

    return BOOL_VALUE(FILE_CAN_WRITE(mode));
}

Value native_fn_io_can_append(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *file_value = &values[0];

    VALIDATE_VALUE_FILE_ARG(file_value, 1, "file", vm)

    char mode = VALUE_TO_FILE_MODE(file_value);
    FILE *handler = VALUE_TO_FILE_HANDLER(file_value);

    if(!handler){
        vmu_error(vm, "File is closed");
    }

    return BOOL_VALUE(FILE_CAN_APPEND(mode));
}

Value native_fn_io_is_binary(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *file_value = &values[0];

    VALIDATE_VALUE_FILE_ARG(file_value, 1, "file", vm)

    char mode = VALUE_TO_FILE_MODE(file_value);
    FILE *handler = VALUE_TO_FILE_HANDLER(file_value);

    if(!handler){
        vmu_error(vm, "File is closed");
    }

    return BOOL_VALUE(FILE_IS_BINARY(mode));
}

Value native_fn_io_is_open(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *file_value = &values[0];

    VALIDATE_VALUE_FILE_ARG(file_value, 1, "file", vm)

    FILE *handler = VALUE_TO_FILE_HANDLER(file_value);

    return BOOL_VALUE(handler != NULL);
}

Value native_fn_io_open_file(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *path_value = &values[0];
    Value *mode_value = &values[1];

    VALIDATE_VALUE_STR_EMPTY_ARG(path_value, 1, "path", vm)
    VALIDATE_VALUE_STR_EMPTY_ARG(mode_value, 2, "mode", vm)

    Str *path_str = VALUE_TO_STR(path_value);
    Str *mode_str = VALUE_TO_STR(mode_value);

    char *path_raw_str = path_str->buff;
    char *mode_raw_str = mode_str->buff;
    size_t mode_len = mode_str->len;

    VALIDATE_I64_RANGE(1, 3, mode_len, 2, "mode", vm)

    char mode = 0;
    char *pathname = factory_clone_raw_str(path_raw_str, vm->fake_allocator);

    for (size_t i = 0; i < mode_len; i++){
        char current_char = mode_raw_str[i];

        if(i == 0){
            switch (current_char){
                case 'r':
                    mode |= FILE_READ_MODE;
                    break;
                case 'w':
                    mode |= FILE_WRITE_MODE;
                    break;
                case 'a':
                    mode |= FILE_APPEND_MODE;
                    break;
                default:{
                    vmu_error(vm, "Illegal value of argument %d: expect '%s' value at index 0 of 'r', 'w' or 'a'", 2, "mode");
                    break;
                }
            }
        }else if(i == 1){
            switch (current_char){
                case '+':
                    mode |= FILE_PLUS_MODE;
                    break;
                case 'b':
                    mode |= FILE_BINARY_MODE;
                    break;
                default:{
                    vmu_error(vm, "Illegal value of argument %d: expect '%s' value at index 1 of '+' or 'b'", 2, "mode");
                    break;
                }
            }
        }else if(i == 2){
            switch (current_char){
                case '+':
                    mode |= FILE_PLUS_MODE;
                    break;
                case 'b':
                    mode |= FILE_BINARY_MODE;
                    break;
                default:{
                    vmu_error(vm, "Illegal value of argument %d: expect '%s' value at index 2 of '+' or 'b'", 2, "mode");
                    break;
                }
            }

            char previous_char = mode_raw_str[i - 1];

            if(previous_char == '+' && current_char != 'b'){
                vmu_error(vm, "Illegal value of argument %d: expect '%s' value 'b'", 2, "mode");
            }
            if(previous_char == 'b' && current_char != '+'){
                vmu_error(vm, "Illegal value of argument %d: expect '%s' value '+'", 2, "mode");
            }
        }
    }

    if(!FILE_CAN_WRITE(mode) && !FILE_CAN_APPEND(mode)){
        if(!UTILS_FILES_EXISTS(path_raw_str)){
            vmu_error(vm, "Illegal value of argument %d: '%s' value '%s' does not exist", 1, "path", path_raw_str);
        }

        if(utils_files_is_directory(path_raw_str)){
            vmu_error(vm, "Illegal value of argument %d: '%s' value is a directory '%s'", 1, "path", path_raw_str);
        }
    }

    FILE *handler = fopen(path_raw_str, mode_raw_str);

    if(!handler){
        vmu_error(vm, "Illegal value of argument %d: '%s': %s", strerror(errno));
    }

    Obj *record_obj = vmu_record_obj(0, vm);
    Record *record = OBJ_TO_RECORD(record_obj);

    record->type = FILE_RTYPE;
    record->content.file.mode = mode;
    record->content.file.pathname = pathname;
    record->content.file.handler = handler;

    return OBJ_VALUE(record_obj);
}

Value native_fn_io_close_file(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *file_value = &values[0];

    VALIDATE_VALUE_FILE_ARG(file_value, 1, "file", vm)

    Record *record = VALUE_TO_RECORD(file_value);
    FILE *handler = VALUE_TO_FILE_HANDLER(file_value);

    if(!handler){
        vmu_error(vm, "File already closed");
    }

    record->content.file.handler = NULL;

    fclose(handler);

    return EMPTY_VALUE;
}

Value native_fn_io_read_byte(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *file_value = &values[0];

    VALIDATE_VALUE_FILE_ARG(file_value, 1, "file", vm)

    char mode = VALUE_TO_FILE_MODE(file_value);
    char *pathname = VALUE_TO_FILE_PATHNAME(file_value);
    FILE *handler = VALUE_TO_FILE_HANDLER(file_value);

    if(!handler){
        vmu_error(vm, "File already closed");
    }

    if(!FILE_CAN_READ(mode)){
        vmu_error(vm, "File handler for '%s' is no opened for read", pathname);
    }

    int64_t byte = fgetc(handler);

    return INT_VALUE(byte == EOF ? -1 : byte);
}

Value native_fn_io_read_char(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *file_value = &values[0];

    VALIDATE_VALUE_FILE_ARG(file_value, 1, "file", vm)

    char mode = VALUE_TO_FILE_MODE(file_value);
    char *pathname = VALUE_TO_FILE_PATHNAME(file_value);
    FILE *handler = VALUE_TO_FILE_HANDLER(file_value);

    if(!handler){
        vmu_error(vm, "File already closed");
    }

    if(!FILE_CAN_READ(mode)){
        vmu_error(vm, "File handler for '%s' is no opened for read", pathname);
    }

    int64_t byte = fgetc(handler);

    if(byte == EOF){
        return INT_VALUE(-1);
    }

    char raw_str[] = {(char)byte, 0};
    char *cloned_raw_str = factory_clone_raw_str(raw_str, vm->fake_allocator);
    Obj *str_obj = vmu_str_obj(&cloned_raw_str, vm);

    return OBJ_VALUE(str_obj);
}

Value native_fn_io_write_byte(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *byte_value = &values[0];
    Value *file_value = &values[1];

    VALIDATE_VALUE_INT_ARG(byte_value, 1, "byte", vm)
    VALIDATE_I64_RANGE(0, 255, VALUE_TO_INT(byte_value), 1, "byte", vm)
    VALIDATE_VALUE_FILE_ARG(file_value, 2, "file", vm)

    int64_t byte = VALUE_TO_INT(byte_value);
    char mode = VALUE_TO_FILE_MODE(file_value);
    char *pathname = VALUE_TO_FILE_PATHNAME(file_value);
    FILE *handler = VALUE_TO_FILE_HANDLER(file_value);

    if(!handler){
        vmu_error(vm, "File already closed");
    }

    if(!FILE_CAN_WRITE(mode)){
        vmu_error(vm, "File handler for '%s' is no opened for write", pathname);
    }

    if(fputc((int)byte, handler) == EOF){
        vmu_error(vm, "Failed to write byte");
    }

    return EMPTY_VALUE;
}

Value native_fn_io_write_str(uint8_t argsc, Value *values, Value *target, VM *vm){
    Value *str_value = &values[0];
    Value *file_value = &values[1];

    VALIDATE_VALUE_STR_ARG(str_value, 1, "str", vm)
    VALIDATE_VALUE_FILE_ARG(file_value, 2, "file", vm)

    Str *str = VALUE_TO_STR(str_value);
    char mode = VALUE_TO_FILE_MODE(file_value);
    char *pathname = VALUE_TO_FILE_PATHNAME(file_value);
    FILE *handler = VALUE_TO_FILE_HANDLER(file_value);

    if(!handler){
        vmu_error(vm, "File already closed");
    }

    if(!FILE_CAN_WRITE(mode)){
        vmu_error(vm, "File handler for '%s' is no opened for write", pathname);
    }

    if(fputs(str->buff, handler) == EOF){
        vmu_error(vm, "Failed to write 'str'");
    }

    return EMPTY_VALUE;
}

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

void io_module_init(Allocator *allocator){
    io_module = factory_native_module("io", allocator);

    factory_add_native_fn("can_read", 1, native_fn_io_can_read, io_module, allocator);
    factory_add_native_fn("can_write", 1, native_fn_io_can_write, io_module, allocator);
    factory_add_native_fn("can_append", 1, native_fn_io_can_append, io_module, allocator);
    factory_add_native_fn("is_binary", 1, native_fn_io_is_binary, io_module, allocator);
    factory_add_native_fn("is_open", 1, native_fn_io_is_open, io_module, allocator);

    factory_add_native_fn("open", 2, native_fn_io_open_file, io_module, allocator);
    factory_add_native_fn("close", 1, native_fn_io_close_file, io_module, allocator);
    factory_add_native_fn("read_byte", 1, native_fn_io_read_byte, io_module, allocator);
    factory_add_native_fn("read_char", 1, native_fn_io_read_char, io_module, allocator);
    factory_add_native_fn("write_byte", 2, native_fn_io_write_byte, io_module, allocator);
    factory_add_native_fn("write_str", 2, native_fn_io_write_str, io_module, allocator);

    factory_add_native_fn("read_file", 1, native_fn_io_read_file, io_module, allocator);
}

#endif
