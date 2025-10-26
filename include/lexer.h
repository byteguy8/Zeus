#ifndef LEXER_H
#define LEXER_H

#include "dynarr.h"
#include "lzbstr.h"
#include "lzohtable.h"
#include "lzarena.h"
#include "memory.h"

#include <setjmp.h>

typedef struct lexer{
    jmp_buf err_buf;
    int status;
	int line;
	int start;
	int current;
    char *pathname;
    DStr *source;
	DynArr *tokens;
    LZOHTable *keywords;
    LZArena *ctarena;
    Allocator *ctarena_allocator;
    Allocator *rtallocator;
    Allocator *ctallocator;
}Lexer;

Lexer *lexer_create(Allocator *compile_time_allocator, Allocator *runtime_allocator);

int lexer_scan(
    DStr *source,
    DynArr *tokens,
    LZOHTable *keywords,
    char *pathname,
    Lexer *lexer
);

#endif
