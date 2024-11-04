#include "utils.h"
#include "memory.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

RawStr *utils_read_source(char *path){
	FILE *source_file = fopen(path, "r");
    if(source_file == NULL) return NULL;

	fseek(source_file, 0, SEEK_END);

	size_t source_size = (size_t)ftell(source_file);
	char *buff = memory_alloc(source_size + 1);
	RawStr *rstr = (RawStr *)memory_alloc(sizeof(RawStr));

	fseek(source_file, 0, SEEK_SET);
	fread(buff, 1, source_size, source_file);
	fclose(source_file);

	buff[source_size] = '\0';

	rstr->size = source_size;
	rstr->buff = buff;

	return rstr;
}