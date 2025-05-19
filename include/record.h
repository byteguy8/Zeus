#ifndef RECORD_H
#define RECORD_H

#include "xoshiro256.h"
#include "lzhtable.h"

typedef enum record_type{
    NONE_RTYPE,
    RANDOM_RTYPE,
}RecordType;

typedef struct record{
    RecordType type;

    union{
        XOShiro256 xos256;
    }content;

	LZHTable *attributes;
}Record;

#endif