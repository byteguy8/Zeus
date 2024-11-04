#ifndef TOKEN_H
#define TOKEN_H

typedef enum token_type{
    PLUS_TOKTYPE, MINUS_TOKTYPE,
	SLASH_TOKTYPE, ASTERISK_TOKTYPE,
	
    LEFT_PAREN_TOKTYPE, RIGHT_PAREN_TOKTYPE,

    SEMICOLON_TOKTYPE,    
    
    // keywords
    FALSE_TOKTYPE,
    TRUE_TOKTYPE,
    PRINT_TOKTYPE,

    //types
    INT_TOKTYPE,

    EOF_TOKTYPE
}TokenType;

typedef struct token{
    int line;
	char *lexeme;
	void *literal;
	size_t literal_size;
	TokenType type;
}Token;

#endif
