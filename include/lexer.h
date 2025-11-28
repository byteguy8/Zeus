#ifndef LEXER_H
#define LEXER_H

#include "essentials/dynarr.h"
#include "essentials/lzbstr.h"
#include "essentials/lzohtable.h"
#include "essentials/lzarena.h"
#include "essentials/memory.h"

#include "types.h"

#include <setjmp.h>

typedef struct lexer{
    jmp_buf err_buf;
    int status;
	int line;
	int start;
	int current;
    const char *pathname;
    const DStr *source;
	DynArr *tokens;
    const LZOHTable *keywords;
    LZArena *ctarena;
    Allocator *ctarena_allocator;

    const Allocator *rtallocator;
    const Allocator *ctallocator;
}Lexer;

Lexer *lexer_create(const Allocator *compile_time_allocator, const Allocator *runtime_allocator);

int lexer_scan(
    const DStr *source,
    DynArr *tokens,
    const LZOHTable *keywords,
    const char *pathname,
    Lexer *lexer
);

#endif
