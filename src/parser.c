#include "parser.h"
#include "memory.h"
#include "token.h"
#include "stmt.h"
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <setjmp.h>

static jmp_buf err_jmp;

static void error(Parser *parser, Token *token, char *msg, ...){
    va_list args;
	va_start(args, msg);

	fprintf(stderr, "Parser error at line %d:\n\t", token->line);
	vfprintf(stderr, msg, args);
    fprintf(stderr, "\n");

	va_end(args);

    longjmp(err_jmp, 1);
}

Expr *create_expr(ExprType type, void *sub_expr){
    Expr *expr = (Expr *)memory_alloc(sizeof(Expr));
    expr->type = type;
    expr->sub_expr = sub_expr;

    return expr;
}

Stmt *create_stmt(StmtType type, void *sub_stmt){
    Stmt *stmt = (Stmt *)memory_alloc(sizeof(Stmt));
    stmt->type = type;
    stmt->sub_stmt = sub_stmt;
    
    return stmt;
}

static Token *peek(Parser *parser){
	DynArrPtr *tokens = parser->tokens;
	return (Token *)DYNARR_PTR_GET(parser->current, tokens);
}

static Token *previous(Parser *parser){
	DynArrPtr *tokens = parser->tokens;
	return (Token *)DYNARR_PTR_GET(parser->current - 1, tokens);
}

static int is_at_end(Parser *parser){
    Token *token = peek(parser);
	return token->type == EOF_TOKTYPE;
}

static int match(Parser *parser, int count, ...){
	Token *token = peek(parser);

	va_list args;
	va_start(args, count);

	for(int i = 0; i < count; i++){
		TokenType type = va_arg(args, TokenType);

		if(token->type == type){
			parser->current++;
			va_end(args);
			return 1;
		}
	}

	va_end(args);
	return 0;
}

static int check(Parser *parser, TokenType type){
    Token *token = peek(parser);
    return token->type == type;
}

Token *consume(Parser *parser, TokenType type, char *err_msg, ...){
    Token *token = peek(parser);
    
    if(token->type == type){
        parser->current++;
        return token;
    }

    va_list args;
	va_start(args, err_msg);

	fprintf(stderr, "Parser error at line %d:\n\t", token->line);
	vfprintf(stderr, err_msg, args);
    fprintf(stderr, "\n");

	va_end(args);

    longjmp(err_jmp, 1);
}

Expr *parse_expr(Parser *paser);
Expr *parse_assign(Parser *parser);
Expr *parse_or(Parser *parser);
Expr *parse_and(Parser *parser);
Expr *parse_comparison(Parser *parser);
Expr *parse_term(Parser *parser);
Expr *parse_factor(Parser *parser);
Expr *parse_unary(Parser *parser);
Expr *parse_literal(Parser *parser);

Stmt *parse_stmt(Parser *parser);
Stmt *parse_expr_stmt(Parser *parser);
Stmt *parse_print_stmt(Parser *parser);
Stmt *parse_var_decl_stmt(Parser *parser);
DynArrPtr *parse_block_stmt(Parser *parser);
Stmt *parse_if_stmt(Parser *parser);
Stmt *parse_while_stmt(Parser *parser);

Expr *parse_expr(Parser *parser){
	return parse_assign(parser);
}

Expr *parse_assign(Parser *parser){
    Expr *expr = parse_or(parser);
	
	if(match(parser, 4, 
			 COMPOUND_ADD_TOKTYPE, 
			 COMPOUND_SUB_TOKTYPE, 
			 COMPOUND_MUL_TOKTYPE, 
			 COMPOUND_DIV_TOKTYPE)){
		if(expr->type != IDENTIFIER_EXPRTYPE)
            error(
                parser, 
                peek(parser), 
                "Expect identifier in left side of assignment expression.");

		IdentifierExpr *identifier_expr = (IdentifierExpr *)expr->sub_expr;
		Token *identifier_token = identifier_expr->identifier_token;
		Token *operator = previous(parser);
		Expr *right = parse_assign(parser);

		CompoundExpr *compound_expr = (CompoundExpr *)memory_alloc(sizeof(CompoundExpr));
		compound_expr->identifier_token = identifier_token;
		compound_expr->operator = operator;
		compound_expr->right = right;

		return create_expr(COMPOUND_EXPRTYPE, compound_expr);
	}

    if(match(parser, 1, EQUALS_TOKTYPE)){
        if(expr->type != IDENTIFIER_EXPRTYPE)
            error(
                parser, 
                peek(parser), 
                "Expect identifier in left side of assignment expression.");

        IdentifierExpr *idenfifier_expr = (IdentifierExpr *)expr->sub_expr;
        Token *identifier_token = idenfifier_expr->identifier_token;
        Expr *value_expr = parse_assign(parser);

        AssignExpr *assign_expr = (AssignExpr *)memory_alloc(sizeof(AssignExpr));
        assign_expr->identifier_token = identifier_token;
        assign_expr->value_expr = value_expr;

        return create_expr(ASSIGN_EXPRTYPE, assign_expr);
    }

    return expr;
}

