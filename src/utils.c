#include "utils.h"
#include "memory.h"
#include "factory.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#if _WIN32
    #include <shlwapi.h>
#elif __linux__
    #define _POSIX_C_SOURCE 200809L

    #include <errno.h>
    #include <time.h>
    #include <libgen.h>
    #include <sys/utsname.h>
#endif

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
    if(!UTILS_FILES_EXISTS(pathname)){
        snprintf(err_str, err_len, "Pathname does not exists");
        return 1;
    }
    if(!UTILS_FILE_CAN_READ(pathname)){
        snprintf(err_str, err_len, "Pathname can not be read");
        return 1;
    }
    if(!utils_files_is_regular(pathname)){
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


#ifdef _WIN32
    int utils_files_is_regular(LPCSTR pathname){
	    DWORD attributes = GetFileAttributesA(pathname);
        return attributes & FILE_ATTRIBUTE_ARCHIVE;
    }

    char *utils_files_parent_pathname(char *pathname){
        PathRemoveFileSpecA(pathname);
        return pathname;
    }

    char *utils_files_cwd(Allocator *allocator){
        DWORD buff_len = 0;
        LPTSTR buff = NULL;

        buff_len = GetCurrentDirectory(buff_len, buff);
        buff = MEMORY_ALLOC(CHAR, buff_len, allocator);

        if(!buff){
            return NULL;
        }

        if(GetCurrentDirectory(buff_len, buff) == 0){
            MEMORY_DEALLOC(CHAR, buff_len, buff, allocator);
            return NULL;
        }

        return buff;
    }

    int64_t utils_millis(){
        FILETIME current_filetime = {0};
        GetSystemTimeAsFileTime(&current_filetime);

        SYSTEMTIME epoch_systime = {0};
        epoch_systime.wYear = 1970;
        epoch_systime.wMonth = 1;
        epoch_systime.wDayOfWeek = 4;
        epoch_systime.wDay = 1;

        FILETIME epoch_filetime = {0};
        SystemTimeToFileTime(&epoch_systime, &epoch_filetime);

        ULARGE_INTEGER current_ularge = {0};
        current_ularge.u.LowPart = current_filetime.dwLowDateTime;
        current_ularge.u.HighPart = current_filetime.dwHighDateTime;

        ULARGE_INTEGER epoch_ularge = {0};
        epoch_ularge.u.LowPart = epoch_filetime.dwLowDateTime;
        epoch_ularge.u.HighPart = epoch_filetime.dwHighDateTime;

        ULARGE_INTEGER new_current_ularge = {0};
        new_current_ularge.QuadPart = current_ularge.QuadPart - epoch_ularge.QuadPart;

        return (new_current_ularge.QuadPart * 100) / 1000000;
    }

    void utils_sleep(int64_t time){
        Sleep(time);
    }

    char *utils_files_sysname(Allocator *allocator){
        return factory_clone_raw_str("Windows", allocator);
    }
#elif __linux__
    int utils_files_is_regular(char *pathname){
        struct stat file = {0};

        if(stat(pathname, &file) == -1){
            return -1;
        }

        return S_ISREG(file.st_mode);
    }

    char *utils_files_parent_pathname(char *pathname){
        return dirname(pathname);
    }

    char *utils_files_cwd(Allocator *allocator){
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

    int64_t utils_millis(){
        struct timespec spec = {0};
        clock_gettime(CLOCK_REALTIME, &spec);

        return spec.tv_nsec / 1e+6 + spec.tv_sec * 1000;
    }

    void utils_sleep(int64_t time){
        usleep(time * 1000);
    }

    char *utils_files_sysname(Allocator *allocator){
        return factory_clone_raw_str("Linux", allocator);
    }
#endif