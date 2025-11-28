#ifndef VM_TYPES
#define VM_TYPES

#include <stddef.h>

typedef struct static_str{
    size_t len;
    char *buff; // NULL terminated (I know)
}VmStaticStr;

#endif