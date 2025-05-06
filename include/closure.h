#ifndef CLOSURE_H
#define CLOSURE_H

#include "value.h"
#include "obj.h"
#include "fn.h"
#include <stdint.h>

#define OUT_VALUES_LENGTH 255

typedef struct meta_out_value{
    uint8_t at;
}MetaOutValue;

typedef struct meta_closure{
    uint8_t values_len;
    MetaOutValue values[OUT_VALUES_LENGTH];
    Fn *fn;
}MetaClosure;

typedef struct out_value{
    uint8_t linked;
    uint8_t at;
    Value *value;
    struct out_value *prev;
    struct out_value *next;
    Obj *closure;
}OutValue;

typedef struct closure{
    OutValue *out_values;
    MetaClosure *meta;
}Closure;

#endif