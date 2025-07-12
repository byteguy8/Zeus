#ifndef NATIVE_IO_H
#define NATIVE_IO_H

#include "vmu.h"
#include "utils.h"
#include <stdio.h>
#include <errno.h>

static NativeModule *io_module = NULL;

#define VALIDATE_VALUE_FILE_ARG(_value, _param, _name, _vm){                                                    \
    if(!IS_VALUE_RECORD((_value))){                                                                             \
        vmu_error(_vm, "Illegal type of argument %d: expect '%s' type to represent 'file'", (_param), (_name)); \
    }                                                                                                           \
    if(VALUE_TO_RECORD(_value)->type != FILE_RTYPE){                                                            \
        vmu_error(_vm, "Illegal type of argument %d: expect '%s' type to represent 'file'", (_param), (_name)); \
    }                                                                                                           \
}

#define VALUE_TO_FILE_MODE(_value)(RECORD_FILE_MODE(VALUE_TO_RECORD(_value)))
#define VALUE_TO_FILE_PATHNAME(_value)(RECORD_FILE_PATHNAME(VALUE_TO_RECORD(_value)))
#define VALUE_TO_FILE_HANDLER(_value)(RECORD_FILE_HANDLER(VALUE_TO_RECORD(_value)))

Value native_fn_io_is_file(uint8_t argsc, Value *values, Value *target, void *context){
    Value *file_value = &values[0];
    return BOOL_VALUE(IS_VALUE_RECORD(file_value) && VALUE_TO_RECORD(file_value)->type == FILE_RTYPE);
}

Value native_fn_io_can_read(uint8_t argsc, Value *values, Value *target, void *context){
    Value *file_value = &values[0];

    VALIDATE_VALUE_FILE_ARG(file_value, 1, "file", context)

    char mode = VALUE_TO_FILE_MODE(file_value);
    FILE *handler = VALUE_TO_FILE_HANDLER(file_value);

    if(!handler){
        vmu_error(context, "File is closed");
    }

    return BOOL_VALUE(FILE_CAN_READ(mode));
}

Value native_fn_io_can_write(uint8_t argsc, Value *values, Value *target, void *context){
    Value *file_value = &values[0];

    VALIDATE_VALUE_FILE_ARG(file_value, 1, "file", context)

    char mode = VALUE_TO_FILE_MODE(file_value);
    FILE *handler = VALUE_TO_FILE_HANDLER(file_value);

    if(!handler){
        vmu_error(context, "File is closed");
    }

    return BOOL_VALUE(FILE_CAN_WRITE(mode));
}

Value native_fn_io_can_append(uint8_t argsc, Value *values, Value *target, void *context){
    Value *file_value = &values[0];

    VALIDATE_VALUE_FILE_ARG(file_value, 1, "file", context)

    char mode = VALUE_TO_FILE_MODE(file_value);
    FILE *handler = VALUE_TO_FILE_HANDLER(file_value);

    if(!handler){
        vmu_error(context, "File is closed");
    }

    return BOOL_VALUE(FILE_CAN_APPEND(mode));
}

Value native_fn_io_is_binary(uint8_t argsc, Value *values, Value *target, void *context){
    Value *file_value = &values[0];

    VALIDATE_VALUE_FILE_ARG(file_value, 1, "file", context)

    char mode = VALUE_TO_FILE_MODE(file_value);
    FILE *handler = VALUE_TO_FILE_HANDLER(file_value);

    if(!handler){
        vmu_error(context, "File is closed");
    }

    return BOOL_VALUE(FILE_IS_BINARY(mode));
}

Value native_fn_io_is_open(uint8_t argsc, Value *values, Value *target, void *context){
    Value *file_value = &values[0];

    VALIDATE_VALUE_FILE_ARG(file_value, 1, "file", context)

    FILE *handler = VALUE_TO_FILE_HANDLER(file_value);

    return BOOL_VALUE(handler != NULL);
}

