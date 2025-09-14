#ifndef UTILS_H
#define UTILS_H

#include "types.h"
#include <stdint.h>
#include <string.h>

#ifdef _WIN32
    #include <io.h>
    #include <windows.h>

    #define ACCESS_MODE_EXISTS 0
    #define ACCESS_MODE_WRITE_ONLY 2
    #define ACCESS_MODE_READ_ONLY 4
    #define ACCESS_MODE_READ_AND_WRITE 6
#elif __linux__
    #include <unistd.h>
    #include <sys/stat.h>
#endif

#ifndef SYS_PATH_SEPARATOR
    #ifdef _WIN32
        #define SYS_PATH_SEPARATOR ';'
    #elif __linux__
        #define SYS_PATH_SEPARATOR ':'
    #else
        #error "Must define a system path separator"
    #endif
#endif

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

#define INVLEN 1
#define INVPRE 2
#define INVDIG 3

int utils_decimal_str_to_i64(char *str, int64_t *out_value);
int utils_hexadecimal_str_to_i64(char *str, int64_t *out_value);
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

#ifdef _WIN32
    #define UTILS_FILES_EXISTS(pathname) (_access(pathname, ACCESS_MODE_EXISTS) == 0)
    #define UTILS_FILES_CAN_READ(pathname) (access(pathname, ACCESS_MODE_READ_ONLY) == 0)
    int utils_files_is_directory(LPCSTR pathname);
    int utils_files_is_regular(LPCSTR pathname);
#elif __linux__
    #define UTILS_FILES_EXISTS(pathname) (access(pathname, F_OK) == 0)
    #define UTILS_FILES_CAN_READ(pathname) (access(pathname, R_OK) == 0)
    int utils_files_is_directory(char *pathname);
    int utils_files_is_regular(char *pathname);
#endif

char *utils_files_parent_pathname(char *pathname, Allocator *allocator);
char *utils_files_cwd(Allocator *allocator);
char *utils_files_sysname(Allocator *allocator);

int64_t utils_millis();
void utils_sleep(int64_t time);

#endif
