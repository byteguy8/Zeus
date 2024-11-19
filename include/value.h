#ifndef VALUE_H
#define VALUE_H

#include <stddef.h>
#include <stdint.h>

typedef enum obj_type{
	STRING_OTYPE
}ObjType;

typedef struct str{
	char core;
	size_t len;
	char *buff;
}Str;

typedef struct obj{    
    char marked;
    struct obj *prev;
    struct obj *next;
	
    ObjType type;

    union{
		Str str;
	}value;
}Obj;

typedef enum value_type{
    EMPTY_VTYPE,
    BOOL_VTYPE,
    INT_VTYPE,
	OBJ_VTYPE
}ValueType;

typedef struct value
{
    ValueType type;
    union{
        uint8_t bool;
        int64_t i64;
		Obj *obj;
    }literal;
}Value;

#endif