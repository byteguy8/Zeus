#ifndef OBJ_H
#define OBJ_H

#include "dynarr.h"
#include "lzhtable.h"
#include "types.h"

typedef enum obj_type{
	STR_OTYPE,
    ARRAY_OTYPE,
	LIST_OTYPE,
    DICT_OTYPE,
	RECORD_OTYPE,
    FN_OTYPE,
    NATIVE_FN_OTYPE,
    NATIVE_MODULE_OTYPE,
    MODULE_OTYPE,
    FOREIGN_FN_OTYPE,
    NATIVE_LIB_OTYPE,
}ObjType;

typedef struct obj{    
    char marked;
    struct obj *prev;
    struct obj *next;
	
    ObjType type;

    union{
		Str *str;
        Array *array;
		DynArr *list;
		Record *record;
        LZHTable *dict;
        NativeFn *native_fn;
        Fn *fn;
        NativeModule *native_module;
        Module *module;
        ForeignFn *foreign_fn;
        NativeLib *native_lib;
	}value;
}Obj;

#endif
