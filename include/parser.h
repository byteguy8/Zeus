#ifndef PARSER_H
#define PARSER_H

#include "dynarr.h"
#include <stddef.h>
#include <setjmp.h>

typedef struct parser{
    jmp_buf err_jmp;
	size_t current;
	DynArrPtr *tokens;
	DynArrPtr *stmt;
}Parser;

Parser *parser_create();
int parser_parse(DynArrPtr *tokens, DynArrPtr *stmt, Parser *parser);
int parser_parse_template(DynArrPtr *tokens, DynArrPtr *exprs, Parser *parser);

#endif
