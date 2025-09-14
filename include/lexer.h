#ifndef LEXER_H
#define LEXER_H

#include "types.h"
#include "lzbstr.h"
#include "dynarr.h"
#include "lzohtable.h"
#include <setjmp.h>

typedef struct lexer{
    jmp_buf err_buf;
    int status;
	int line;
	int start;
	int current;
    char *pathname;
    RawStr *source;
	DynArr *tokens;
    LZBStr *str_helper;
    LZOHTable *keywords;
    Allocator *rtallocator;
    Allocator *ctallocator;
    Allocator fake_rtallocator;
    Allocator fake_ctallocator;
}Lexer;

Lexer *lexer_create(Allocator *compile_time_allocator, Allocator *runtime_allocator);

int lexer_scan(
    RawStr *source,
    DynArr *tokens,
    LZOHTable *keywords,
    char *pathname,
    Lexer *lexer
);

#endif
