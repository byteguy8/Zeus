#ifndef VM_H
#define VM_H

#include "value.h"
#include "dynarr.h"
#include "lzhtable.h"

#define TEMPS_SIZE 255
#define LOCALS_SIZE 255

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
