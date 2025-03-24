#ifndef TOKEN_H
#define TOKEN_H

typedef enum token_type{
    EXCLAMATION_TOKTYPE,
    QUESTION_MARK_TOKTYPE,

    PLUS_TOKTYPE, MINUS_TOKTYPE,
	SLASH_TOKTYPE, ASTERISK_TOKTYPE,
	COMMA_TOKTYPE, DOT_TOKTYPE,

    LESS_TOKTYPE, GREATER_TOKTYPE,
    LESS_EQUALS_TOKTYPE, GREATER_EQUALS_TOKTYPE,
    EQUALS_EQUALS_TOKTYPE, NOT_EQUALS_TOKTYPE,

    OR_TOKTYPE, AND_TOKTYPE,

    LEFT_SQUARE_TOKTYPE, RIGHT_SQUARE_TOKTYPE,
    LEFT_PAREN_TOKTYPE, RIGHT_PAREN_TOKTYPE,
    LEFT_BRACKET_TOKTYPE, RIGHT_BRACKET_TOKTYPE,

    EQUALS_TOKTYPE, COMPOUND_ADD_TOKTYPE,
	COMPOUND_SUB_TOKTYPE, COMPOUND_MUL_TOKTYPE,
	COMPOUND_DIV_TOKTYPE, COLON_TOKTYPE,
	SEMICOLON_TOKTYPE,

    NOT_BITWISE_TOKTYPE, LEFT_SHIFT_TOKTYPE,
    RIGHT_SHIFT_TOKTYPE, AND_BITWISE_TOKTYPE,
    XOR_BITWISE_TOKTYPE, OR_BITWISE_TOKTYPE,

    // keywords
	MOD_TOKTYPE, EMPTY_TOKTYPE,
    FALSE_TOKTYPE, TRUE_TOKTYPE,
    MUT_TOKTYPE, IMUT_TOKTYPE,
    IF_TOKTYPE, ELSE_TOKTYPE,
    WHILE_TOKTYPE, STOP_TOKTYPE,
    CONTINUE_TOKTYPE, ARRAY_TOKTYPE,
    LIST_TOKTYPE, TO_TOKTYPE,
    DICT_TOKTYPE, RECORD_TOKTYPE,
    PROC_TOKTYPE, ANON_TOKTYPE,
    RET_TOKTYPE, IMPORT_TOKTYPE,
    AS_TOKTYPE, BOOL_TOKTYPE,
    FLOAT_TOKTYPE, INT_TOKTYPE,
    STR_TOKTYPE, IS_TOKTYPE,
    TRY_TOKTYPE, CATCH_TOKTYPE,
    THROW_TOKTYPE, LOAD_TOKTYPE,
    EXPORT_TOKTYPE,

    //types
    INT_TYPE_TOKTYPE, FLOAT_TYPE_TOKTYPE,
    STR_TYPE_TOKTYPE, TEMPLATE_TYPE_TOKTYPE,

    IDENTIFIER_TOKTYPE,
    EOF_TOKTYPE
}TokenType;

typedef struct token{
    int line;
	char *lexeme;
	void *literal;
	size_t literal_size;
	TokenType type;
    char *pathname;
    void *extra;
}Token;

typedef enum template_item_type{
    STR_TITEMTYPE,
    TOKENS_TITEMTYPE,
}TemplateItemType;

typedef struct template_item{
    TemplateItemType type;
    union{
        Token *str;
        void *tokens; //can be NULL
    }value;

}TemplateItem;

#endif
