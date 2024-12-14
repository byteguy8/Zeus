#ifndef OBJ_H
#define OBJ_H

#include "dynarr.h"
#include "lzhtable.h"
#include "types.h"

typedef enum obj_type{
	STR_OTYPE,
	LIST_OTYPE,
    DICT_OTYPE,
	RECORD_OTYPE,
    FN_OTYPE,
    NATIVE_FN_OTYPE,
    MODULE_OTYPE,
}ObjType;

typedef struct obj{    
    char marked;
    struct obj *prev;
    struct obj *next;
	
    ObjType type;

    union{
		Str *str;
		DynArr *list;
		Record *record;
        LZHTable *dict;
        Fn *fn;
        NativeFn *native_fn;
        Module *module;
	}value;
}Obj;

#endif