Expr *parse_or(Parser *parser){
    Expr *left = parse_and(parser);

	while(match(parser, 1, OR_TOKTYPE)){
		Token *operator = previous(parser);
		Expr *right = parse_and(parser);

		LogicalExpr *logical_expr = (LogicalExpr *)memory_alloc(sizeof(LogicalExpr));
		logical_expr->left = left;
		logical_expr->operator = operator;
		logical_expr->right = right;

		left = create_expr(LOGICAL_EXPRTYPE, logical_expr);
	}

    return left;
}

Expr *parse_and(Parser *parser){
    Expr *left = parse_comparison(parser);

	while(match(parser, 1, AND_TOKTYPE)){
		Token *operator = previous(parser);
		Expr *right = parse_comparison(parser);

		LogicalExpr *logical_expr = (LogicalExpr *)memory_alloc(sizeof(LogicalExpr));
		logical_expr->left = left;
		logical_expr->operator = operator;
		logical_expr->right = right;

		left = create_expr(LOGICAL_EXPRTYPE, logical_expr);
	}

    return left;
}

Expr *parse_comparison(Parser *parser){
    Expr *left = parse_term(parser);

	while(match(parser, 6, 
        LESS_TOKTYPE, 
        GREATER_TOKTYPE,
        LESS_EQUALS_TOKTYPE,
        GREATER_EQUALS_TOKTYPE,
        EQUALS_EQUALS_TOKTYPE,
        NOT_EQUALS_TOKTYPE)){
		Token *operator = previous(parser);
		Expr *right = parse_term(parser);

		ComparisonExpr *comparison_expr = (ComparisonExpr *)memory_alloc(sizeof(ComparisonExpr));
		comparison_expr->left = left;
		comparison_expr->operator = operator;
		comparison_expr->right = right;

		left = create_expr(COMPARISON_EXPRTYPE, comparison_expr);
	}

    return left;
}

Expr *parse_term(Parser *parser){
	Expr *left = parse_factor(parser);

	while(match(parser, 2, PLUS_TOKTYPE, MINUS_TOKTYPE)){
		Token *operator = previous(parser);
		Expr *right = parse_factor(parser);

		BinaryExpr *binary_expr = (BinaryExpr *)memory_alloc(sizeof(BinaryExpr));
		binary_expr->left = left;
		binary_expr->operator = operator;
		binary_expr->right = right;

		left = create_expr(BINARY_EXPRTYPE, binary_expr);
	}

	return left;
}

Expr *parse_factor(Parser *parser){
	Expr *left = parse_unary(parser);

	while(match(parser, 2, ASTERISK_TOKTYPE, SLASH_TOKTYPE)){
		Token *operator = previous(parser);
		Expr *right = parse_unary(parser);

		BinaryExpr *binary_expr = (BinaryExpr *)memory_alloc(sizeof(BinaryExpr));
		binary_expr->left = left;
		binary_expr->operator = operator;
		binary_expr->right = right;

		left = create_expr(BINARY_EXPRTYPE, binary_expr);
	}

	return left;
}

Expr *parse_unary(Parser *parser){
    if(match(parser, 2, MINUS_TOKTYPE, EXCLAMATION_TOKTYPE)){
        Token *operator = previous(parser);
        Expr *right = parse_unary(parser);

        UnaryExpr *unary_expr = (UnaryExpr *)malloc(sizeof(UnaryExpr));
        unary_expr->operator = operator;
        unary_expr->right = right;

        return create_expr(UNARY_EXPRTYPE, unary_expr);
    }

    return parse_literal(parser);
}

