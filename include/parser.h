#ifndef PARSER_H
#define PARSER_H

#include "types.h"
#include "dynarr.h"
#include <stddef.h>
#include <setjmp.h>

typedef struct parser{
    jmp_buf err_jmp;
	size_t current;
	DynArr *tokens;
    Allocator *ctallocator;
}Parser;

Parser *parser_create(Allocator *allocator);
int parser_parse(DynArr *tokens, DynArr *stmt, Parser *parser);
int parser_parse_template(DynArr *tokens, DynArr *exprs, Parser *parser);

#endif
