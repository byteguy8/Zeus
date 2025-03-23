#ifndef UTILS_H
#define UTILS_H

#include "rtypes.h"
#include "types.h"
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

char *utils_join_raw_strs(
    size_t buffa_len,
    char *buffa,
    size_t buffb_len,
    char *buffb,
    Allocator *allocator
);
char *utils_multiply_raw_str(size_t by, size_t buff_len, char *buff, Allocator *allocator);

int utils_is_integer(char *buff);
int utils_is_float(char *buff);

int utils_str_to_i64(char *str, int64_t *out_value);
int utils_i64_to_str(int64_t value, char *out_value);

int utils_str_to_double(char *raw_str, double *out_value);
int utils_double_to_str(int buff_len, double value, char *out_value);
int utils_read_file(
    char *pathname,
    size_t *length,
    char **content,
    size_t err_len,
    char *err_str,
    Allocator *allocator
);
RawStr *utils_read_source(char *pathname, Allocator *allocator);

//> SYSTEM DEPENDENT
// test for the existence of a file
#define UTILS_FILE_EXISTS(pathname) (access(pathname, F_OK) == 0)
// test for the existence of a file and if can be read
#define UTILS_FILE_CAN_READ(pathname) (access(pathname, R_OK) == 0)
int utils_file_is_regular(char *filename);
char *utils_parent_pathname(char *pathname);
char *utils_cwd(Allocator *allocator);
char *utils_sysname(Allocator *allocator);
//< SYSTEM DEPENDENT

#endif
