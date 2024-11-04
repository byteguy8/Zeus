#ifndef PARSER_H
#define PARSER_H

#include "dynarr.h"
#include <stddef.h>

typedef struct parser{
	size_t current;
	DynArrPtr *tokens;
	DynArrPtr *stmt;
}Parser;

Parser *parser_create();
int parser_parse(DynArrPtr *tokens, DynArrPtr *stmt, Parser *parser);

#endif
