#ifndef UTILS_H
#define UTILS_H

#include "types.h"
#include <unistd.h>
#include <sys/stat.h>

#define UTILS_EXISTS_FILE(pathname) \
	(access(pathname, F_OK) == 0)
int utils_is_reg(char *filename);
RawStr *utils_read_source(char *path);

#endif
