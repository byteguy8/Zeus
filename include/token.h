#ifndef TOKEN_H
#define TOKEN_H

typedef enum token_type{
    EXCLAMATION_TOKTYPE,

    PLUS_TOKTYPE, MINUS_TOKTYPE,
	SLASH_TOKTYPE, ASTERISK_TOKTYPE,
	COMMA_TOKTYPE,

    LESS_TOKTYPE, GREATER_TOKTYPE,
    LESS_EQUALS_TOKTYPE, GREATER_EQUALS_TOKTYPE,
    EQUALS_EQUALS_TOKTYPE, NOT_EQUALS_TOKTYPE,

    OR_TOKTYPE, AND_TOKTYPE,
	
    LEFT_PAREN_TOKTYPE, RIGHT_PAREN_TOKTYPE,
    LEFT_BRACKET_TOKTYPE, RIGHT_BRACKET_TOKTYPE,

    EQUALS_TOKTYPE, COMPOUND_ADD_TOKTYPE,
	COMPOUND_SUB_TOKTYPE, COMPOUND_MUL_TOKTYPE,
	COMPOUND_DIV_TOKTYPE, SEMICOLON_TOKTYPE,
    
    // keywords
	MOD_TOKTYPE, EMPTY_TOKTYPE,
    FALSE_TOKTYPE, TRUE_TOKTYPE,
    PRINT_TOKTYPE, MUT_TOKTYPE,
    IMUT_TOKTYPE, IF_TOKTYPE,
    ELSE_TOKTYPE, WHILE_TOKTYPE,
	STOP_TOKTYPE, LIST_TOKTYPE,
    PROC_TOKTYPE, RET_TOKTYPE,
    
    //types
    INT_TOKTYPE, STRING_TOKTYPE,

    IDENTIFIER_TOKTYPE,
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