Value native_fn_io_open_file(uint8_t argsc, Value *values, Value *target, void *context){
    Value *path_value = &values[0];
    Value *mode_value = &values[1];

    VALIDATE_VALUE_STR_EMPTY_ARG(path_value, 1, "path", context)
    VALIDATE_VALUE_STR_EMPTY_ARG(mode_value, 2, "mode", context)

    StrObj *path_str = VALUE_TO_STR(path_value);
    StrObj *mode_str = VALUE_TO_STR(mode_value);

    char *path_raw_str = path_str->buff;
    char *mode_raw_str = mode_str->buff;
    size_t mode_len = mode_str->len;

    VALIDATE_I64_RANGE(1, 3, mode_len, 2, "mode", context)

    char mode = 0;

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
                    vmu_error(context, "Illegal value of argument %d: expect '%s' value at index 0 of 'r', 'w' or 'a'", 2, "mode");
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
                    vmu_error(context, "Illegal value of argument %d: expect '%s' value at index 1 of '+' or 'b'", 2, "mode");
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
                    vmu_error(context, "Illegal value of argument %d: expect '%s' value at index 2 of '+' or 'b'", 2, "mode");
                    break;
                }
            }

            char previous_char = mode_raw_str[i - 1];

            if(previous_char == '+' && current_char != 'b'){
                vmu_error(context, "Illegal value of argument %d: expect '%s' value 'b'", 2, "mode");
            }
            if(previous_char == 'b' && current_char != '+'){
                vmu_error(context, "Illegal value of argument %d: expect '%s' value '+'", 2, "mode");
            }
        }
    }

    if(!FILE_CAN_WRITE(mode) && !FILE_CAN_APPEND(mode)){
        if(!UTILS_FILES_EXISTS(path_raw_str)){
            vmu_error(context, "Illegal value of argument %d: '%s' value '%s' does not exist", 1, "path", path_raw_str);
        }

        if(utils_files_is_directory(path_raw_str)){
            vmu_error(context, "Illegal value of argument %d: '%s' value is a directory '%s'", 1, "path", path_raw_str);
        }
    }

    ObjHeader *record_header = vmu_create_record_file_obj(mode_raw_str, mode, path_raw_str, context);

    return OBJ_VALUE(record_header);
}

Value native_fn_io_close_file(uint8_t argsc, Value *values, Value *target, void *context){
    Value *file_value = &values[0];

    VALIDATE_VALUE_FILE_ARG(file_value, 1, "file", context)

    RecordObj *record_obj = VALUE_TO_RECORD(file_value);
    RecordFile *record_file = RECORD_FILE(record_obj);
    FILE *handler = record_file->handler;

    if(!handler){
        vmu_error(context, "File already closed");
    }

    fclose(handler);
    record_file->handler = NULL;

    return EMPTY_VALUE;
}

Value native_fn_io_read_byte(uint8_t argsc, Value *values, Value *target, void *context){
    Value *file_value = &values[0];

    VALIDATE_VALUE_FILE_ARG(file_value, 1, "file", context)

    char mode = VALUE_TO_FILE_MODE(file_value);
    char *pathname = VALUE_TO_FILE_PATHNAME(file_value);
    FILE *handler = VALUE_TO_FILE_HANDLER(file_value);

    if(!handler){
        vmu_error(context, "File already closed");
    }

    if(!FILE_CAN_READ(mode)){
        vmu_error(context, "File handler for '%s' is no opened for read", pathname);
    }

    int64_t byte = fgetc(handler);

    return INT_VALUE(byte == EOF ? -1 : byte);
}

