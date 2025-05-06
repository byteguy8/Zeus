#ifndef OBJ_H
#define OBJ_H

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

typedef struct obj{
    char marked;
    ObjType type;
    struct obj *prev;
    struct obj *next;
    void *content;
}Obj;

#define OBJ_TYPE(_obj)((_obj)->type)

#endif