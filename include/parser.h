#ifndef PARSER_H
#define PARSER_H

#include "types.h"
#include "dynarr.h"
#include <stddef.h>
#include <setjmp.h>

typedef struct parser{
    jmp_buf err_buf;
    uint8_t fns_stack_count;
	size_t current;
	DynArr *tokens;
    DynArr *fns_prototypes;
    Allocator *ctallocator;
}Parser;

Parser *parser_create(Allocator *allocator);
int parser_parse(DynArr *tokens, DynArr *fns_prototypes, DynArr *stmt, Parser *parser);
int parser_parse_str_interpolation(DynArr *tokens, DynArr *exprs, Parser *parser);

#endif
