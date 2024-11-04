#ifndef EXPR_H
#define XPR_H

#include "token.h"

typedef enum expr_type{
	BOOL_EXPRTYPE,
    INT_EXPRTYPE,
    GROUP_EXPRTYPE,
	BINARY_EXPRTYPE
}ExprType;

typedef struct expr{
	ExprType type;
	void *sub_expr;
}Expr;

typedef struct bool_expr
{
    uint8_t value;
    Token *bool_token;
}BoolExpr;

typedef struct int_expr{
	Token *token;
}IntExpr;

typedef struct group_expr
{
    Token *left_paren_token;
    Expr *expr;
}GroupExpr;

typedef struct binary_expr{
	Expr *left;
	Token *operator;
	Expr *right;
}BinaryExpr;

#endif
