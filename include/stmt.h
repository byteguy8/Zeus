#ifndef STMT_H
#define STMT_H

#include "token.h"
#include "expr.h"
#include "dynarr.h"

typedef enum stmt_type{
    EXPR_STMTTYPE,
    PRINT_STMTTYPE,
    BLOCK_STMTTYPE,
    IF_STMTTYPE,
    STOP_STMTTYPE,
    CONTINUE_STMTTYPE,
    WHILE_STMTTYPE,
    THROW_STMTTYPE,
    TRY_STMTTYPE,
    RETURN_STMTTYPE,

    VAR_DECL_STMTTYPE,
    FUNCTION_STMTTYPE,
    IMPORT_STMTTYPE,
    LOAD_STMTTYPE,
    EXPORT_STMTTYPE,
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

typedef struct block_stmt{
    DynArrPtr *stmts;
}BlockStmt;

typedef struct if_stmt{
    Token *if_token;
	Expr *if_condition;
	DynArrPtr *if_stmts;
	DynArrPtr *else_stmts;
}IfStmt;

typedef struct stop_stmt{
	Token *stop_token;
}StopStmt;

typedef struct continue_stmt{
    Token *continue_token;
}ContinueStmt;

typedef struct while_stmt{
    Token *while_token;
	Expr *condition;
	DynArrPtr *stmts;
}WhileStmt;

typedef struct throw_stmt{
    Token *throw_token;
	Expr *value;
}ThrowStmt;

typedef struct try_stmt{
    Token *try_token;
    DynArrPtr *try_stmts;
	Token *catch_token;
	Token *err_identifier;
	DynArrPtr *catch_stmts;
}TryStmt;

typedef struct return_stmt{
    Token *return_token;
    Expr *value;
}ReturnStmt;

typedef struct var_decl_stmt{
    char is_const;
    char is_initialized;
    Token *identifier_token;
    Expr *initializer_expr;
}VarDeclStmt;

typedef struct function_stmt{
    Token *identifier_token;
    DynArrPtr *params;
    DynArrPtr *stmts;
}FunctionStmt;

typedef struct import_stmt{
    Token *import_token;
    DynArr *names;
    Token *alt_name;
}ImportStmt;

typedef struct load_stmt{
    Token *load_token;
    Token *pathname;
    Token *name;
}LoadStmt;

typedef struct export_stmt{
    Token *export_token;
    DynArrPtr *symbols;
}ExportStmt;

#endif