Expr *parse_literal(Parser *parser){
	if(match(parser, 1, INT_TOKTYPE)){
		IntExpr *int_expr = (IntExpr *)memory_alloc(sizeof(IntExpr));
		int_expr->token = previous(parser);

		Expr *expr = (Expr *)memory_alloc(sizeof(Expr));
		expr->type = INT_EXPRTYPE;
		expr->sub_expr = int_expr;

		return expr;
	}

    if(match(parser, 1, FALSE_TOKTYPE)){
        BoolExpr *bool_expr = (BoolExpr *)memory_alloc(sizeof(BoolExpr));
        bool_expr->value = 0;
        bool_expr->bool_token = previous(parser);

        Expr *expr = (Expr *)memory_alloc(sizeof(Expr));
		expr->type = BOOL_EXPRTYPE;
		expr->sub_expr = bool_expr;

		return expr;
    }

    if(match(parser, 1, TRUE_TOKTYPE)){
        BoolExpr *bool_expr = (BoolExpr *)memory_alloc(sizeof(BoolExpr));
        bool_expr->value = 1;
        bool_expr->bool_token = previous(parser);

        Expr *expr = (Expr *)memory_alloc(sizeof(Expr));
		expr->type = BOOL_EXPRTYPE;
		expr->sub_expr = bool_expr;

		return expr;
    }

    if(match(parser, 1, LEFT_PAREN_TOKTYPE)){
        Token *left_paren_token = previous(parser);
        Expr *group_sub_expr = parse_expr(parser);

        GroupExpr *group_expr = (GroupExpr *)memory_alloc(sizeof(GroupExpr));
        group_expr->left_paren_token = left_paren_token;
        group_expr->expr = group_sub_expr;
        
        consume(parser, RIGHT_PAREN_TOKTYPE, "Expect ')' after expression in group expression");

        Expr *expr = (Expr *)memory_alloc(sizeof(Expr));
		expr->type = GROUP_EXPRTYPE;
		expr->sub_expr = group_expr;

		return expr;
    }

    if(match(parser, 1, IDENTIFIER_TOKTYPE)){
        Token *identifier_token = previous(parser);
        
        IdentifierExpr *identifier_expr = (IdentifierExpr *)memory_alloc(sizeof(IdentifierExpr));
        identifier_expr->identifier_token= identifier_token;

        Expr *expr = (Expr *)memory_alloc(sizeof(Expr));
		expr->type = IDENTIFIER_EXPRTYPE;
		expr->sub_expr = identifier_expr;

		return expr;
    }

	Token *token = peek(parser);
    error(parser, token, "Expect something: 'false', 'true' or integer, but got '%s'", token->lexeme);
    return NULL;
}

Stmt *parse_stmt(Parser *parser){
    if(match(parser, 1, PRINT_TOKTYPE))
        return parse_print_stmt(parser);

    if(match(parser, 2, MUT_TOKTYPE, IMUT_TOKTYPE))
        return parse_var_decl_stmt(parser);

    if(match(parser, 1, LEFT_BRACKET_TOKTYPE)){
        DynArrPtr *stmts = parse_block_stmt(parser);
        
        BlockStmt *block_stmt = (BlockStmt *)memory_alloc(sizeof(BlockStmt));
        block_stmt->stmts = stmts;

        return create_stmt(BLOCK_STMTTYPE, block_stmt);
    }

	if(match(parser, 1, IF_TOKTYPE))
		return parse_if_stmt(parser);

	if(match(parser, 1, WHILE_TOKTYPE))
		return parse_while_stmt(parser);

    return parse_expr_stmt(parser);
}

Stmt *parse_expr_stmt(Parser *parser){
    Expr *expr = parse_expr(parser);
    consume(parser, SEMICOLON_TOKTYPE, "Expect ';' at end of statement expression.");
    
    ExprStmt *expr_stmt = (ExprStmt *)memory_alloc(sizeof(ExprStmt));
    expr_stmt->expr = expr;

    Stmt *stmt = (Stmt *)memory_alloc(sizeof(Stmt));
    stmt->type = EXPR_STMTTYPE;
    stmt->sub_stmt = expr_stmt;

    return stmt;
}

Stmt *parse_print_stmt(Parser *parser){
    Token *print_token = previous(parser);
    Expr *expr = parse_expr(parser);

    consume(parser, SEMICOLON_TOKTYPE, "Expect ';' at end of statement expression.");

    PrintStmt *print_stmt = (PrintStmt *)memory_alloc(sizeof(PrintStmt));
    print_stmt->expr = expr;
    print_stmt->print_token = print_token;

    return create_stmt(PRINT_STMTTYPE, print_stmt);
}

