#ifndef LEXER_H
#define LEXER_H

#include "types.h"
#include "dynarr.h"
#include "lzhtable.h"

typedef struct lexer{
    int status;
	int line;
	int start;
	int current;
    RawStr *source;
	DynArrPtr *tokens;
	LZHTable *strings;
    LZHTable *keywords;
}Lexer;

Lexer *lexer_create();
int lexer_scan(
	RawStr *source,
	DynArrPtr *tokens,
	LZHTable *strings,
	LZHTable *keywords,
	Lexer *lexer
);

#endif
