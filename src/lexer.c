#include "lexer.h"
#include "memory.h"
#include "utils.h"
#include "token.h"
#include "types.h"
#include <stdint.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <math.h>

// PRIVATE INTERFACE
static void error(Lexer *lexer, char *msg, ...);
static int is_at_end(Lexer *lexer);
static int is_digit(char c);
static int is_alpha(char c);
static int is_alpha_numeric(char c);
static char peek(Lexer *lexer);
static int match(char c, Lexer *lexer);
static char advance(Lexer *lexer);
static char *lexeme_range(int start, int end, Lexer *lexer);
static int lexeme_range_copy(
    int start,
    int end,
    char *lexeme,
    size_t lexeme_len,
    Lexer *lexer
);
static size_t lexeme_current_len(Lexer *lexer);
static char *lexeme_current(Lexer *lexer);
static char *lexeme_current_copy(char *lexeme_copy, Lexer *lexer);
static Token *create_token_raw(
	int line, 
    char *lexeme, 
    void *literal, 
    size_t literal_size, 
    TokenType type,
    Lexer *lexer
);
static Token *create_token_literal(
	void *literal,
	size_t literal_size,
	TokenType type,
	Lexer *lexer
);
static Token *create_token(TokenType type, Lexer *lexer);
#define ADD_TOKEN_RAW(token)(dynarr_ptr_insert((token), lexer->tokens))
static void comment(Lexer *lexer);
static Token *number(Lexer *lexer);
static Token *identifier(Lexer *lexer);
static uint32_t *str_to_table(size_t len, char *str, Lexer *lexer);
static Token *string(Lexer *lexer);
static Token *scan_token(char c, Lexer *lexer);

// PRIVATE IMPLEMENTATION
void error(Lexer *lexer, char *msg, ...){
	if(lexer->status == 0){
		lexer->status = 1;
	}

    va_list args;
	va_start(args, msg);

    int line = lexer->line;

	fprintf(stderr, "Lexer error at line %d:\n\t", line);
	vfprintf(stderr, msg, args);
    fprintf(stderr, "\n");

	va_end(args);
}

int is_at_end(Lexer *lexer){
    RawStr *source = lexer->source;
    return lexer->current >= ((int) source->size);
}

int is_digit(char c){
    return c >= '0' && c <= '9';
}

