#include "utils.h"
#include "memory.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

int utils_is_reg(char *filename){
	struct stat file = {0};

	if(stat(filename, &file) == -1)
		return -1;

	return S_ISREG(file.st_mode);
}

RawStr *utils_read_source(char *path){
	FILE *source_file = fopen(path, "r");
    if(source_file == NULL) return NULL;

	fseek(source_file, 0, SEEK_END);

	size_t source_size = (size_t)ftell(source_file);
	char *buff = A_COMPILE_ALLOC(source_size + 1);
	RawStr *rstr = (RawStr *)A_COMPILE_ALLOC(sizeof(RawStr));

	fseek(source_file, 0, SEEK_SET);
	fread(buff, 1, source_size, source_file);
	fclose(source_file);

	buff[source_size] = '\0';

	rstr->size = source_size;
	rstr->buff = buff;

	return rstr;
}