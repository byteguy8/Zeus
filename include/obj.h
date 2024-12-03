#ifndef OBJ_H
#define OBJ_H

#include "dynarr.h"
#include "lzhtable.h"
#include "types.h"

typedef enum obj_type{
	STRING_OTYPE,
	LIST_OTYPE,
    DICT_OTYPE,
    FN_OTYPE,
    NATIVE_FN_OTYPE,
}ObjType;

typedef struct obj{    
    char marked;
    struct obj *prev;
    struct obj *next;
	
    ObjType type;

    union{
		Str *str;
		DynArr *list;
        LZHTable *dict;
        Function *fn;
        NativeFunction *native_fn;
	}value;
}Obj;

#endif