int is_alpha(char c){
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

int is_alpha_numeric(char c){
    return is_digit(c) || is_alpha(c);
}

char peek(Lexer *lexer){
    if(is_at_end(lexer)){
		return '\0';
	}
	
    RawStr *source = lexer->source;
    return source->buff[lexer->current];
}

int match(char c, Lexer *lexer){
    if(is_at_end(lexer)){
		return '\0';
	}
    
    RawStr *source = lexer->source;
    char cc = source->buff[lexer->current];

    if(c != cc){
		return 0;
	}
    
    lexer->current++;
    return 1;
}

char advance(Lexer *lexer){
    if(is_at_end(lexer)){
		return '\0';
	}
	
    RawStr *source = lexer->source;
    return source->buff[lexer->current++];
}

// the output str is valid only during compilation allocated
char *lexeme_range(int start, int end, Lexer *lexer){
    assert(end >= start);

    RawStr *source = lexer->source;
    size_t lexeme_size = end - start;
    char *lexeme = (char *)A_COMPILE_ALLOC(lexeme_size + 1);

    memcpy(lexeme, source->buff + (size_t)start, lexeme_size);
    lexeme[lexeme_size] = '\0';

    return lexeme;
}

int lexeme_range_copy(
    int start,
    int end,
    char *lexeme,
    size_t lexeme_len,
    Lexer *lexer
){
    int len = end - start + 1;

	if(!lexeme){
		return len;
	}
    if(lexeme_len == 0){
		return 0;
	}

    RawStr *source = lexer->source;
	memcpy(lexeme, source->buff + start, lexeme_len);

	return 0;
}

size_t lexeme_current_len(Lexer *lexer){
    return lexer->current - lexer->start;
}

char *lexeme_current(Lexer *lexer){
    return lexeme_range(lexer->start, lexer->current, lexer);
}

char *lexeme_current_copy(char *lexeme_copy, Lexer *lexer){
    size_t len = lexeme_current_len(lexer);
    RawStr *source = lexer->source;

    memcpy(lexeme_copy, source->buff + lexer->start, len);
    lexeme_copy[len] = '\0';

    return lexeme_copy;
}

Token *create_token_raw(
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
    token->extra = NULL;
    
    return token;
}

Token *create_token_literal(
	void *literal,
	size_t literal_size,
	TokenType type,
	Lexer *lexer
){
	char *lexeme = lexeme_current(lexer);
	return create_token_raw(
		lexer->line,
        lexeme,
        literal,
        literal_size,
        type,
        lexer
	);
}

Token *create_token(TokenType type, Lexer *lexer){
	return create_token_literal(NULL, 0, type, lexer);
}

void comment(Lexer *lexer){
	while(!is_at_end(lexer)){
		if(advance(lexer) == '\n'){
			lexer->line++;
			break;
		}
	}
}

Token *number(Lexer *lexer){
    TokenType type = INT_TYPE_TOKTYPE;
    
    while (!is_at_end(lexer) && is_digit(peek(lexer))){
        advance(lexer);
    }
        
	if(match('.', lexer)){
		type = FLOAT_TYPE_TOKTYPE;
		
		while (!is_at_end(lexer) && is_digit(peek(lexer))){
            advance(lexer);
        }
	}

    char *lexeme = lexeme_current(lexer);
    
    if(type == INT_TYPE_TOKTYPE){
		int64_t *value = A_COMPILE_ALLOC(sizeof(int64_t));
        utils_str_to_i64(lexeme, value);

		return create_token_raw(
			lexer->line,
			lexeme,
			value,
			sizeof(int64_t),
			type,
			lexer
		);
	}else{
		double *value = A_COMPILE_ALLOC(sizeof(double));
        utils_str_to_double(lexeme, value);

        return create_token_raw(
			lexer->line,
			lexeme,
			value,
			sizeof(double),
			type,
			lexer
		);
	}
}

Token *identifier(Lexer *lexer){
    while (!is_at_end(lexer) && is_alpha_numeric(peek(lexer))){
        advance(lexer);
    }
    
    size_t lexeme_len = lexeme_current_len(lexer);
    char lexeme[lexeme_len + 1];
    lexeme_current_copy(lexeme, lexer);

    TokenType *type = (TokenType *)lzhtable_get(
        (uint8_t *)lexeme, 
        lexeme_len, 
        lexer->keywords
    );

    if(type == NULL){
        return create_token(IDENTIFIER_TOKTYPE, lexer);
    }else{
        return create_token(*type, lexer);
    }
}

// str must be runtime allocated
uint32_t *str_to_table(size_t len, char *str, Lexer *lexer){
    uint32_t *hash = A_COMPILE_ALLOC(sizeof(uint32_t));
	*hash = lzhtable_hash((uint8_t *)str, len);

	if(!lzhtable_hash_contains(*hash, lexer->strings, NULL)){
		lzhtable_hash_put(*hash, str, lexer->strings);
	}

    return hash;
}

Token *string(Lexer *lexer){
    int from = 0;
	int lines = 0;
    DynArrPtr *tokens = NULL;
    TokenType type = STR_TYPE_TOKTYPE;
    BStr *bstr = bstr_create_empty(NULL);

    if(!bstr){
        error(lexer, "out of memory");
        return NULL;
    }
	
	while(!is_at_end(lexer) && peek(lexer) != '"'){
		char c = advance(lexer);
		
		if(c == '\n'){
			lines++;
		}else if(c == '\\'){
            size_t escape_start = lexer->current - 1;
            size_t escape_end = lexer->current;
            
            char escape;

            if(peek(lexer) == '0'){escape = '\0';}
            else if(peek(lexer) == 'a'){escape = '\a';}
            else if(peek(lexer) == 'b'){escape = '\b';}
            else if(peek(lexer) == 'f'){escape = '\f';}
            else if(peek(lexer) == 'n'){escape = '\n';}
            else if(peek(lexer) == 'r'){escape = '\r';}
            else if(peek(lexer) == 't'){escape = '\t';}
            else if(peek(lexer) == 'v'){escape = '\v';}
            else if(peek(lexer) == '\\'){escape = '\\';}
            else if(peek(lexer) == '\"'){escape = '\"';}
            else{
                int len = escape_end - escape_start + 1;
                char sequence[len + 1];

                lexeme_range_copy(escape_start, escape_end, sequence, len, lexer);
                sequence[len] = 0;

                error(lexer, "unknown scape sequence %s", sequence);
                
                advance(lexer);

                continue;
            }

            if(bstr_append_range(&escape, 0, 0, bstr)){
                bstr_destroy(bstr);
                error(lexer, "Out of memory");
                return NULL;
            }

            advance(lexer);
        }else if(c == '$'){
            size_t interpolation_start = lexer->current - 1;

			if(match('{', lexer)){
                if(type != TEMPLATE_TYPE_TOKTYPE) type = TEMPLATE_TYPE_TOKTYPE;
                if(!tokens) tokens = compile_dynarr_ptr();

                if(bstr->used > 0){
                    size_t start = from;
                    size_t len = bstr->used - start + 1;
                    char *str = runtime_clone_str_range(start, len, (char *)bstr->buff);
        
                    uint32_t *hash = str_to_table(len, str,lexer);
                    Token *str_token = create_token_literal(
                        hash,
                        sizeof(uint32_t),
                        STR_TYPE_TOKTYPE,
                        lexer
                    );

                    dynarr_ptr_insert(str_token, tokens);
                }
				
                size_t prev_start = lexer->start;
                lexer->start = lexer->current;

				while(!is_at_end(lexer) && peek(lexer) != '}'){
					c = advance(lexer);

					Token *token = scan_token(c, lexer);
					
                    if(token){
                        dynarr_ptr_insert(token, tokens);
					}

                    lexer->start = lexer->current;
				}
				
                lexer->start = prev_start;

				if(peek(lexer) != '}'){
					error(lexer, "unterminated placeholder");
					return NULL;
				}

                size_t interpolation_end = lexer->current;
                
                if(bstr_append_range(lexer->source->buff, interpolation_start, interpolation_end, bstr)){
                    bstr_destroy(bstr);
                    error(lexer, "Out of memory");
                    return NULL;
                }

                advance(lexer);
				
				from = bstr->used;
			}
		}else{
            if(bstr_append_range(&c, 0, 0, bstr)){
                bstr_destroy(bstr);
                error(lexer, "Out of memory");
                return NULL;
            }
        }
	}

    if(type == TEMPLATE_TYPE_TOKTYPE){
        if(bstr->len - from + 1 > 0){
            size_t start = from;
            size_t len = bstr->used - start + 1;
            char *str = runtime_clone_str_range(start, len, (char *)bstr->buff);
            
            uint32_t *hash = str_to_table(len, str,lexer);
            Token *str_token = create_token_literal(
                hash,
                sizeof(uint32_t),
                STR_TYPE_TOKTYPE,
                lexer
            );

            dynarr_ptr_insert(str_token, tokens);
        }
        
        Token *eof = create_token(EOF_TOKTYPE, lexer);
        
        dynarr_ptr_insert(eof, tokens);
    }

	if(peek(lexer) != '"'){
		error(lexer, "Unterminated string");
		return NULL;
	}

    advance(lexer);

    size_t str_len = bstr->used;
    char *str = runtime_clone_str_range(0, bstr->used, (char *)bstr->buff);

	uint32_t *hash = str_to_table(str_len, str, lexer);
    Token *str_token = create_token_literal(
		hash,
		sizeof(uint32_t),
		type,
		lexer
	);

	str_token->extra = tokens;
	lexer->line += lines;
	
    bstr_destroy(bstr);

	return str_token;
}

Token *scan_token(char c, Lexer *lexer){
    switch (c)
    {
        case '+':{
			if(match('=', lexer)){
				return create_token(COMPOUND_ADD_TOKTYPE, lexer);
			}else{
				return create_token(PLUS_TOKTYPE, lexer);
			}
        }
        case '-':{
			if(match('=', lexer)){
				return create_token(COMPOUND_SUB_TOKTYPE, lexer);
			}else{
				return create_token(MINUS_TOKTYPE, lexer);
			}
        }
        case '*':{
			if(match('=', lexer)){
				return create_token(COMPOUND_MUL_TOKTYPE, lexer);
			}else{
				return create_token(ASTERISK_TOKTYPE, lexer);
			}
        }
        case '/':{
			if(match('/', lexer)){
				comment(lexer);
				return NULL;
			}else if(match('=', lexer)){
				return create_token(COMPOUND_DIV_TOKTYPE, lexer);
			}else{
				return create_token(SLASH_TOKTYPE, lexer);
			}
        }
		case ',':{
			return create_token(COMMA_TOKTYPE, lexer);
		}
		case '.':{
			return create_token(DOT_TOKTYPE, lexer);
		}
        case '<':{
            if(match('=', lexer)){
				return create_token(LESS_EQUALS_TOKTYPE, lexer);
			}else{
				return create_token(LESS_TOKTYPE, lexer);
			}
        }
        case '>':{
            if(match('=', lexer)){
				return create_token(GREATER_EQUALS_TOKTYPE, lexer);
			}else{
				return create_token(GREATER_TOKTYPE, lexer);
			}
        }
        case '=':{
            if(match('=', lexer)){
				return create_token(EQUALS_EQUALS_TOKTYPE, lexer);
			}else{
				return create_token(EQUALS_TOKTYPE, lexer);
			}
        }
        case '!':{
            if(match('=', lexer)){
				return create_token(NOT_EQUALS_TOKTYPE, lexer);
			}else{
				return create_token(EXCLAMATION_TOKTYPE, lexer);
			}
        }
		case ':':{
			return create_token(COLON_TOKTYPE, lexer);
		}
        case ';':{
            return create_token(SEMICOLON_TOKTYPE, lexer);
        }
        case '(':{
            return create_token(LEFT_PAREN_TOKTYPE, lexer);
        }
        case ')':{
            return create_token(RIGHT_PAREN_TOKTYPE, lexer);
        }
        case '{':{
            return create_token(LEFT_BRACKET_TOKTYPE, lexer);
        }
        case '}':{
            return create_token(RIGHT_BRACKET_TOKTYPE, lexer);
        }
        case '\n':{
            lexer->line++;
            return NULL;
        }
        case ' ':
        case '\t':
            return NULL;
        default:{
            if(is_digit(c)){
				return number(lexer);
			}else if(is_alpha_numeric(c)){
				return identifier(lexer);
			}else if(c == '"'){
				return string(lexer);
			}else{
				error(lexer, "Unknown token '%c'", c);
				return NULL;
			}
        }
    }
    
    return NULL;
}

// PUBLIC IMPLEMENTATION
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
		char c = advance(lexer);
        Token *token = scan_token(c, lexer);
        
        if(token){
			ADD_TOKEN_RAW(token);
		}
        
        lexer->start = lexer->current;
    }
    
    ADD_TOKEN_RAW(create_token(EOF_TOKTYPE, lexer));

    return lexer->status;
}
