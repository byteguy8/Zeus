#ifndef EXPR_H
#define XPR_H

#include "token.h"
#include <stdint.h>

typedef enum expr_type{
    EMPTY_EXPRTYPE,
	BOOL_EXPRTYPE,
    INT_EXPRTYPE,
	STRING_EXPRTYPE,
    GROUP_EXPRTYPE,
    IDENTIFIER_EXPRTYPE,
    CALL_EXPRTYPE,
	ACCESS_EXPRTYPE,
    UNARY_EXPRTYPE,
	BINARY_EXPRTYPE,
    COMPARISON_EXPRTYPE,
    LOGICAL_EXPRTYPE,
    ASSIGN_EXPRTYPE,
	COMPOUND_EXPRTYPE,
	LIST_EXPRTYPE,
    DICT_EXPRTYPE,
	RECORD_EXPRTYPE,
	IS_EXPRTYPE,
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

typedef struct string_expr{
	uint32_t hash;
	Token *string_token;
}StringExpr;

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
    DynArrPtr *args;    
}CallExpr;

typedef struct access_expr{
	Expr *left;
	Token *dot_token;
	Token *symbol_token;
}AccessExpr;

typedef struct unary_expr{
    Token *operator;
    Expr *right;
}UnaryExpr;

typedef struct binary_expr{
	Expr *left;
	Token *operator;
	Expr *right;
}BinaryExpr;

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
	Token *identifier_token;
	Token *operator;
	Expr *right;
}CompoundExpr;

typedef struct list_expr{
	Token *list_token;
	DynArrPtr *exprs;
}ListExpr;

typedef struct dict_key_value{
    Expr *key;
    Expr *value;
}DictKeyValue;

typedef struct dict_expr{
    Token *dict_token;
    DynArrPtr *key_values;
}DictExpr;

typedef struct record_expr_value{
	Token *key;
	Expr *value;
}RecordExprValue;

typedef struct record_expr{
	Token *record_token;
	DynArrPtr *key_values;
}RecordExpr;

typedef struct is_expr{
	Expr *left_expr;
	Token *is_token;
	Token *type_token;
}IsExpr;

#endif
