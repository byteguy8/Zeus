#ifndef TRANSFORMER_H
#define TRANSFORMER_H

#include "value.h"
#include "obj.h"
#include "dynarr.h"

#define EMPTY_VALUE ((Value){.type = EMPTY_VTYPE})
#define BOOL_VALUE(_value)((Value){.type = BOOL_VTYPE, .content.bool = (_value)})
#define INT_VALUE(_value)((Value){.type = INT_VTYPE, .content.ivalue = (_value)})
#define FLOAT_VALUE(_value)((Value){.type = FLOAT_VTYPE, .content.fvalue = (_value)})
#define OBJ_VALUE(_value)((Value){.type = OBJ_VTYPE, .content.obj = (_value)})

#define IS_VALUE_EMPTY(_value)((_value).type == EMPTY_VTYPE)
#define IS_VALUE_BOOL(_value)((_value).type == BOOL_VTYPE)
#define IS_VALUE_INT(_value)((_value).type == INT_VTYPE)
#define IS_VALUE_FLOAT(_value)((_value).type == FLOAT_VTYPE)
#define IS_VALUE_OBJ(_value)((_value).type == OBJ_VTYPE)

static inline int vm_is_value_numeric(Value value){
    return value.type == INT_VTYPE || value.type == FLOAT_VTYPE;
}

static inline int is_callable(Value *value){
    ValueType type = value->type;

    if(type != OBJ_VTYPE){
        return 0;
    }

    ObjType obj_type = ((Obj *)value->content.obj)->type;

    return obj_type == FN_OBJ_TYPE ||
           obj_type == CLOSURE_OBJ_TYPE;
}

static inline int is_value_str(Value value){
    return value.type == OBJ_VTYPE && ((Obj *)(value.content.obj))->type == STR_OBJ_TYPE;
}

static inline int is_value_array(Value value){
    return value.type == OBJ_VTYPE && ((Obj *)(value.content.obj))->type == ARRAY_OBJ_TYPE;
}

static inline int is_value_list(Value value){
    return value.type == OBJ_VTYPE && ((Obj *)(value.content.obj))->type == LIST_OBJ_TYPE;
}

static inline int is_value_dict(Value value){
    return value.type == OBJ_VTYPE && ((Obj *)(value.content.obj))->type == DICT_OBJ_TYPE;
}

static inline int is_value_record(Value value){
    return value.type == OBJ_VTYPE && ((Obj *)(value.content.obj))->type == RECORD_OBJ_TYPE;
}

static inline int is_value_native(Value value){
	return value.type == OBJ_VTYPE && ((Obj *)(value.content.obj))->type == NATIVE_OBJ_TYPE;
}

static inline int is_value_native_fn(Value value){
    return value.type == OBJ_VTYPE && ((Obj *)(value.content.obj))->type == NATIVE_FN_OBJ_TYPE;
}

static inline int is_value_fn(Value value){
    return value.type == OBJ_VTYPE && ((Obj *)(value.content.obj))->type == FN_OBJ_TYPE;
}

static inline int is_value_closure(Value value){
    return value.type == OBJ_VTYPE && ((Obj *)(value.content.obj))->type == CLOSURE_OBJ_TYPE;
}

static inline int is_value_native_module(Value value){
    return value.type == OBJ_VTYPE && ((Obj *)(value.content.obj))->type == NATIVE_MODULE_OBJ_TYPE;
}

static inline int is_value_module(Value value){
    return value.type == OBJ_VTYPE && ((Obj *)(value.content.obj))->type == MODULE_OBJ_TYPE;
}

#define VALUE_TO_BOOL(_value)((_value).content.bool)
#define VALUE_TO_INT(_value)((_value).content.ivalue)
#define VALUE_TO_FLOAT(_value)((_value).content.fvalue)
#define VALUE_TO_OBJ(_value)((Obj *)(_value).content.obj)
#define VALUE_TO_STR(_value)((StrObj *)(_value).content.obj)
#define VALUE_TO_ARRAY(_value)((ArrayObj *)(_value).content.obj)
#define VALUE_TO_LIST(_value)((ListObj*)(_value).content.obj)
#define VALUE_TO_DICT(_value)((DictObj *)(_value).content.obj)
#define VALUE_TO_RECORD(_value)((RecordObj *)(_value).content.obj)
#define VALUE_TO_NATIVE(_value)((NativeObj *)(_value).content.obj)
#define VALUE_TO_NATIVE_FN(_value)((NativeFnObj *)(_value).content.obj)
#define VALUE_TO_FN(_value)((FnObj *)(_value).content.obj)
#define VALUE_TO_CLOSURE(_value)((ClosureObj *)(_value).content.obj)

#define OBJ_TO_STR(_obj)((StrObj *)(_obj))
#define OBJ_TO_ARRAY(_obj)((ArrayObj *)(_obj))
#define OBJ_TO_LIST(_obj)((ListObj *)(_obj))
#define OBJ_TO_DICT(_obj)((DictObj *)(_obj))
#define OBJ_TO_RECORD(_obj)((RecordObj *)(_obj))
#define OBJ_TO_NATIVE(_obj)((NativeObj *)(_obj))
#define OBJ_TO_NATIVE_FN(_obj)((NativeFnObj *)(_obj))
#define OBJ_TO_FN(_obj)((FnObj *)(_obj))
#define OBJ_TO_CLOSURE(_obj)((ClosureObj *)(_obj))
#define OBJ_TO_NATIVE_MODULE(_obj)((NativeModuleObj *)(_obj))
#define OBJ_TO_MODULE(_obj)((ModuleObj *)(_obj))

#endif
