#ifndef VM_H
#define VM_H

#include "dynarr.h"
#include "lzhtable.h"

#define TEMPS_SIZE 255
#define LOCALS_SIZE 255

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

typedef struct vm
{
    size_t ip;

    int temps_ptr;
    Value temps[TEMPS_SIZE];
    
    Value locals[LOCALS_SIZE];

    DynArr *constants;
	LZHTable *strings;
    DynArr *chunks;

    Obj *head;
    Obj *tail;
}VM;

VM *vm_create();
void vm_print_stack(VM *vm);
int vm_execute(DynArr *constants, LZHTable *strings, DynArr *chunks, VM *vm);

#endif
