#ifndef OBJ_H
#define OBJ_H

#include "xoshiro256.h"
#include "dynarr.h"
#include "lzhtable.h"
#include "value.h"
#include "native_fn.h"
#include "fn.h"
#include "closure.h"
#include "native_module.h"
#include "module.h"
#include <stdio.h>

typedef enum obj_type{
	STR_OTYPE,
    ARRAY_OTYPE,
	LIST_OTYPE,
    DICT_OTYPE,
	RECORD_OTYPE,
    NATIVE_FN_OTYPE,
    FN_OTYPE,
    CLOSURE_OTYPE,
    NATIVE_MODULE_OTYPE,
    MODULE_OTYPE,
}ObjType;

typedef struct obj_header{
    ObjType type;
    char marked;
    struct obj_header *prev;
    struct obj_header *next;
}ObjHeader;

#define OBJ_TYPE(_obj)((_obj)->type)

typedef int32_t sidx_t;
#define STR_LENGTH_MAX INT32_MAX
#define STR_LENGTH_TYPE sidx_t

typedef struct str_obj{
    ObjHeader header;
	uint32_t hash;
    sidx_t len;
    char runtime;
	char *buff;
}StrObj;

typedef int32_t aidx_t;
#define ARRAY_LENGTH_MAX INT32_MAX
#define ARRAY_LENGTH_TYPE aidx_t

typedef struct array_obj{
    ObjHeader header;
    aidx_t len;
    Value *values;
}ArrayObj;

typedef int32_t lidx_t;
#define LIST_LENGTH_MAX INT32_MAX
#define LIST_LENGTH_TYPE lidx_t

typedef struct list_obj{
    ObjHeader header;
    DynArr *list;
}ListObj;

typedef struct dict_obj{
    ObjHeader header;
    LZHTable *dict;
}DictObj;

typedef enum record_type{
    NONE_RTYPE,
    RANDOM_RTYPE,
    FILE_RTYPE,
}RecordType;

#define FILE_READ_MODE   0b10000000
#define FILE_WRITE_MODE  0b01000000
#define FILE_APPEND_MODE 0b00100000
#define FILE_BINARY_MODE 0b00000010
#define FILE_PLUS_MODE   0b00000001

#define FILE_CAN_READ(_mode) (((_mode) & FILE_READ_MODE) || ((_mode) & FILE_PLUS_MODE))
#define FILE_CAN_WRITE(_mode) (((_mode) & FILE_WRITE_MODE) || ((_mode) & FILE_APPEND_MODE) || ((_mode) & FILE_PLUS_MODE))
#define FILE_CAN_APPEND(_mode) ((_mode) & FILE_APPEND_MODE)
#define FILE_IS_BINARY(_mode)((_mode) & FILE_BINARY_MODE)

typedef struct record_obj{
    ObjHeader header;
    RecordType type;

    union{
        XOShiro256 xos256;
        struct {
            char mode;
            char *pathname;
            FILE *handler;
        }file;
    }content;

	LZHTable *attributes;
}RecordObj;

typedef struct native_fn_obj{
    ObjHeader header;
    NativeFn *native_fn;
}NativeFnObj;

typedef struct fn_obj{
    ObjHeader header;
    Fn *fn;
}FnObj;

typedef struct closure_obj{
    ObjHeader header;
    Closure *closure;
}ClosureObj;

typedef struct native_module_obj{
    ObjHeader header;
    NativeModule *native_module;
}NativeModuleObj;

typedef struct module_obj{
    ObjHeader header;
    Module *module;
}ModuleObj;

#endif