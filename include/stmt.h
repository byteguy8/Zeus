#ifndef STMT_H
#define STMT_H

#include "token.h"
#include "expr.h"
#include "dynarr.h"

typedef enum stmt_type{
    EXPR_STMTTYPE,
    PRINT_STMTTYPE,
    VAR_DECL_STMTTYPE,
    BLOCK_STMTTYPE,
	IF_STMTTYPE,
	WHILE_STMTTYPE,
	STOP_STMTTYPE,
    CONTINUE_STMTTYPE,
    FUNCTION_STMTTYPE,
    RETURN_STMTTYPE,
    IMPORT_STMTTYPE
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

typedef struct var_decl_stmt{
    char is_const;
    char is_initialized;
    Token *identifier_token;
    Expr *initializer_expr;
}VarDeclStmt;

typedef struct block_stmt{
    DynArrPtr *stmts;
}BlockStmt;

typedef struct if_stmt{
	Expr *if_condition;
	DynArrPtr *if_stmts;
	DynArrPtr *else_stmts;
}IfStmt;

typedef struct while_stmt{
	Expr *condition;
	DynArrPtr *stmts;
}WhileStmt;

typedef struct stop_stmt{
	Token *stop_token;
}StopStmt;

typedef struct continue_stmt{
    Token *continue_token;
}ContinueStmt;

typedef struct function_stmt{
    Token *name_token;
    DynArrPtr *params;
    DynArrPtr *stmts;
}FunctionStmt;

typedef struct return_stmt{
    Token *return_token;
    Expr *value;
}ReturnStmt;

typedef struct import_stmt{
    Token *import_token;
    Token *path_token;
}ImportStmt;

#endif
