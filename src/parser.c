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
Expr *parse_term(Parser *parser);
Expr *parse_factor(Parser *parser);
Expr *parse_literal(Parser *parser);

Stmt *parse_stmt(Parser *parser);
Stmt *parse_expr_stmt(Parser *parser);
Stmt *parse_print_stmt(Parser *parser);

Expr *parse_expr(Parser *parser){
	return parse_term(parser);
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

		Expr *expr = memory_alloc(sizeof(Expr));
		expr->type = BINARY_EXPRTYPE;
		expr->sub_expr = binary_expr;

		left = expr;
	}

	return left;
}

Expr *parse_factor(Parser *parser){
	Expr *left = parse_literal(parser);

	while(match(parser, 2, ASTERISK_TOKTYPE, SLASH_TOKTYPE)){
		Token *operator = previous(parser);
		Expr *right = parse_literal(parser);

		BinaryExpr *binary_expr = (BinaryExpr *)memory_alloc(sizeof(BinaryExpr));
		binary_expr->left = left;
		binary_expr->operator = operator;
		binary_expr->right = right;

		Expr *expr = memory_alloc(sizeof(Expr));
		expr->type = BINARY_EXPRTYPE;
		expr->sub_expr = binary_expr;

		left = expr;
	}

	return left;
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

	Token *token = peek(parser);
    error(parser, token, "Expect something: 'false', 'true' or integer, but got '%s'", token->lexeme);
    return NULL;
}

Stmt *parse_stmt(Parser *parser){
    if(match(parser, 1, PRINT_TOKTYPE))
        return parse_print_stmt(parser);

    return parse_expr_stmt(parser);
}

Stmt *parse_print_stmt(Parser *parser){
    Token *print_token = previous(parser);
    Expr *expr = parse_expr(parser);

    consume(parser, SEMICOLON_TOKTYPE, "Expect ';' at end of statement expression.");

    PrintStmt *print_stmt = (PrintStmt *)memory_alloc(sizeof(PrintStmt));
    print_stmt->expr = expr;
    print_stmt->print_token = print_token;

    Stmt *stmt = (Stmt *)memory_alloc(sizeof(Stmt));
    stmt->type = PRINT_STMTTYPE;
    stmt->sub_stmt = print_stmt;

    return stmt;
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