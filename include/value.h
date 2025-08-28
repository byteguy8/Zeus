#ifndef VALUE_H
#define VALUE_H

#include <stdint.h>

typedef enum value_type{
    EMPTY_VTYPE,
    BOOL_VTYPE,
    INT_VTYPE,
    FLOAT_VTYPE,
	OBJ_VTYPE
}ValueType;

typedef struct value{
    ValueType type;

    union{
        uint8_t bool;
        int64_t ivalue;
        double fvalue;
		void *obj;
    }content;
}Value;

#define VALUE_SIZE sizeof(Value)
#define VALUE_TYPE(_value)((_value)->type)
#define IS_VALUE_MARKED(_object)(((ObjHeader *)(_object))->marked == 1)

#endif