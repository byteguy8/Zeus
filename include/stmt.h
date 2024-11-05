#ifndef STMT_H
#define STMT_H

#include "token.h"
#include "expr.h"

typedef enum stmt_type{
    EXPR_STMTTYPE,
    PRINT_STMTTYPE,
    VAR_DECL_STMTTYPE
}StmtType;

typedef struct stmt{
    StmtType type;
    void *sub_stmt;
}Stmt;

typedef struct expr_stmt{
    Expr *expr;
}ExprStmt;

typedef struct print_stmt{
    Expr *expr;
    Token *print_token;
}PrintStmt;

typedef struct var_decl_stmt
{
    Token *identifier_token;
    Expr *initializer_expr;
}VarDeclStmt;

#endif