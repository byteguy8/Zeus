#ifndef LEXER_H
#define LEXER_H

#include "types.h"
#include "dynarr.h"
#include "lzhtable.h"

typedef struct scanner{
    int status;
	int line;
	int start;
	int current;
    RawStr *source;
	DynArrPtr *tokens;
    LZHTable *keywords;
}Lexer;

Lexer *lexer_create();
int lexer_scan(RawStr *source, DynArrPtr *tokens, LZHTable *keywords, Lexer *lexer);

#endif
