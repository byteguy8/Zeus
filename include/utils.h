#ifndef UTILS_H
#define UTILS_H

#include "types.h"
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

int utils_is_integer(char *buff);
int utils_str_to_i64(char *str, int64_t *out_value);
int utils_i64_to_str(int64_t value, char *out_value);

#define UTILS_EXISTS_FILE(pathname) \
	(access(pathname, F_OK) == 0)

int utils_is_reg(char *filename);

RawStr *utils_read_source(char *path);

#endif
