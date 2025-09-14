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

int utils_hexadecimal_str_to_i64(char *str, int64_t *out_value){
    int len = (int)strlen(str);

    if(len <= 2 || len > 18){
        return INVLEN;
    }

    if(strncasecmp(str, "0x", 2) != 0){
        return INVPRE;
    }

    uint8_t value_offset = (len - 2) * 4;
    int64_t value = 0;

    for (int i = 2; i < len; i++){
        char c = str[i];

        if((c < '0' || c > '9') && (c < 'a' || c > 'f') && (c < 'A' || c > 'F')){
            return INVDIG;
        }

        uint64_t nibble = 0;

        switch (c){
            case 'a':
            case 'A':{
                nibble = 10;
                break;
            }
            case 'b':
            case 'B':{
                nibble = 11;
                break;
            }
            case 'c':
            case 'C':{
                nibble = 12;
                break;
            }
            case 'd':
            case 'D':{
                nibble = 13;
                break;
            }
            case 'e':
            case 'E':{
                nibble = 14;
                break;
            }
            case 'f':
            case 'F':{
                nibble = 15;
                break;
            }
            default:{
                nibble = ((int64_t)c) - 48;
            }
        }

        nibble <<= value_offset -= 4;
        value |= nibble;
    }

    *out_value = value;

    return 0;
}

int utils_decimal_str_to_i64(char *str, int64_t *out_value){
    int len = (int)strlen(str);

    if(len == 0){
        return INVLEN;
    }

    int64_t value = 0;
    int is_negative = str[0] == '-';

    for(int i = is_negative ? 1 : 0; i < len; i++){
        char c = str[i];

        if(c < '0' || c > '9'){
            return INVDIG;
        }

        int64_t digit = ((int64_t)c) - 48;

        value *= 10;
        value += digit;
    }

    if(is_negative == 1){
        value *= -1;
    }

    *out_value = value;

    return 0;
}

#define ABS(value)(value < 0 ? value * -1 : value)

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

RawStr *utils_read_source(char *pathname, Allocator *allocator){
	FILE *source_file = fopen(pathname, "r");

    if(!source_file){
        return NULL;
    }

	fseek(source_file, 0, SEEK_END);

	size_t source_size = (size_t)ftell(source_file);
    char *buff = MEMORY_ALLOC(char, source_size + 1, allocator);
	RawStr *rstr = MEMORY_ALLOC(RawStr, 1, allocator);

	fseek(source_file, 0, SEEK_SET);
	fread(buff, 1, source_size, source_file);
	fclose(source_file);

	buff[source_size] = '\0';

	rstr->len = source_size;
	rstr->buff = buff;

	return rstr;
}


#ifdef _WIN32
    int utils_files_is_directory(LPCSTR pathname){
        DWORD attributes = GetFileAttributesA(pathname);
        return attributes & FILE_ATTRIBUTE_DIRECTORY;
    }

    int utils_files_is_regular(LPCSTR pathname){
	    DWORD attributes = GetFileAttributesA(pathname);
        return attributes & FILE_ATTRIBUTE_ARCHIVE;
    }

    char *utils_files_parent_pathname(char *pathname, Allocator *allocator){
        char *cloned_pathname = allocator ? factory_clone_raw_str(pathname, allocator, NULL) : pathname;
        PathRemoveFileSpecA(cloned_pathname);
        return cloned_pathname;
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
#elif __linux__
    int utils_files_is_directory(char *pathname){
        struct stat file = {0};

        if(stat(pathname, &file) == -1){
            return -1;
        }

        return S_ISDIR(file.st_mode);
    }

    int utils_files_is_regular(char *pathname){
        struct stat file = {0};

        if(stat(pathname, &file) == -1){
            return -1;
        }

        return S_ISREG(file.st_mode);
    }

    char *utils_files_parent_pathname(char *pathname, Allocator *allocator){
        char *cloned_pathname = allocator ? factory_clone_raw_str(pathname, allocator, NULL) : pathname;
        return dirname(cloned_pathname);
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
#endif