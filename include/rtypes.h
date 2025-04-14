#ifndef RTYPES_H
#define RTYPES_H

#include "dynarr.h"
#include "lzhtable.h"
#include <stddef.h>

#define NAME_LEN 256
#define OUT_VALUES_LENGTH 255

#define MODULE_TRIES(m)(m->submodule->tries)
#define MODULE_SYMBOLS(m)(m->submodule->symbols)
#define MODULE_STRINGS(m)(m->submodule->strings)
#define MODULE_GLOBALS(m)(m->submodule->globals)

typedef struct vm VM;

//> MODULE RELATED
typedef enum obj_type ObjType;
typedef struct obj Obj;
typedef enum value_type ValueType;
typedef struct value Value;

typedef struct str Str;
typedef struct array Array;
typedef struct record Record;

typedef Value (*RawNativeFn)(uint8_t argc, Value *values, Value *target, VM *vm);
typedef struct native_fn_info NativeFnInfo;
typedef struct native_fn NativeFn;
typedef struct opcode_location OPCodeLocation;
typedef struct fn Fn;

typedef struct meta_out_value MetaOutValue;
typedef struct meta_closure MetaClosure;

typedef struct out_value OutValue;
typedef struct closure Closure;

typedef Value *(*RawForeignFn)(Value *values);
typedef struct foreign_fn ForeignFn;

typedef enum native_module_symbol_type NativeModuleSymbolType;
typedef struct native_module_symbol NativeModuleSymbol;
typedef struct native_module NativeModule;

typedef enum submodule_symbol_type SubModuleSymbolType;
typedef struct submodule_symbol SubModuleSymbol;
typedef struct try_block TryBlock;
typedef struct submodule SubModule;

typedef struct module Module;

typedef struct foreign_lib ForeignLib;

enum obj_type{
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
    FOREIGN_FN_OTYPE,
    NATIVE_LIB_OTYPE,
};

struct obj{
    char marked;
    ObjType type;
    struct obj *prev;
    struct obj *next;

    union{
		Str *str;
        Array *array;
		DynArr *list;
        LZHTable *dict;
		Record *record;
        NativeFn *native_fn;
        Fn *fn;
        Closure *closure;
        ForeignFn *foreign_fn;
        NativeModule *native_module;
        Module *module;
        ForeignLib *native_lib;
	}content;
};

enum value_type{
    EMPTY_VTYPE,
    BOOL_VTYPE,
    INT_VTYPE,
    FLOAT_VTYPE,
	OBJ_VTYPE
};

struct value{
    ValueType type;
    union{
        uint8_t bool;
        int64_t i64;
        double fvalue;
		Obj *obj;
    }content;
};

struct str{
	char runtime;
    uint32_t hash;
	size_t len;
	char *buff;
};

struct array{
    int32_t len;
    Value *values;
};

struct record{
	LZHTable *attributes;
};

struct native_fn_info{
    uint8_t arity;
    RawNativeFn raw_native;
};

struct native_fn{
    uint8_t core;
    uint8_t arity;
    Value target;
    char *name;
    RawNativeFn raw_fn;
};

struct opcode_location{
	size_t offset;
	int line;
    char *filepath;
};

struct fn{
    char *name;
    DynArr *chunks;
    DynArr *params;
	DynArr *locations;
    DynArr *integers;
    DynArr *floats;
    Module *module;
};

struct meta_out_value{
    uint8_t at;
};

struct meta_closure{
    uint8_t values_len;
    MetaOutValue values[OUT_VALUES_LENGTH];
    Fn *fn;
};

struct out_value{
    uint8_t linked;
    uint8_t at;
    Value *value;
    OutValue *prev;
    OutValue *next;
    Obj *closure;
};

struct closure{
    OutValue *out_values;
    MetaClosure *meta;
};

struct foreign_fn{
    RawForeignFn raw_fn;
};

enum native_module_symbol_type{
	NATIVE_FUNCTION_NMSYMTYPE
};

struct native_module_symbol{
	NativeModuleSymbolType type;
	union{
		NativeFn *fn;
	}value;
};

struct native_module{
	char *name;
	LZHTable *symbols;
};

enum submodule_symbol_type{
    FUNCTION_MSYMTYPE,
    CLOSURE_MSYMTYPE,
	NATIVE_MODULE_MSYMTYPE,
    MODULE_MSYMTYPE
};

