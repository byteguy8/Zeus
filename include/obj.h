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
    CLOSURE_OTYPE,
    NATIVE_FN_OTYPE,
    NATIVE_MODULE_OTYPE,
    MODULE_OTYPE,
    FOREIGN_FN_OTYPE,
    NATIVE_LIB_OTYPE,
}ObjType;

typedef struct obj{    
    char marked;
    ObjType type;
    struct obj *prev;
    struct obj *next;

    union{
		Str *str;
        Array *array;
		DynArr *list;
        LZHTable *dict;
		Record *record;
        NativeFn *native_fn;
        Fn *fn;
        Closure *closure;
        ForeignFn *foreign_fn;
        NativeModule *native_module;
        Module *module;
        NativeLib *native_lib;
	}value;
}Obj;

#endif
