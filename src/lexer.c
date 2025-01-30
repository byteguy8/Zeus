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

static double str_to_double(char *str, double *out_value){
    int len = (int)strlen(str);
    int is_negative = 0;
    int is_decimal = 0;
    double special = 10.0;
    double value = 0.0;
    
    for(int i = 0; i < len; i++){
        char c = str[i];
        
        if(c == '-' && i == 0){
            is_negative = 1;
            continue;
        }
        
        if(c == '.'){
			if(i == 0) return 1;
			is_decimal = 1;
			continue;
		}
        
        if(c < '0' || c > '9')
            return 1;
            
        int digit = ((int)c) - 48;
        
        if(is_decimal){
			double decimal = digit / special;
			value += decimal;
			special *= 10.0;
		}else{
			value *= 10.0;
			value += digit;
		}
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
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int is_alpha_numeric(char c){
    return is_digit(c) || is_alpha(c);
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
    char *lexeme = (char *)A_COMPILE_ALLOC(lexeme_size + 1);

    memcpy(lexeme, source->buff + (size_t)start, lexeme_size);
    lexeme[lexeme_size] = '\0';

    return lexeme;
}

static int lexeme_range_copy(
    int start,
    int end,
    char *lexeme,
    size_t lexeme_len,
    Lexer *lexer
){
    int len = end - start + 1;

	if(!lexeme) return len;
    if(lexeme_len == 0) return 0;

    RawStr *source = lexer->source;
	memcpy(lexeme, source->buff + start, lexeme_len);

	return 0;
}

static size_t lexeme_current_len(Lexer *lexer){
    return lexer->current - lexer->start;
}

static char *lexeme_current(Lexer *lexer){
    return lexeme_range(lexer->start, lexer->current, lexer);
}

static char *lexeme_current_copy(char *lexeme_copy, Lexer *lexer){
    size_t len = lexeme_current_len(lexer);
    RawStr *source = lexer->source;

    memcpy(lexeme_copy, source->buff + lexer->start, len);
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
    Token *token = (Token *)A_COMPILE_ALLOC(sizeof(Token));

    token->line = line;
    token->lexeme = lexeme;
    token->literal = literal;
    token->literal_size = literal_size;
    token->type = type;
    token->pathname = lexer->pathname;

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

static void comment(Lexer *lexer){
	while(!is_at_end(lexer)){
		if(advance(lexer) == '\n'){
			lexer->line++;
			break;
		}
	}
}

static void number(Lexer *lexer){
    TokenType type = INT_TYPE_TOKTYPE;
    
    while (!is_at_end(lexer) && is_digit(peek(lexer)))
		advance(lexer);
        
	if(match('.', lexer)){
		type = FLOAT_TYPE_TOKTYPE;
		
		while (!is_at_end(lexer) && is_digit(peek(lexer)))
			advance(lexer);
	}

    char *lexeme = lexeme_current(lexer);
    
    if(type == INT_TYPE_TOKTYPE){
		int64_t *value = A_COMPILE_ALLOC(sizeof(int64_t));

		str_to_i64(lexeme, value);
		add_token_raw(
			lexer->line,
			lexeme,
			value,
			sizeof(int64_t),
			type,
			lexer
		);
	}else{
		double *value = A_COMPILE_ALLOC(sizeof(double));
		
		str_to_double(lexeme, value);
		add_token_raw(
			lexer->line,
			lexeme,
			value,
			sizeof(double),
			type,
			lexer
		);
	}
}

static void identifier(Lexer *lexer){
    while (!is_at_end(lexer) && is_alpha_numeric(peek(lexer)))
        advance(lexer);
    
    size_t lexeme_len = lexeme_current_len(lexer);
    char lexeme[lexeme_len + 1];
    lexeme_current_copy(lexeme, lexer);

    TokenType *type = (TokenType *)lzhtable_get(
        (uint8_t *)lexeme, 
        lexeme_len, 
        lexer->keywords);

    if(type == NULL) add_token(IDENTIFIER_TOKTYPE, lexer);
    else add_token(*type, lexer);
}

static void string(Lexer *lexer){
	int lines = 0;
    char empty = peek(lexer) == '"';
	
	while(peek(lexer) != '"')
		if(advance(lexer) == '\n') lines++;

	if(peek(lexer) != '"'){
		error(lexer, "Unterminated string");
		return;
	}

	size_t len = empty ? 0 : lexeme_range_copy(
        lexer->start + 1,
        lexer->current - 1,
        NULL,
        0,
        lexer
    );
	char str[len + 1];

	lexeme_range_copy(
        lexer->start + 1,
        lexer->current - 1,
        str,
        empty ? 0 : len,
        lexer
    );
    str[len] = 0;

    advance(lexer);

	uint32_t *hash = A_COMPILE_ALLOC(sizeof(uint32_t));
	*hash = lzhtable_hash((uint8_t *)str, len);

	if(!lzhtable_hash_contains(*hash, lexer->strings, NULL)){
		char *literal = A_RUNTIME_ALLOC(len + 1);
		memcpy(literal, str, len);
		literal[len] = '\0';

		lzhtable_hash_put(*hash, literal, lexer->strings);
	}

	add_token_literal(hash, sizeof(uint32_t), STR_TYPE_TOKTYPE, lexer);
	lexer->line += lines;
}

static void scan_token(Lexer *lexer){
    char c = advance(lexer);

    switch (c)
    {
        case '+':{
			if(match('=', lexer)) add_token(COMPOUND_ADD_TOKTYPE, lexer);
			else add_token(PLUS_TOKTYPE, lexer);
            break;
        }
        case '-':{
			if(match('=', lexer)) add_token(COMPOUND_SUB_TOKTYPE, lexer);
			else add_token(MINUS_TOKTYPE, lexer);
            break;
        }
        case '*':{
			if(match('=', lexer)) add_token(COMPOUND_MUL_TOKTYPE, lexer);
            else add_token(ASTERISK_TOKTYPE, lexer);
            break;
        }
        case '/':{
			if(match('/', lexer)) comment(lexer);
			else if(match('=', lexer)) add_token(COMPOUND_DIV_TOKTYPE, lexer);
            else add_token(SLASH_TOKTYPE, lexer);
            break;
        }
		case ',':{
			add_token(COMMA_TOKTYPE, lexer);
			break;
		}
		case '.':{
			add_token(DOT_TOKTYPE, lexer);
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
            else add_token(EXCLAMATION_TOKTYPE, lexer);
            break;
        }
		case ':':{
			add_token(COLON_TOKTYPE, lexer);
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
            else if(is_alpha_numeric(c)) identifier(lexer);
			else if(c == '"') string(lexer);
            else error(lexer, "Unknown token '%c'", c);
            break;
        }
    }
}

Lexer *lexer_create(){
    Lexer *lexer = (Lexer *)A_COMPILE_ALLOC(sizeof(Lexer));
    memset(lexer, 0, sizeof(Lexer));
    return lexer;
}

int lexer_scan(
	RawStr *source,
	DynArrPtr *tokens,
	LZHTable *strings,
	LZHTable *keywords,
    char *pathname,
	Lexer *lexer
){
    lexer->line = 1;
    lexer->start = 0;
    lexer->current = 0;
    lexer->source = source;
    lexer->tokens = tokens;
	lexer->strings = strings;
    lexer->keywords = keywords;
    lexer->pathname = pathname;

    while (!is_at_end(lexer)){
        scan_token(lexer);
        lexer->start = lexer->current;
    }
    
    add_token(EOF_TOKTYPE, lexer);

    return lexer->status;
}
