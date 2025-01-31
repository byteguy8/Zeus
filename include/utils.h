#ifndef UTILS_H
#define UTILS_H

#include "types.h"
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

int utils_is_integer(char *buff);
int utils_str_to_i64(char *str, int64_t *out_value);
int utils_i64_to_str(int64_t value, char *out_value);
// test for the existence of a file
#define UTILS_FILE_EXISTS(pathname) (access(pathname, F_OK) == 0)
// test for the existence of a file and if can be read
#define UTILS_FILE_CAN_READ(pathname) (access(pathname, R_OK) == 0)
int utils_file_is_regular(char *filename);
RawStr *compile_read_source(char *path);
char *compile_cwd();

#endif
