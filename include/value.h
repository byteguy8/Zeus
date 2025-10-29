#ifndef VALUE_H
#define VALUE_H

#include <stdint.h>

typedef enum value_type{
    EMPTY_VALUE_TYPE,
    BOOL_VALUE_TYPE,
    INT_VALUE_TYPE,
    FLOAT_VALUE_TYPE,
	OBJ_VALUE_TYPE
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