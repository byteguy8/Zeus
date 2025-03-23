#include "utils.h"
#include "memory.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
//> LINUX
#include <unistd.h>
#include <libgen.h>
#include <sys/utsname.h>
#include <errno.h>
//< LINUX

char *utils_join_raw_strs(
    size_t buffa_len,
    char *buffa,
    size_t buffb_len,
    char *buffb,
    Allocator *allocator
){
    size_t buffc_len = buffa_len + buffb_len;
    char *buff = MEMORY_ALLOC(char, buffc_len + 1, allocator);

	if(!buff){return NULL;}

    memcpy(buff, buffa, buffa_len);
    memcpy(buff + buffa_len, buffb, buffb_len);
    buff[buffc_len] = '\0';

    return buff;
}

char *utils_multiply_raw_str(size_t by, size_t buff_len, char *buff, Allocator *allocator){
    size_t new_buff_len = buff_len * by;
	char *new_buff = MEMORY_ALLOC(char, new_buff_len + 1, allocator);

	if(!new_buff){return NULL;}

	for(size_t i = 0; i < by; i++){
        memcpy(new_buff + (i * buff_len), buff, buff_len);
    }

	new_buff[new_buff_len] = '\0';

	return new_buff;
}

int utils_is_integer(char *buff){
	size_t buff_len = strlen(buff);
	if(buff_len == 0) return 0;

	for(size_t i = 0; i < buff_len; i++){
		char c = buff[i];

        if(c == '-'){
            if(i > 0) return 0;
            continue;
        }

		if(c < '0' || c > '9'){return 0;}
	}

	return 1;
}

int utils_is_float(char *buff){
    size_t buff_len = strlen(buff);
	if(buff_len == 0){return 0;}
    int decimal_dot = 0;

	for(size_t i = 0; i < buff_len; i++){
		char c = buff[i];

        if(c == '-'){
            if(i > 0){return 0;}
            continue;
        }

        if(c == '.'){
            if(decimal_dot){return 0;}
            if(i == 0 || i + 1 >= buff_len){return 0;}
            if(!decimal_dot){decimal_dot = 1;}
            continue;
        }

		if(c < '0' || c > '9'){return 0;}
	}

	return decimal_dot;
}

int utils_str_to_i64(char *str, int64_t *out_value){
    int len = (int)strlen(str);
    int is_negative = 0;
    int64_t value = 0;

    if(len == 0){return 1;}

    for(int i = 0; i < len; i++){
        char c = str[i];

        if(c == '-' && i == 0){
            is_negative = 1;
            continue;
        }

        if(c < '0' || c > '9'){return 1;}

        int64_t digit = ((int64_t)c) - 48;

        value *= 10;
        value += digit;
    }

    if(is_negative == 1){value *= -1;}

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

        if(value == 0){break;}
    }

    if(is_negative) buff[cursor--] = '-';

    int len = buff_len - 1 - cursor;
    memcpy(out_value, buff + cursor + 1, len);

    return len;
}

int utils_str_to_double(char *raw_str, double *out_value){
    int len = (int)strlen(raw_str);
    int is_negative = 0;
    int is_decimal = 0;
    double special = 10.0;
    double value = 0.0;

    for(int i = 0; i < len; i++){
        char c = raw_str[i];

        if(c == '-' && i == 0){
            is_negative = 1;
            continue;
        }

        if(c == '.'){
			if(i == 0){return 1;}
			is_decimal = 1;
			continue;
		}

        if(c < '0' || c > '9'){return 1;}

        int digit = ((int)c) - 48;

        if(is_decimal){
			double decimal = digit / special;
			value += decimal;
			special *= 10.0;
		}else{
			value *= 10.0;
			value += digit;
		}
    }

    if(is_negative == 1){value *= -1;}

    *out_value = value;

    return 0;
}

int utils_double_to_str(int buff_len, double value, char *out_value){
    assert(buff_len > 0);
    char buff[buff_len];
    int written = snprintf(buff, buff_len, "%.8f", value);
    memcpy(out_value, buff, written);
    return written == -1 ? -1 : written == buff_len ? -1 : written;
}

int utils_read_file(
    char *pathname,
    size_t *content_len,
    char **content,
    size_t err_len,
    char *err_str,
    Allocator *allocator
){
    if(!UTILS_FILE_EXISTS(pathname)){
        snprintf(err_str, err_len, "Pathname does not exists");
        return 1;
    }
    if(!UTILS_FILE_CAN_READ(pathname)){
        snprintf(err_str, err_len, "Pathname can not be read");
        return 1;
    }
    if(!utils_file_is_regular(pathname)){
        snprintf(err_str, err_len, "Pathname is not a regular file");
        return 1;
    }

    FILE *file = fopen(pathname, "r");

    if(!file){
        snprintf(err_str, err_len, "%s", strerror(errno));
        return 1;
    }

	fseek(file, 0, SEEK_END);

	size_t file_len = (size_t)ftell(file);
	char *file_content = allocator->alloc(file_len + 1, allocator->ctx);

    if(!file_content){
        fclose(file);
        snprintf(err_str, err_len, "Failed to allocate memory for buffer");
        return 1;
    }

	fseek(file, 0, SEEK_SET);
	fread(file_content, 1, file_len, file);
	fclose(file);

	file_content[file_len] = '\0';

	*content_len = file_len;
	*content = file_content;

	return 0;
}

RawStr *utils_read_source(char *pathname, Allocator *allocator){
	FILE *source_file = fopen(pathname, "r");
    if(source_file == NULL){return NULL;}

	fseek(source_file, 0, SEEK_END);

	size_t source_size = (size_t)ftell(source_file);
    char *buff = MEMORY_ALLOC(char, source_size + 1, allocator);
	RawStr *rstr = MEMORY_ALLOC(RawStr, 1, allocator);

	fseek(source_file, 0, SEEK_SET);
	fread(buff, 1, source_size, source_file);
	fclose(source_file);

	buff[source_size] = '\0';

	rstr->size = source_size;
	rstr->buff = buff;

	return rstr;
}

int utils_file_is_regular(char *filename){
	struct stat file = {0};

    if(stat(filename, &file) == -1){return -1;}

	return S_ISREG(file.st_mode);
}

char *utils_parent_pathname(char *pathname){
    return dirname(pathname);
}

char *utils_cwd(Allocator *allocator){
    char *pathname = getcwd(NULL, 0);
    size_t pathname_len = strlen(pathname);
    char *cloned_pathname = MEMORY_ALLOC(char, pathname_len + 1, allocator);

    if(!cloned_pathname){
        free(pathname);
        return NULL;
    }

    memcpy(cloned_pathname, pathname, pathname_len);
    cloned_pathname[pathname_len] = '\0';

    free(pathname);

    return cloned_pathname;
}

char *utils_sysname(Allocator *allocator){
    struct utsname sysinfo = {0};

    if(uname(&sysinfo) == 0){
        size_t name_len = strlen(sysinfo.sysname);
        char *name = allocator->alloc(name_len + 1, allocator->ctx);

        if(!name){return NULL;}

        memcpy(name, sysinfo.sysname, name_len);
        name[name_len] = 0;

        return name;
    }

    return NULL;
}