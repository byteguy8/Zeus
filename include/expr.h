#ifndef EXPR_H
#define EXPR_H

#include "token.h"
#include <stdint.h>

typedef enum expr_type{
    EMPTY_EXPRTYPE,
	BOOL_EXPRTYPE,
    INT_EXPRTYPE,
    FLOAT_EXPRTYPE,
	STRING_EXPRTYPE,
    TEMPLATE_EXPRTYPE,
    ANON_EXPRTYPE,
    GROUP_EXPRTYPE,
    IDENTIFIER_EXPRTYPE,
    CALL_EXPRTYPE,
	ACCESS_EXPRTYPE,
    INDEX_EXPRTYPE,
    UNARY_EXPRTYPE,
	BINARY_EXPRTYPE,
    CONCAT_EXPRTYPE,
    MULSTR_EXPRTYPE,
    BITWISE_EXPRTYPE,
    COMPARISON_EXPRTYPE,
    LOGICAL_EXPRTYPE,
    ASSIGN_EXPRTYPE,
	COMPOUND_EXPRTYPE,
    ARRAY_EXPRTYPE,
	LIST_EXPRTYPE,
    DICT_EXPRTYPE,
	RECORD_EXPRTYPE,
	IS_EXPRTYPE,
	TENARY_EXPRTYPE
}ExprType;

typedef struct expr{
	ExprType type;
	void *sub_expr;
}Expr;

typedef struct empty_expr{
    Token *empty_token;
}EmptyExpr;

typedef struct bool_expr{
    uint8_t value;
    Token *bool_token;
}BoolExpr;

typedef struct int_expr{
	Token *token;
}IntExpr;

typedef struct float_expr{
	Token *token;
}FloatExpr;

typedef struct string_expr{
	Token *str_token;
}StrExpr;

typedef struct template_expr{
    Token *template_token;
    DynArr *exprs; //expressions statements
}TemplateExpr;

typedef struct anon_expr{
    Token *anon_token;
    DynArr *params;
    DynArr *stmts;
}AnonExpr;

typedef struct group_expr{
    Token *left_paren_token;
    Expr *expr;
}GroupExpr;

typedef struct identifier_expr{
    Token *identifier_token;
}IdentifierExpr;

typedef struct call_expr{
    Expr *left;
    Token *left_paren;
    DynArr *args;
}CallExpr;

typedef struct access_expr{
	Expr *left;
	Token *dot_token;
	Token *symbol_token;
}AccessExpr;

typedef struct index_expr{
    Expr *target;
    Token *left_square_token;
    Expr *index_expr;
}IndexExpr;

typedef struct unary_expr{
    Token *operator;
    Expr *right;
}UnaryExpr;

typedef struct binary_expr{
	Expr *left;
	Token *operator;
	Expr *right;
}BinaryExpr;

typedef struct concat_expr{
    Expr *left;
	Token *operator;
	Expr *right;
}ConcatExpr;

typedef struct mul_str_expr{
    Expr *left;
	Token *operator;
	Expr *right;
}MulStrExpr;

typedef struct bitwise_expr{
    Expr *left;
	Token *operator;
	Expr *right;
}BitWiseExpr;

typedef struct comparison_expr{
    Expr *left;
	Token *operator;
	Expr *right;
}ComparisonExpr;

typedef struct logical_expr{
	Expr *left;
	Token *operator;
	Expr *right;
}LogicalExpr;

typedef struct assign_expr{
	Expr *left;
	Token *equals_token;
    Expr *value_expr;
}AssignExpr;

typedef struct compound_expr{
	Expr *left;
	Token *operator;
	Expr *right;
}CompoundExpr;

typedef struct array_expr{
    Token *array_token;
    Expr *len_expr;
    DynArr *values;
}ArrayExpr;

typedef struct list_expr{
	Token *list_token;
	DynArr *exprs;
}ListExpr;

typedef struct dict_key_value{
    Expr *key;
    Expr *value;
}DictKeyValue;

typedef struct dict_expr{
    Token *dict_token;
    DynArr *key_values;
}DictExpr;

typedef struct record_expr_value{
	Token *key;
	Expr *value;
}RecordExprValue;

typedef struct record_expr{
	Token *record_token;
	DynArr *key_values;
}RecordExpr;

typedef struct is_expr{
	Expr *left_expr;
	Token *is_token;
	Token *type_token;
}IsExpr;

typedef struct tenary_expr{
    Expr *condition;
    Expr *left;
    Token *mark_token;
    Expr *right;
}TenaryExpr;

#endif
