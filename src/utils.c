#include "utils.h"
#include "memory.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

int utils_is_integer(char *buff){
	size_t buff_len = strlen(buff);
	if(buff_len == 0) return 0;

	for(size_t i = 0; i < buff_len; i++){
		char c = buff[i];
		if(c < '0' || c > '9') return 0;
	}

	return 1;
}

int utils_str_to_i64(char *str, int64_t *out_value){
    int len = (int)strlen(str);
    int is_negative = 0;
    int64_t value = 0;

    if(len == 0) return 1;
    
    for(int i = 0; i < len; i++){
        char c = str[i];
        
        if(c == '-' && i == 0){
            is_negative = 1;
            continue;
        }
        
        if(c < '0' || c > '9')
            return 1;
            
        int64_t digit = ((int64_t)c) - 48;
        
        value *= 10;
        value += digit;
    }
    
    if(is_negative == 1)
        value *= -1;
        
    *out_value = value;
    
    return 0;
}

#define ABS(value)(value < 0 ? value * -1 : value)

int utils_i64_to_str(int64_t value, char *out_value){
    int buff_len = 20;
    char buff[buff_len];
    int cursor = buff_len - 1;
    char is_negative = value < 0;

    for (int i = cursor; i >= 0; i--){
        int64_t a = value / 10;
        int64_t b = a * 10;
        int64_t part = ABS((value - b));

        value = a;
        buff[cursor--] = 48 + part;

        if(value == 0) break;
    }
    
    if(is_negative) buff[cursor--] = '-';
    
    int len = buff_len - 1 - cursor;
    memcpy(out_value, buff + cursor + 1, len);

    return len;
}

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
