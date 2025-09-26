#include "lexer.h"
#include "memory.h"
#include "factory.h"
#include "utils.h"
#include "token.h"
#include <stdint.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <math.h>

#define COMPILE_ALLOCATOR (lexer->ctallocator)
#define RUNTIME_ALLOCATOR (lexer->rtallocator)
//--------------------------------------------------------------------------//
//                            PRIVATE INTERFACE                             //
//--------------------------------------------------------------------------//
static void error(Lexer *lexer, char *msg, ...);

static inline int is_at_end(Lexer *lexer);
static inline int is_dec_digit(char c);
static inline int is_hex_digit(char c);
static inline int is_alpha(char c);
static inline int is_alpha_numeric(char c);

static inline char peek(Lexer *lexer);
static int match(char c, Lexer *lexer);
static char advance(Lexer *lexer);

static char *copy_source_range(size_t start, size_t end, Lexer *lexer, size_t *out_len);
static char *create_lexeme(char *lexeme, Lexer *lexer, size_t *out_len);
static inline size_t current_lexeme_len(Lexer *lexer);
static inline char *current_lexeme(Lexer *lexer, size_t *out_len);
static inline void put_current_lexeme(char *buff, Lexer *lexer);

static inline Token *create_token_raw(
	int line,
    TokType type,
    size_t lexeme_len,
    char *lexeme,
    size_t literal_size,
    void *literal,
    Lexer *lexer
);
static inline Token *create_token_literal(
	TokType type,
    size_t literal_size,
    void *literal,
	Lexer *lexer
);
static inline Token *create_token(TokType type, Lexer *lexer);
#define ADD_TOKEN_RAW(_token)(dynarr_insert_ptr((_token), lexer->tokens))

static void comment(Lexer *lexer);
static Token *decimal(Lexer *lexer);
static Token *identifier(Lexer *lexer);
static int interpolation(DynArr *tokens, Lexer *lexer);
static Token *string(Lexer *lexer);
static Token *scan_token(char c, Lexer *lexer);
//--------------------------------------------------------------------------//
//                          PRIVATE IMPLEMENTATION                          //
//--------------------------------------------------------------------------//
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

static inline int is_at_end(Lexer *lexer){
    RawStr *source = lexer->source;
    return lexer->current >= ((int)source->len);
}

static inline int is_dec_digit(char c){
    return c >= '0' && c <= '9';
}

