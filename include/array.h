#ifndef ARRAY_H
#define ARRAY_H

#include "value.h"
#include <stdint.h>

typedef int32_t aidx_t;
#define ARRAY_LENGTH_MAX INT32_MAX
#define ARRAY_LENGTH_TYPE aidx_t

typedef struct array{
    aidx_t len;
    Value *values;
}Array;

#endif