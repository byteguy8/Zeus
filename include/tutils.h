#ifndef TRANSFORMER_H
#define TRANSFORMER_H

#include "value.h"
#include "obj.h"
#include "array.h"
#include "dynarr.h"
#include "lzhtable.h"
#include "record.h"

#define EMPTY_VALUE ((Value){.type = EMPTY_VTYPE})
#define BOOL_VALUE(_value)((Value){.type = BOOL_VTYPE, .content.bool = (_value)})
#define INT_VALUE(_value)((Value){.type = INT_VTYPE, .content.i64 = (_value)})
#define FLOAT_VALUE(_value)((Value){.type = FLOAT_VTYPE, .content.fvalue = (_value)})
#define OBJ_VALUE(_value)((Value){.type = OBJ_VTYPE, .content.obj = (_value)})

#define OBJ_TO_STR(_obj)((Str *)((_obj)->content))
#define OBJ_TO_ARRAY(_obj)((Array *)((_obj)->content))
#define OBJ_TO_LIST(_obj)((DynArr *)((_obj)->content))
#define OBJ_TO_DICT(_obj)((LZHTable *)((_obj)->content))
#define OBJ_TO_RECORD(_obj)((Record *)((_obj)->content))
#define OBJ_TO_NATIVE_FN(_obj)((NativeFn *)((_obj)->content))
#define OBJ_TO_FN(_obj)((Fn *)((_obj)->content))
#define OBJ_TO_CLOSURE(_obj)((Closure *)((_obj)->content))
#define OBJ_TO_NATIVE_MODULE(_obj)((NativeModule *)((_obj)->content))
#define OBJ_TO_MODULE(_obj)((Module *)((_obj)->content))

#define VALUE_TO_BOOL(_value)((_value)->content.bool)
#define VALUE_TO_INT(_value)((_value)->content.i64)
#define VALUE_TO_FLOAT(_value)((_value)->content.fvalue)
#define VALUE_TO_OBJ(_value)((Obj *)((_value)->content.obj))
#define VALUE_TO_STR(_value)(OBJ_TO_STR(VALUE_TO_OBJ(_value)))
#define VALUE_TO_ARRAY(_value)(OBJ_TO_ARRAY(VALUE_TO_OBJ(_value)))
#define VALUE_TO_LIST(_value)(OBJ_TO_LIST(VALUE_TO_OBJ(_value)))
#define VALUE_TO_DICT(_value)(OBJ_TO_DICT(VALUE_TO_OBJ(_value)))
#define VALUE_TO_RECORD(_value)(OBJ_TO_RECORD(VALUE_TO_OBJ(_value)))
#define VALUE_TO_FN(_value)(OBJ_TO_FN(VALUE_TO_OBJ(_value)))
#define VALUE_TO_CLOSURE(_value)(OBJ_TO_CLOSURE(VALUE_TO_OBJ(_value)))
#define VALUE_TO_NATIVE_FN(_value)(OBJ_TO_NATIVE_FN(VALUE_TO_OBJ(_value)))
#define VALUE_TO_NATIVE_MODULE(_value)(OBJ_TO_NATIVE_MODULE(VALUE_TO_OBJ(_value)))
#define VALUE_TO_MODULE(_value)(OBJ_TO_MODULE(VALUE_TO_OBJ(_value)))

#define IS_VALUE_EMPTY(_value)(VALUE_TYPE(_value) == EMPTY_VTYPE)
#define IS_VALUE_BOOL(_value)(VALUE_TYPE(_value) == BOOL_VTYPE)
#define IS_VALUE_INT(_value)(VALUE_TYPE(_value) == INT_VTYPE)
#define IS_VALUE_FLOAT(_value)(VALUE_TYPE(_value) == FLOAT_VTYPE)
#define IS_VALUE_OBJ(_value)(VALUE_TYPE(_value) == OBJ_VTYPE)
#define IS_VALUE_STR(_value)(IS_VALUE_OBJ(_value) && OBJ_TYPE(VALUE_TO_OBJ(_value)) == STR_OTYPE)
#define IS_VALUE_ARRAY(_value)(IS_VALUE_OBJ(_value) && OBJ_TYPE(VALUE_TO_OBJ(_value)) == ARRAY_OTYPE)
#define IS_VALUE_LIST(_value)(IS_VALUE_OBJ(_value) && OBJ_TYPE(VALUE_TO_OBJ(_value)) == LIST_OTYPE)
#define IS_VALUE_DICT(_value)(IS_VALUE_OBJ(_value) && OBJ_TYPE(VALUE_TO_OBJ(_value)) == DICT_OTYPE)
#define IS_VALUE_RECORD(_value)(IS_VALUE_OBJ(_value) && OBJ_TYPE(VALUE_TO_OBJ(_value)) == RECORD_OTYPE)
#define IS_VALUE_FN(_value)(IS_VALUE_OBJ(_value) && OBJ_TYPE(VALUE_TO_OBJ(_value)) == FN_OTYPE)
#define IS_VALUE_CLOSURE(_value)(IS_VALUE_OBJ(_value) && OBJ_TYPE(VALUE_TO_OBJ(_value)) == CLOSURE_OTYPE)
#define IS_VALUE_NATIVE_FN(_value)(IS_VALUE_OBJ(_value) && OBJ_TYPE(VALUE_TO_OBJ(_value)) == NATIVE_FN_OTYPE)
#define IS_VALUE_NATIVE_MODULE(_value)(IS_VALUE_OBJ(_value) && OBJ_TYPE(VALUE_TO_OBJ(_value)) == NATIVE_MODULE_OTYPE)
#define IS_VALUE_MODULE(_value)(IS_VALUE_OBJ(_value) && OBJ_TYPE(VALUE_TO_OBJ(_value)) == MODULE_OTYPE)

#endif