static inline int is_hex_digit(char c){
    return is_dec_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static inline int is_alpha(char c){
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static inline int is_alpha_numeric(char c){
    return is_dec_digit(c) || is_alpha(c);
}

static inline char peek(Lexer *lexer){
    if(is_at_end(lexer)){
		return '\0';
	}

    RawStr *source = lexer->source;

    return source->buff[lexer->current];
}

static inline char peek_at(uint16_t offset, Lexer *lexer){
    if(is_at_end(lexer)){
		return '\0';
	}

    RawStr *source = lexer->source;

    return source->buff[(size_t)offset];
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
static char *copy_source_range(size_t start, size_t end, Lexer *lexer, size_t *out_len){
    RawStr *source = lexer->source;

    assert(end > start && (size_t)end <= source->len);

    size_t lexeme_len = end - start;
    char *lexeme = MEMORY_ALLOC(char, lexeme_len + 1, lexer->rtallocator);

    memcpy(lexeme, source->buff + (size_t)start, lexeme_len);
    lexeme[lexeme_len] = '\0';

    if(out_len){
        *out_len = lexeme_len;
    }

    return lexeme;
}

static char *create_lexeme(char *lexeme, Lexer *lexer, size_t *out_len){
    size_t lexeme_len = strlen(lexeme);
    char *new_lexeme = MEMORY_ALLOC(char, lexeme_len + 1, COMPILE_ALLOCATOR);

    memcpy(new_lexeme, lexeme, lexeme_len);
    new_lexeme[lexeme_len] = 0;

    if(out_len){
        *out_len = lexeme_len;
    }

    return new_lexeme;
}

static inline size_t current_lexeme_len(Lexer *lexer){
    return (size_t)(lexer->current - lexer->start);
}

static inline char *current_lexeme(Lexer *lexer, size_t *out_len){
    return copy_source_range(lexer->start, lexer->current, lexer, out_len);
}

static inline void put_current_lexeme(char *buff, Lexer *lexer){
    size_t start = lexer->start;
    size_t current = lexer->current;
    size_t len = current - start;
    RawStr *source = lexer->source;

    memcpy(buff, source->buff + start, len);
    buff[len] = 0;
}

static inline Token *create_token_raw(
	int line,
    TokType type,
    size_t lexeme_len,
    char *lexeme,
    size_t literal_size,
    void *literal,
    Lexer *lexer
){
    Token *token = MEMORY_ALLOC(Token, 1, lexer->rtallocator);

    token->line = line;
    token->type = type;
    token->lexeme_len = lexeme_len;
    token->lexeme = lexeme;
    token->literal_size = literal_size;
    token->literal = literal;
    token->pathname = lexer->pathname;
    token->extra = NULL;

    return token;
}

static inline Token *create_token_literal(
	TokType type,
    size_t literal_size,
    void *literal,
	Lexer *lexer
){
    size_t lexeme_len;
	char *lexeme = current_lexeme(lexer, &lexeme_len);

	return create_token_raw(
        lexer->line,
        type,
        lexeme_len,
        lexeme,
        literal_size,
        literal,
        lexer
    );
}

static inline Token *create_token(TokType type, Lexer *lexer){
	return create_token_literal(type, 0, NULL, lexer);
}

void comment(Lexer *lexer){
	while(!is_at_end(lexer)){
		if(advance(lexer) == '\n'){
			lexer->line++;
			break;
		}
	}
}

Token *decimal(Lexer *lexer){
    TokType type = INT_TYPE_TOKTYPE;

    while (!is_at_end(lexer) && is_dec_digit(peek(lexer))){
        advance(lexer);
    }

	if(match('.', lexer)){
        if(!is_dec_digit(peek(lexer))){
            error(lexer, "Expect digit after decimal point");
            return NULL;
        }

		type = FLOAT_TYPE_TOKTYPE;

		while (!is_at_end(lexer) && is_dec_digit(peek(lexer))){
            advance(lexer);
        }
	}

    size_t lexeme_len;
    char *lexeme = current_lexeme(lexer, &lexeme_len);

    if(type == INT_TYPE_TOKTYPE){
        int64_t *value = MEMORY_ALLOC(int64_t, 1, lexer->rtallocator);
        utils_decimal_str_to_i64(lexeme, value);

		return create_token_raw(
			lexer->line,
            type,
            lexeme_len,
			lexeme,
            sizeof(int64_t),
			value,
			lexer
		);
	}else{
        double *value = MEMORY_ALLOC(double, 1, lexer->rtallocator);
        utils_str_to_double(lexeme, value);

        return create_token_raw(
			lexer->line,
            type,
            lexeme_len,
			lexeme,
            sizeof(double),
			value,
			lexer
		);
	}
}

Token *hexadecimal(Lexer *lexer){
    while(is_hex_digit(peek(lexer))){
        advance(lexer);
    }

    size_t lexeme_len;
    char *lexeme = current_lexeme(lexer, &lexeme_len);

    if(lexeme_len == 2){
        error(lexer, "Expect digit(s) after '%s' prefix", lexeme);
        return NULL;
    }

    if(lexeme_len > 18){
        error(lexer, "Expect at most 18 digits after hexadecimal prefix");
        return NULL;
    }

    int64_t *value = MEMORY_ALLOC(int64_t, 1, lexer->rtallocator);

    utils_hexadecimal_str_to_i64(lexeme, value);

    return create_token_raw(
        lexer->line,
        INT_TYPE_TOKTYPE,
        lexeme_len,
        lexeme,
        sizeof(int64_t),
        value,
        lexer
    );
}

Token *identifier(Lexer *lexer){
    while (!is_at_end(lexer) && is_alpha_numeric(peek(lexer))){
        advance(lexer);
    }

    size_t lexeme_len = current_lexeme_len(lexer);
    char lexeme[lexeme_len + 1];
    TokType *type = NULL;

    put_current_lexeme(lexeme, lexer);
    lzohtable_lookup(lexeme_len, lexeme, lexer->keywords, (void **)(&type));

    if(type){
        return create_token(*type, lexer);
    }

    return create_token(IDENTIFIER_TOKTYPE, lexer);
}

int interpolation(DynArr *tokens, Lexer *lexer){
    while (!is_at_end(lexer) && peek(lexer) != '}'){
        char c = advance(lexer);
        Token *token = NULL;

        switch (c){
            case '?':{
                token = create_token(QUESTION_MARK_TOKTYPE, lexer);
                break;
            }case ':':{
                token = create_token(COLON_TOKTYPE, lexer);
                break;
            }case '[':{
                token = create_token(LEFT_SQUARE_TOKTYPE, lexer);
                break;
            }case ']':{
                token = create_token(RIGHT_SQUARE_TOKTYPE, lexer);
                break;
            }case '(':{
                token = create_token(LEFT_PAREN_TOKTYPE, lexer);
                break;
            }case ')':{
                token = create_token(RIGHT_PAREN_TOKTYPE, lexer);
                break;
            }case '{':{
                token = create_token(LEFT_BRACKET_TOKTYPE, lexer);
                break;
            }case '}':{
                token = create_token(RIGHT_BRACKET_TOKTYPE, lexer);
                break;
            }case '~':{
                token = create_token(NOT_BITWISE_TOKTYPE, lexer);
                break;
            }case '&':{
                token = create_token(AND_BITWISE_TOKTYPE, lexer);
                break;
            }case '^':{
                token = create_token(XOR_BITWISE_TOKTYPE, lexer);
                break;
            }case '|':{
                token = create_token(OR_BITWISE_TOKTYPE, lexer);
                break;
            }case ',':{
                token = create_token(COMMA_TOKTYPE, lexer);
                break;
            }case '.':{
                if(match('.', lexer)){
                    token = create_token(DOUBLE_DOT_TOKTYPE, lexer);
                }else{
                    token = create_token(DOT_TOKTYPE, lexer);
                }

                break;
            }case '+':{
                if(match('=', lexer)){
                    token = create_token(COMPOUND_ADD_TOKTYPE, lexer);
                }else{
                    token = create_token(PLUS_TOKTYPE, lexer);
                }

                break;
            }case '-':{
                if(match('=', lexer)){
                    token = create_token(COMPOUND_SUB_TOKTYPE, lexer);
                }else{
                    token = create_token(MINUS_TOKTYPE, lexer);
                }

                break;
            }case '*':{
                if(match('*', lexer)){
                    token = create_token(DOUBLE_ASTERISK_TOKTYPE, lexer);
                }else if(match('=', lexer)){
                    token = create_token(COMPOUND_MUL_TOKTYPE, lexer);
                }else{
                    token = create_token(ASTERISK_TOKTYPE, lexer);
                }

                break;
            }case '/':{
                if(match('=', lexer)){
                    token = create_token(COMPOUND_DIV_TOKTYPE, lexer);
                }else{
                    token = create_token(SLASH_TOKTYPE, lexer);
                }

                break;
            }case '<':{
                if(match('<', lexer)){
                    token = create_token(LEFT_SHIFT_TOKTYPE, lexer);
                }else if(match('=', lexer)){
                    token = create_token(LESS_EQUALS_TOKTYPE, lexer);
                }else{
                    token = create_token(LESS_TOKTYPE, lexer);
                }

                break;
            }case '>':{
                if(match('>', lexer)){
                    token = create_token(RIGHT_SHIFT_TOKTYPE, lexer);
                }else if(match('=', lexer)){
                    token = create_token(GREATER_EQUALS_TOKTYPE, lexer);
                }else{
                    token = create_token(GREATER_TOKTYPE, lexer);
                }

                break;
            }case '=':{
                if(match('=', lexer)){
                    token = create_token(EQUALS_EQUALS_TOKTYPE, lexer);
                }

                break;
            }case '!':{
                if(match('=', lexer)){
                    token = create_token(NOT_EQUALS_TOKTYPE, lexer);
                }else{
                    token = create_token(EXCLAMATION_TOKTYPE, lexer);
                }

                break;
            }case ' ':{
                lexer->start = lexer->current;
                continue;
            }default:{
                if(is_dec_digit(c) && (match('x', lexer) || match('X', lexer))){
                    token = hexadecimal(lexer);
                }else if(is_dec_digit(c)){
                    token = decimal(lexer);
                }else if(is_alpha_numeric(c)){
                    token = identifier(lexer);
                }else if(c == '"'){
                    token = string(lexer);
                }else{
                    error(lexer, "Unknown token '%c':%d", c, c);
                    return 1;
                }
            }
        }

        if(token){
            dynarr_insert_ptr(token, tokens);
            lexer->start = lexer->current;
            continue;
        }

        error(lexer, "Failed to process string interpolation");

        return 1;
    }

    if(peek(lexer) != '}'){
        error(lexer, "Unclosed string interpolation");
        return 1;
    }

    advance(lexer);

    return 0;
}

Token *string(Lexer *lexer){
    int start = lexer->start;
    size_t from_inner = 0;
    int from_outer = lexer->current;
    DynArr *template_tokens = NULL;
    LZBStr *str_helper = FACTORY_LZBSTR(COMPILE_ALLOCATOR);

    while(!is_at_end(lexer) && peek(lexer) != '"'){
        char c = advance(lexer);

        if(c == '{'){
            if(!template_tokens){
                template_tokens = FACTORY_DYNARR_PTR(COMPILE_ALLOCATOR);
            }

            if(lexer->current - 1 > from_outer){
                size_t lexeme_len;
                char *lexeme = copy_source_range(
                    (size_t)from_outer,
                    lexer->current - 1,
                    lexer,
                    &lexeme_len
                );

                size_t to = LZBSTR_LEN(str_helper);

                size_t buff_len;
                char *buff = lzbstr_rclone_buff_rng(
                    from_inner,
                    to,
                    (LZBStrAllocator *)RUNTIME_ALLOCATOR,
                    str_helper,
                    &buff_len
                );

                Token *token = create_token_raw(
                    lexer->line,
                    STR_TYPE_TOKTYPE,
                    lexeme_len,
                    lexeme,
                    buff_len,
                    buff,
                    lexer
                );

                dynarr_insert_ptr(token, template_tokens);

                from_inner = to;
            }

            lexer->start = lexer->current;

            if(interpolation(template_tokens, lexer)){
                return NULL;
            }

            from_outer = lexer->current;

            continue;
        }

        if(c != '\\'){
            lzbstr_append((char[]){c, 0}, str_helper);
            continue;
        }

        switch (advance(lexer)){
            case 't':{
                lzbstr_append((char[]){'\t', 0}, str_helper);
                break;
            }case 'n':{
                lzbstr_append((char[]){'\n', 0}, str_helper);
                break;
            }case 'r':{
                lzbstr_append((char[]){'\r', 0}, str_helper);
                break;
            }case '"':{
                lzbstr_append((char[]){'"', 0}, str_helper);
                break;
            }case '{':{
                lzbstr_append((char[]){'{', 0}, str_helper);
                break;
            }case '\\':{
                lzbstr_append((char[]){'\\', 0}, str_helper);
                break;
            }default:{
                error(lexer, "Unknown espace sequence: \\%c", peek(lexer));
                return NULL;
            }
        }
    }

    if(peek(lexer) != '"'){
        error(lexer, "Unterminated string");
        return NULL;
    }

    advance(lexer);

    if(template_tokens){
        lexer->start = start;

        if(from_inner < LZBSTR_LEN(str_helper)){
            size_t lexeme_len;
            char *lexeme = copy_source_range(
                (size_t)from_outer,
                lexer->current - 1,
                lexer,
                &lexeme_len
            );

            size_t to = LZBSTR_LEN(str_helper);

            size_t buff_len;
            char *buff = lzbstr_rclone_buff_rng(
                from_inner,
                to,
                (LZBStrAllocator *)RUNTIME_ALLOCATOR,
                str_helper,
                &buff_len
            );

            Token *token = create_token_raw(
                lexer->line,
                STR_TYPE_TOKTYPE,
                lexeme_len,
                lexeme,
                buff_len,
                buff,
                lexer
            );

            dynarr_insert_ptr(token, template_tokens);
        }

        size_t lexeme_len;
        char *lexeme = create_lexeme("EOF", lexer, &lexeme_len);
        Token *token = create_token_raw(-1, EOF_TOKTYPE, lexeme_len, lexeme, 0, NULL, lexer);

        dynarr_insert_ptr(token, template_tokens);

        return create_token_literal(TEMPLATE_TYPE_TOKTYPE, sizeof(DynArr), template_tokens, lexer);
    }

    size_t literal_size;
    char *literal;

    if(LZBSTR_LEN(str_helper) == 0){
        literal_size = 0;
        literal = MEMORY_ALLOC(char, 1, RUNTIME_ALLOCATOR);
        literal[0] = 0;
    }else{
        literal = lzbstr_rclone_buff((LZBStrAllocator *)RUNTIME_ALLOCATOR, str_helper, &literal_size);
    }

    return create_token_literal(STR_TYPE_TOKTYPE, literal_size, literal, lexer);
}

Token *scan_token(char c, Lexer *lexer){
    switch (c){
        case '?':{
        	return create_token(QUESTION_MARK_TOKTYPE, lexer);
        }case ':':{
			return create_token(COLON_TOKTYPE, lexer);
		}case ';':{
            return create_token(SEMICOLON_TOKTYPE, lexer);
        }case '[':{
            return create_token(LEFT_SQUARE_TOKTYPE, lexer);
        }case ']':{
            return create_token(RIGHT_SQUARE_TOKTYPE, lexer);
        }case '(':{
            return create_token(LEFT_PAREN_TOKTYPE, lexer);
        }case ')':{
            return create_token(RIGHT_PAREN_TOKTYPE, lexer);
        }case '{':{
            return create_token(LEFT_BRACKET_TOKTYPE, lexer);
        }case '}':{
            return create_token(RIGHT_BRACKET_TOKTYPE, lexer);
        }case '~':{
            return create_token(NOT_BITWISE_TOKTYPE, lexer);
        }case '&':{
            return create_token(AND_BITWISE_TOKTYPE, lexer);
        }case '^':{
            return create_token(XOR_BITWISE_TOKTYPE, lexer);
        }case '|':{
            return create_token(OR_BITWISE_TOKTYPE, lexer);
        }case ',':{
			return create_token(COMMA_TOKTYPE, lexer);
		}case '.':{
            if(match('.', lexer)){
                return create_token(DOUBLE_DOT_TOKTYPE, lexer);
            }else{
                return create_token(DOT_TOKTYPE, lexer);
            }
		}case '+':{
			if(match('=', lexer)){
				return create_token(COMPOUND_ADD_TOKTYPE, lexer);
			}else{
				return create_token(PLUS_TOKTYPE, lexer);
			}
        }case '-':{
			if(match('=', lexer)){
				return create_token(COMPOUND_SUB_TOKTYPE, lexer);
			}else{
				return create_token(MINUS_TOKTYPE, lexer);
			}
        }case '*':{
			if(match('*', lexer)){
                return create_token(DOUBLE_ASTERISK_TOKTYPE, lexer);
            }else if(match('=', lexer)){
				return create_token(COMPOUND_MUL_TOKTYPE, lexer);
			}else{
				return create_token(ASTERISK_TOKTYPE, lexer);
			}
        }case '/':{
			if(match('/', lexer)){
				comment(lexer);
				return NULL;
			}else if(match('=', lexer)){
				return create_token(COMPOUND_DIV_TOKTYPE, lexer);
			}else{
				return create_token(SLASH_TOKTYPE, lexer);
			}
        }case '<':{
            if(match('<', lexer)){
                return create_token(LEFT_SHIFT_TOKTYPE, lexer);
            }else if(match('=', lexer)){
				return create_token(LESS_EQUALS_TOKTYPE, lexer);
			}else{
				return create_token(LESS_TOKTYPE, lexer);
			}
        }case '>':{
            if(match('>', lexer)){
                return create_token(RIGHT_SHIFT_TOKTYPE, lexer);
            }else if(match('=', lexer)){
				return create_token(GREATER_EQUALS_TOKTYPE, lexer);
			}else{
				return create_token(GREATER_TOKTYPE, lexer);
			}
        }case '=':{
            if(match('=', lexer)){
				return create_token(EQUALS_EQUALS_TOKTYPE, lexer);
			}else{
				return create_token(EQUALS_TOKTYPE, lexer);
			}
        }case '!':{
            if(match('=', lexer)){
				return create_token(NOT_EQUALS_TOKTYPE, lexer);
			}else{
				return create_token(EXCLAMATION_TOKTYPE, lexer);
			}
        }case '\n':{
            lexer->line++;
            return NULL;
        }case ' ':
         case '\r':
         case '\t':
         case '\0':{
            return NULL;
        }default:{
            if(is_dec_digit(c) && (match('x', lexer) || match('X', lexer))){
                return hexadecimal(lexer);
            }else if(is_dec_digit(c)){
				return decimal(lexer);
			}else if(is_alpha_numeric(c)){
				return identifier(lexer);
			}else if(c == '"'){
				return string(lexer);
			}else{
				error(lexer, "Unknown token '%c':%d", c, c);
				return NULL;
			}
        }
    }

    return NULL;
}
//--------------------------------------------------------------------------//
//                          PUBLIC IMPLEMENTATION                           //
//--------------------------------------------------------------------------//
Lexer *lexer_create(Allocator *ctallocator, Allocator *rtallocator){
    Lexer *lexer = (Lexer *)MEMORY_ALLOC(Lexer, 1, ctallocator);

    if(!lexer){
        return NULL;
    }

    memset(lexer, 0, sizeof(Lexer));
    lexer->ctallocator = ctallocator;
    lexer->rtallocator = rtallocator;

    return lexer;
}

int lexer_scan(
	RawStr *source,
	DynArr *tokens,
	LZOHTable *keywords,
    char *pathname,
	Lexer *lexer
){
    if(setjmp(lexer->err_buf) == 0){
        lexer->line = 1;
        lexer->start = 0;
        lexer->current = 0;
        lexer->source = source;
        lexer->tokens = tokens;
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

        size_t lexeme_len;
        char *lexeme = create_lexeme("EOF", lexer, &lexeme_len);
        ADD_TOKEN_RAW(create_token_raw(-1, EOF_TOKTYPE, lexeme_len, lexeme, 0, NULL, lexer));

        return lexer->status;
    }else{
        return 1;
    }
}