Value native_fn_io_read_char(uint8_t argsc, Value *values, Value *target, void *context){
    Value *file_value = &values[0];

    VALIDATE_VALUE_FILE_ARG(file_value, 1, "file", context)

    char mode = VALUE_TO_FILE_MODE(file_value);
    char *pathname = VALUE_TO_FILE_PATHNAME(file_value);
    FILE *handler = VALUE_TO_FILE_HANDLER(file_value);

    if(!handler){
        vmu_error(context, "File already closed");
    }

    if(!FILE_CAN_READ(mode)){
        vmu_error(context, "File handler for '%s' is no opened for read", pathname);
    }

    int64_t byte = fgetc(handler);

    if(byte == EOF){
        return INT_VALUE(-1);
    }

    char raw_str[] = {(char)byte, 0};
    char *cloned_raw_str = factory_clone_raw_str(raw_str, ((VM *)context)->fake_allocator);
    ObjHeader *str_obj = vmu_create_str_obj(&cloned_raw_str, context);

    return OBJ_VALUE(str_obj);
}

Value native_fn_io_write_byte(uint8_t argsc, Value *values, Value *target, void *context){
    Value *byte_value = &values[0];
    Value *file_value = &values[1];

    VALIDATE_VALUE_INT_ARG(byte_value, 1, "byte", context)
    VALIDATE_I64_RANGE(0, 255, VALUE_TO_INT(byte_value), 1, "byte", context)
    VALIDATE_VALUE_FILE_ARG(file_value, 2, "file", context)

    int64_t byte = VALUE_TO_INT(byte_value);
    char mode = VALUE_TO_FILE_MODE(file_value);
    char *pathname = VALUE_TO_FILE_PATHNAME(file_value);
    FILE *handler = VALUE_TO_FILE_HANDLER(file_value);

    if(!handler){
        vmu_error(context, "File already closed");
    }

    if(!FILE_CAN_WRITE(mode)){
        vmu_error(context, "File handler for '%s' is no opened for write", pathname);
    }

    if(fputc((int)byte, handler) == EOF){
        vmu_error(context, "Failed to write byte");
    }

    return EMPTY_VALUE;
}

Value native_fn_io_write_str(uint8_t argsc, Value *values, Value *target, void *context){
    Value *str_value = &values[0];
    Value *file_value = &values[1];

    VALIDATE_VALUE_STR_ARG(str_value, 1, "str", context)
    VALIDATE_VALUE_FILE_ARG(file_value, 2, "file", context)

    StrObj *str = VALUE_TO_STR(str_value);
    char mode = VALUE_TO_FILE_MODE(file_value);
    char *pathname = VALUE_TO_FILE_PATHNAME(file_value);
    FILE *handler = VALUE_TO_FILE_HANDLER(file_value);

    if(!handler){
        vmu_error(context, "File already closed");
    }

    if(!FILE_CAN_WRITE(mode)){
        vmu_error(context, "File handler for '%s' is no opened for write", pathname);
    }

    if(fputs(str->buff, handler) == EOF){
        vmu_error(context, "Failed to write 'str'");
    }

    return EMPTY_VALUE;
}

Value native_fn_io_read_file(uint8_t argsc, Value *values, Value *target, void *context){
    Value *path_value = &values[0];

    if(!IS_VALUE_STR(path_value)){
        vmu_error(context, "Expect string at argument 1, but got something else");
    }

    StrObj *path = VALUE_TO_STR(path_value);
    char *raw_path = path->buff;

    size_t content_len = 0;
    char *content = NULL;
    size_t err_len = 1024;
    char err_str[err_len];

    if(utils_read_file(raw_path, &content_len, &content, err_len, err_str, ((VM *)context)->fake_allocator)){
        vmu_error(context, err_str);
    }

    if(!content){
        vmu_error(context, "Unexpected outcome");
    }

    ObjHeader *content_obj = vmu_create_str_obj(&content, context);

    return OBJ_VALUE(content_obj);
}

void io_module_init(Allocator *allocator){
    io_module = factory_native_module("io", allocator);

    factory_add_native_fn("is_file", 1, native_fn_io_is_file, io_module, allocator);

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
