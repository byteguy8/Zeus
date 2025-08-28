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
#include <inttypes.h>

typedef enum obj_type ObjType;
typedef enum obj_color ObjColor;
typedef struct obj Obj;
typedef struct obj_list ObjList;

enum obj_type{
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
};

enum obj_color{
    WHITE_OBJ_COLOR,
    GRAY_OBJ_COLOR,
    BLACK_OBJ_COLOR,
};

struct obj{
    ObjType type;
    char marked;
    ObjColor color;
    Obj *prev;
    Obj *next;
    ObjList *list;
};

struct obj_list{
    size_t len;
    Obj *head;
    Obj *tail;
};

typedef struct str_obj{
    Obj header;
    char runtime;
    size_t len;
	char *buff;
}StrObj;

typedef struct array_obj{
    Obj header;
    size_t len;
    Value *values;
}ArrayObj;

typedef struct list_obj{
    Obj header;
    DynArr *items;
}ListObj;

typedef struct dict_obj{
    Obj header;
    LZOHTable *key_values;
}DictObj;

typedef enum record_type{
    NONE_RTYPE,
    RANDOM_RTYPE,
    FILE_RTYPE,
}RecordType;

typedef XOShiro256 RecordRandom;

typedef struct record_file{
    char mode;
    char *pathname;
    FILE *handler;
}RecordFile;

typedef struct record_obj{
    Obj header;
    RecordType type;
    void *content;
	LZOHTable *attrs;
}RecordObj;

typedef struct native_fn_obj{
    Obj header;
    Value target;
    NativeFn *native_fn;
}NativeFnObj;

typedef struct fn_obj{
    Obj header;
    Fn *fn;
}FnObj;

typedef struct closure_obj{
    Obj header;
    Closure *closure;
}ClosureObj;

typedef struct native_module_obj{
    Obj header;
    NativeModule *native_module;
}NativeModuleObj;

typedef struct module_obj{
    Obj header;
    Module *module;
}ModuleObj;

#define OBJ_TYPE(_obj)((_obj)->type)
#define RECORD_RANDOM(_record)((RecordRandom *)((_record)->content))

#define RECORD_FILE(_record)((RecordFile *)((_record)->content))
#define RECORD_FILE_MODE(_record)(RECORD_FILE(_record)->mode)
#define RECORD_FILE_PATHNAME(_record)(RECORD_FILE(_record)->pathname)
#define RECORD_FILE_HANDLER(_record)(RECORD_FILE(_record)->handler)

#define FILE_READ_MODE   0b10000000
#define FILE_WRITE_MODE  0b01000000
#define FILE_APPEND_MODE 0b00100000
#define FILE_BINARY_MODE 0b00000010
#define FILE_PLUS_MODE   0b00000001

#define FILE_CAN_READ(_mode) (((_mode) & FILE_READ_MODE) || ((_mode) & FILE_PLUS_MODE))
#define FILE_CAN_WRITE(_mode) (((_mode) & FILE_WRITE_MODE) || ((_mode) & FILE_APPEND_MODE) || ((_mode) & FILE_PLUS_MODE))
#define FILE_CAN_APPEND(_mode) ((_mode) & FILE_APPEND_MODE)
#define FILE_IS_BINARY(_mode)((_mode) & FILE_BINARY_MODE)

void obj_list_insert(Obj *obj, ObjList *list);
void obj_list_remove(Obj *obj);

#endif