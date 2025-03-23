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
    char *pathname;
    RawStr *source;
	DynArrPtr *tokens;
	LZHTable *strings;
    LZHTable *keywords;
    Allocator *rtallocator;
}Lexer;

Lexer *lexer_create(Allocator *allocator);
int lexer_scan(RawStr *source, DynArrPtr *tokens, LZHTable *strings, LZHTable *keywords, char *pathname, Lexer *lexer);

#endif
