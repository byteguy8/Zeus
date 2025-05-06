#ifndef STR_H
#define STR_H

#include <stdint.h>
#include <stddef.h>

typedef int32_t sidx_t;
#define STR_LENGTH_MAX INT32_MAX
#define STR_LENGTH_TYPE sidx_t

typedef struct str{
	char runtime;
    uint32_t hash;
	sidx_t len;
	char *buff;
}Str;

#endif