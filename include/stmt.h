#ifndef STMT_H
#define STMT_H

#include "token.h"
#include "expr.h"
#include "dynarr.h"

typedef enum stmt_type{
    EXPR_STMTTYPE,
    BLOCK_STMTTYPE,
    IF_STMTTYPE,
    STOP_STMTTYPE,
    CONTINUE_STMTTYPE,
    WHILE_STMTTYPE,
    FOR_RANGE_STMTTYPE,
    THROW_STMTTYPE,
    TRY_STMTTYPE,
    RETURN_STMTTYPE,

    VAR_DECL_STMTTYPE,
    FUNCTION_STMTTYPE,
    IMPORT_STMTTYPE,
    EXPORT_STMTTYPE,
}StmtType;

typedef struct stmt{
    StmtType type;
    void *sub_stmt;
}Stmt;

typedef struct expr_stmt{
    Expr *expr;
}ExprStmt;

typedef struct block_stmt{
    DynArr *stmts;
}BlockStmt;

typedef struct if_stmt_branch{
    Token *branch_token;
    Expr *condition_expr;
    DynArr *stmts;
}IfStmtBranch;

typedef struct if_stmt{
    IfStmtBranch *if_branch;
    DynArr *elif_branches;
	DynArr *else_stmts;
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
	DynArr *stmts;
}WhileStmt;

typedef struct for_range_stmt{
    Token *for_token;
    Token *symbol_token;
    Expr *left_expr;
    Token *for_type_token;
    Expr *right_expr;
    DynArr *stmts;
}ForRangeStmt;

typedef struct throw_stmt{
    Token *throw_token;
	Expr *value;
}ThrowStmt;

typedef struct try_stmt{
    Token *try_token;
    DynArr *try_stmts;
	Token *catch_token;
	Token *err_identifier;
	DynArr *catch_stmts;
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
    DynArr *params;
    DynArr *stmts;
}FunctionStmt;

typedef struct import_stmt{
    Token *import_token;
    DynArr *names;
    Token *alt_name;
}ImportStmt;

typedef struct export_stmt{
    Token *export_token;
    DynArr *symbols;
}ExportStmt;

#endif