Stmt *parse_var_decl_stmt(Parser *parser){
    char is_const = 0;
    char is_initialized = 0;
    Token *identifier_token = NULL;
    Expr *initializer_expr = NULL;

    is_const = previous(parser)->type == IMUT_TOKTYPE;

    identifier_token = consume(
        parser, 
        IDENTIFIER_TOKTYPE, 
        "Expect symbol name after 'var' keyword.");
    
    if(match(parser, 1, EQUALS_TOKTYPE)){
        is_initialized = 1;
        initializer_expr = parse_expr(parser);
    }

    consume(
        parser, 
        SEMICOLON_TOKTYPE, 
        "Expect ';' at end of symbol declaration.");

    VarDeclStmt *var_decl_stmt = (VarDeclStmt *)memory_alloc(sizeof(VarDeclStmt));
    var_decl_stmt->is_const = is_const;
    var_decl_stmt->is_initialized = is_initialized;
    var_decl_stmt->identifier_token = identifier_token;
    var_decl_stmt->initializer_expr = initializer_expr;

    return create_stmt(VAR_DECL_STMTTYPE, var_decl_stmt);
}

DynArrPtr *parse_block_stmt(Parser *parser){
    DynArrPtr *stmts = memory_dynarr_ptr();

    while (!check(parser, RIGHT_BRACKET_TOKTYPE))
    {
        Stmt *stmt = parse_stmt(parser);
        dynarr_ptr_insert(stmt, stmts);
    }

    consume(parser, RIGHT_BRACKET_TOKTYPE, "Expect '}' at end of block statement.");
    
    return stmts;
}

Stmt *parse_if_stmt(Parser *parser){
	Expr *if_condition = NULL;
	DynArrPtr *if_stmts = NULL;
	DynArrPtr *else_stmts = NULL;

	consume(parser, LEFT_PAREN_TOKTYPE, "Expect '(' after 'if' keyword.");
	if_condition = parse_expr(parser);
	consume(parser, RIGHT_PAREN_TOKTYPE, "Expect ')' at end of if condition.");

	consume(parser, LEFT_BRACKET_TOKTYPE, "Expect '{' at start of if body.");
	if_stmts = parse_block_stmt(parser);

	if(match(parser, 1, ELSE_TOKTYPE)){
		consume(parser, LEFT_BRACKET_TOKTYPE, "Expect '{' at start of else body.");
		else_stmts = parse_block_stmt(parser);
	}

	IfStmt *if_stmt = (IfStmt *)memory_alloc(sizeof(IfStmt));
	if_stmt->if_condition = if_condition;
	if_stmt->if_stmts = if_stmts;
	if_stmt->else_stmts = else_stmts;

	return create_stmt(IF_STMTTYPE, if_stmt);
}

Stmt *parse_while_stmt(Parser *parser){
	Expr *condition = NULL;
	DynArrPtr *stmts = NULL;

	consume(parser, LEFT_PAREN_TOKTYPE, "Expect '(' after 'while' keyword.");
	condition = parse_expr(parser);
	consume(parser, RIGHT_PAREN_TOKTYPE, "Expect ')' at end of while statement condition.");

	consume(parser, LEFT_BRACKET_TOKTYPE, "Expect '{' at start of while statement body.");
	stmts = parse_block_stmt(parser);

	WhileStmt *while_stmt = (WhileStmt *)memory_alloc(sizeof(WhileStmt));
	while_stmt->condition = condition;
	while_stmt->stmts = stmts;

	return create_stmt(WHILE_STMTTYPE, while_stmt);
}

Parser *parser_create(){
	Parser *parser = (Parser *)memory_alloc(sizeof(Parser));
	memset(parser, 0, sizeof(Parser));
	return parser;
}

int parser_parse(DynArrPtr *tokens, DynArrPtr *stmts, Parser *parser){
	if(setjmp(err_jmp) == 1) return 1;
    else {
        parser->current = 0;
        parser->tokens = tokens;
        parser->stmt = stmts;

        while(!is_at_end(parser)){
            Stmt *stmt = parse_stmt(parser);
            dynarr_ptr_insert(stmt, stmts);
        }

        return 0;
    }
}
