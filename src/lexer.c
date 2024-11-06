#include "lexer.h"
#include "memory.h"
#include "token.h"
#include <stdint.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

static void error(Lexer *lexer, char *msg, ...){
	if(lexer->status == 0) lexer->status = 1;

    va_list args;
	va_start(args, msg);

    int line = lexer->line;

	fprintf(stderr, "Lexer error at line %d:\n\t", line);
	vfprintf(stderr, msg, args);
    fprintf(stderr, "\n");

	va_end(args);
}

static int str_to_i64(char *str, int64_t *out_value){
    int len = (int)strlen(str);
    int is_negative = 0;
    int64_t value = 0;
    
    for(int i = 0; i < len; i++){
        char c = str[i];
        
        if(c == '-' && i == 0){
            is_negative = 1;
            continue;
        }
        
        if(c < '0' || c > '9')
            return 1;
            
        int64_t digit = ((int64_t)c) - 48;
        
        value *= 10;
        value += digit;
    }
    
    if(is_negative == 1)
        value *= -1;
        
    *out_value = value;
    
    return 0;
}

static int is_at_end(Lexer *lexer){
    RawStr *source = lexer->source;
    return lexer->current >= ((int) source->size);
}

static int is_digit(char c){
    return c >= '0' && c <= '9';
}

static int is_alpha(char c){
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static char peek(Lexer *lexer){
    if(is_at_end(lexer)) return '\0';
    RawStr *source = lexer->source;
    return source->buff[lexer->current];
}

static int match(char c, Lexer *lexer){
    if(is_at_end(lexer)) return '\0';
    
    RawStr *source = lexer->source;
    char cc = source->buff[lexer->current];

    if(c != cc) return 0;
    
    lexer->current++;
    return 1;
}

static char advance(Lexer *lexer){
    if(is_at_end(lexer)) return '\0';
    RawStr *source = lexer->source;
    return source->buff[lexer->current++];
}

static char *lexeme_range(int start, int end, Lexer *lexer){
    assert(end >= start);

    RawStr *source = lexer->source;
    size_t lexeme_size = end - start;
    char *lexeme = (char *)memory_alloc(lexeme_size + 1);

    memcpy(lexeme, source->buff + (size_t)start, lexeme_size);
    lexeme[lexeme_size] = '\0';

    return lexeme;
}

static size_t lexeme_current_len(Lexer *lexer){
    return lexer->current - lexer->start + 1;
}

static char *lexeme_current(Lexer *lexer){
    return lexeme_range(lexer->start, lexer->current, lexer);
}

static char *lexeme_current_copy(char *lexeme_copy, Lexer *lexer){
    size_t len = lexeme_current_len(lexer);
    RawStr *source = lexer->source;

    memcpy(lexeme_copy, source->buff + lexer->start, len - 1);
    lexeme_copy[len] = '\0';

    return lexeme_copy;
}

static void add_token_raw(
    int line, 
    char *lexeme, 
    void *literal, 
    size_t literal_size, 
    TokenType type, 
    Lexer *lexer
){
    Token *token = (Token *)memory_alloc(sizeof(Token));

    token->line = line;
    token->lexeme = lexeme;
    token->literal = literal;
    token->literal_size = literal_size;
    token->type = type;

    dynarr_ptr_insert(token, lexer->tokens);
}

static void add_token_literal(void *literal, size_t literal_size, TokenType type, Lexer *lexer){
    char *lexeme = lexeme_current(lexer);
    add_token_raw(
        lexer->line,
        lexeme,
        literal,
        literal_size,
        type,
        lexer
    );
}

static void add_token(TokenType type, Lexer *lexer){
    add_token_literal(NULL, 0, type, lexer);
}

static void number(Lexer *lexer){
    while (!is_at_end(lexer) && is_digit(peek(lexer)))
        advance(lexer);

    char *lexeme = lexeme_current(lexer);
    int64_t *value = memory_alloc(sizeof(int64_t));

    str_to_i64(lexeme, value);
    add_token_raw(
        lexer->line,
        lexeme,
        value,
        sizeof(int64_t),
        INT_TOKTYPE,
        lexer
    );
}

static void identifier(Lexer *lexer){
    while (!is_at_end(lexer) && is_alpha(peek(lexer)))
        advance(lexer);
    
    size_t lexeme_len = lexeme_current_len(lexer);
    char lexeme[lexeme_len];
    lexeme_current_copy(lexeme, lexer);

    TokenType *type = (TokenType *)lzhtable_get(
        (uint8_t *)lexeme, 
        lexeme_len - 1, 
        lexer->keywords);

    if(type == NULL) add_token(IDENTIFIER_TOKTYPE, lexer);
    else add_token(*type, lexer);
}

static void scan_token(Lexer *lexer){
    char c = advance(lexer);

    switch (c)
    {
        case '+':{
            add_token(PLUS_TOKTYPE, lexer);
            break;
        }
        case '-':{
            add_token(MINUS_TOKTYPE, lexer);
            break;
        }
        case '*':{
            add_token(ASTERISK_TOKTYPE, lexer);
            break;
        }
        case '/':{
            add_token(SLASH_TOKTYPE, lexer);
            break;
        }
        case '<':{
            if(match('=', lexer)) add_token(LESS_EQUALS_TOKTYPE, lexer);
            else add_token(LESS_TOKTYPE, lexer);
            break;
        }
        case '>':{
            if(match('=', lexer)) add_token(GREATER_EQUALS_TOKTYPE, lexer);
            else add_token(GREATER_TOKTYPE, lexer);
            break;
        }
        case '=':{
            if(match('=', lexer)) add_token(EQUALS_EQUALS_TOKTYPE, lexer);
            else add_token(EQUALS_TOKTYPE, lexer);
            break;
        }
        case '!':{
            if(match('=', lexer)) add_token(NOT_EQUALS_TOKTYPE, lexer);
            else error(lexer, "Unknown token '%c'", c);
            break;
        }
        case ';':{
            add_token(SEMICOLON_TOKTYPE, lexer);
            break;
        }
        case '(':{
            add_token(LEFT_PAREN_TOKTYPE, lexer);
            break;
        }
        case ')':{
            add_token(RIGHT_PAREN_TOKTYPE, lexer);
            break;
        }
        case '{':{
            add_token(LEFT_BRACKET_TOKTYPE, lexer);
            break;
        }
        case '}':{
            add_token(RIGHT_BRACKET_TOKTYPE, lexer);
            break;
        }
        case '\n':{
            lexer->line++;
            break;
        }
        case ' ':
        case '\t':
            break;
        default:{
            if(is_digit(c)) number(lexer);
            else if(is_alpha(c)) identifier(lexer);
            else error(lexer, "Unknown token '%c'", c);
            break;
        }
    }
}

Lexer *lexer_create(){
    Lexer *lexer = (Lexer *)memory_alloc(sizeof(Lexer));
    memset(lexer, 0, sizeof(Lexer));
    return lexer;
}

int lexer_scan(RawStr *source, DynArrPtr *tokens, LZHTable *keywords, Lexer *lexer){
    lexer->line = 1;
    lexer->start = 0;
    lexer->current = 0;
    lexer->source = source;
    lexer->tokens = tokens;
    lexer->keywords = keywords;

    while (!is_at_end(lexer)){
        scan_token(lexer);
        lexer->start = lexer->current;
    }
    
    add_token(EOF_TOKTYPE, lexer);

    return lexer->status;
}