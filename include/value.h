#ifndef VALUE_H
#define VALUE_H

#include "obj.h"
#include <stddef.h>
#include <stdint.h>

typedef enum value_type{
    EMPTY_VTYPE,
    BOOL_VTYPE,
    INT_VTYPE,
	OBJ_VTYPE
}ValueType;

typedef struct value{
    ValueType type;
    union{
        uint8_t bool;
        int64_t i64;
		Obj *obj;
    }literal;
}Value;

#endif