struct submodule_symbol{
	SubModuleSymbolType type;
	union{
        Fn *fn;
        MetaClosure *meta_closure;
		NativeModule *native_module;
		Module *module;
	}value;
};

struct try_block{
    size_t try;
    size_t catch;
	uint8_t local;
    struct try_block *outer;
};

struct submodule{
    char resolved;
    LZHTable *tries;
	DynArr *symbols;
    LZHTable *strings;
    LZHTable *globals;
};

typedef struct module{
    char shadow;
    char *name;
    char *pathname;
    SubModule *submodule;
}Module;

struct foreign_lib{
    void *handler;
};

#define VALUE_SIZE sizeof(Value)
#define IS_MARKED(o)((o)->marked == 1)

#define IS_EMPTY(v)((v)->type == EMPTY_VTYPE)
#define IS_BOOL(v)((v)->type == BOOL_VTYPE)
#define IS_INT(v)((v)->type == INT_VTYPE)
#define IS_FLOAT(v)((v)->type == FLOAT_VTYPE)
#define IS_OBJ(v)((v)->type == OBJ_VTYPE)
#define IS_STR(v)(IS_OBJ(v) && (v)->content.obj->type == STR_OTYPE)
#define IS_ARRAY(v)(IS_OBJ(v) && (v)->content.obj->type == ARRAY_OTYPE)
#define IS_LIST(v)(IS_OBJ(v) && (v)->content.obj->type == LIST_OTYPE)
#define IS_DICT(v)(IS_OBJ(v) && (v)->content.obj->type == DICT_OTYPE)
#define IS_RECORD(v)(IS_OBJ(v) && (v)->content.obj->type == RECORD_OTYPE)
#define IS_FN(v)(IS_OBJ(v) && (v)->content.obj->type == FN_OTYPE)
#define IS_CLOSURE(v)(IS_OBJ(v) && (v)->content.obj->type == CLOSURE_OTYPE)
#define IS_NATIVE_FN(v)(IS_OBJ(v) && (v)->content.obj->type == NATIVE_FN_OTYPE)
#define IS_NATIVE_MODULE(v)(IS_OBJ(v) && (v)->content.obj->type == NATIVE_MODULE_OTYPE)
#define IS_MODULE(v)(IS_OBJ(v) && (v)->content.obj->type == MODULE_OTYPE)
#define IS_NATIVE_LIBRARY(v)(IS_OBJ(v) && (v)->content.obj->type == NATIVE_LIB_OTYPE)
#define IS_FOREIGN_FN(v)(IS_OBJ(v) && (v)->content.obj->type == FOREIGN_FN_OTYPE)

#define TO_BOOL(v)((v)->content.bool)
#define TO_INT(v)((v)->content.i64)
#define TO_FLOAT(v)((v)->content.fvalue)
#define TO_OBJ(v)((v)->content.obj)
#define TO_STR(v)((v)->content.obj->content.str)
#define TO_ARRAY(v)((v)->content.obj->content.array)
#define TO_LIST(v)((v)->content.obj->content.list)
#define TO_DICT(v)((v)->content.obj->content.dict)
#define TO_RECORD(v)((v)->content.obj->content.record)
#define TO_FN(v)((v)->content.obj->content.fn)
#define TO_CLOSURE(v)((v)->content.obj->content.closure)
#define TO_NATIVE_FN(v)((v)->content.obj->content.native_fn)
#define TO_NATIVE_MODULE(v)((v)->content.obj->content.native_module)
#define TO_MODULE(v)((v)->content.obj->content.module)
#define TO_NATIVE_LIBRARY(v)((v)->content.obj->content.native_lib)
#define TO_FOREIGN_FN(v)((v)->content.obj->content.foreign_fn)

#define EMPTY_VALUE ((Value){.type = EMPTY_VTYPE})
#define BOOL_VALUE(value)((Value){.type = BOOL_VTYPE, .content.bool = (value)})
#define INT_VALUE(value)((Value){.type = INT_VTYPE, .content.i64 = (value)})
#define FLOAT_VALUE(value)((Value){.type = FLOAT_VTYPE, .content.fvalue = (value)})
#define OBJ_VALUE(value)((Value){.type = OBJ_VTYPE, .content.obj = (value)})

#endif