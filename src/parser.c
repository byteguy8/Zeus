#include "parser.h"
#include "memory.h"
#include "token.h"
#include "stmt.h"
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <setjmp.h>
#include <assert.h>

static jmp_buf err_jmp;

static void error(Parser *parser, Token *token, char *msg, ...){
    va_list args;
	va_start(args, msg);

	fprintf(stderr, "Parser error at line %d in file '%s':\n\t", token->line, token->pathname);
	vfprintf(stderr, msg, args);
    fprintf(stderr, "\n");

	va_end(args);

    longjmp(err_jmp, 1);
}

Expr *create_expr(ExprType type, void *sub_expr){
    Expr *expr = (Expr *)A_COMPILE_ALLOC(sizeof(Expr));
    expr->type = type;
    expr->sub_expr = sub_expr;

    return expr;
}

Stmt *create_stmt(StmtType type, void *sub_stmt){
    Stmt *stmt = (Stmt *)A_COMPILE_ALLOC(sizeof(Stmt));
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

	fprintf(stderr, "Parser error at line %d in file '%s':\n\t", token->line, token->pathname);
	vfprintf(stderr, err_msg, args);
    fprintf(stderr, "\n");

	va_end(args);

    longjmp(err_jmp, 1);
}

Expr *parse_expr(Parser *paser);
Expr *parse_is_expr(Parser *parser);
Expr *parse_assign(Parser *parser);
Expr *parse_dict(Parser *parser);
Expr *parse_list(Parser *parser);
Expr *parse_record(Parser *parser);
Expr *parse_or(Parser *parser);
Expr *parse_and(Parser *parser);
Expr *parse_comparison(Parser *parser);
Expr *parse_term(Parser *parser);
Expr *parse_factor(Parser *parser);
Expr *parse_unary(Parser *parser);
Expr *parse_call(Parser *parser);
Expr *parse_literal(Parser *parser);

Stmt *parse_stmt(Parser *parser);
Stmt *parse_expr_stmt(Parser *parser);
Stmt *parse_print_stmt(Parser *parser);
DynArrPtr *parse_block_stmt(Parser *parser);
Stmt *parse_if_stmt(Parser *parser);
Stmt *parse_while_stmt(Parser *parser);
Stmt *parse_throw_stmt(Parser *parser);
Stmt *parse_try_stmt(Parser *parser);
Stmt *parse_return_stmt(Parser *parser);

Stmt *parse_var_decl_stmt(Parser *parser);
Stmt *parse_function_stmt(Parser *parser);
Stmt *parse_import_stmt(Parser *parser);
Stmt *parse_load_stmt(Parser *parser);

Expr *parse_expr(Parser *parser){
	return parse_is_expr(parser);
}

Expr *parse_is_expr(Parser *parser){
	Expr *left = parse_assign(parser);

	if(match(parser, 1, IS_TOKTYPE)){
		Token *is_token = NULL;
		Token *type_token = NULL;

		is_token = previous(parser);

		if(match(parser, 7, 
			             EMPTY_TOKTYPE,
					     BOOL_TOKTYPE,
					     INT_TOKTYPE,
					     FLOAT_TOKTYPE,
					     STR_TOKTYPE,
					     LIST_TOKTYPE,
					     DICT_TOKTYPE,
                         RECORD_TOKTYPE)){
			type_token = previous(parser);
		}

		if(!type_token)
			error(parser, is_token, "Expect type after 'is' keyword.");

		IsExpr *is_expr = (IsExpr *)A_COMPILE_ALLOC(sizeof(IsExpr));
		is_expr->left_expr = left;
		is_expr->is_token = is_token;
		is_expr->type_token = type_token;

		return create_expr(IS_EXPRTYPE, is_expr);
	}

	return left;
}

Expr *parse_assign(Parser *parser){
    Expr *expr = parse_record(parser);
	
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

		CompoundExpr *compound_expr = (CompoundExpr *)A_COMPILE_ALLOC(sizeof(CompoundExpr));
		compound_expr->identifier_token = identifier_token;
		compound_expr->operator = operator;
		compound_expr->right = right;

		return create_expr(COMPOUND_EXPRTYPE, compound_expr);
	}

    if(match(parser, 1, EQUALS_TOKTYPE)){
		Token *equals_token = previous(parser);
        Expr *value_expr = parse_assign(parser);

        AssignExpr *assign_expr = (AssignExpr *)A_COMPILE_ALLOC(sizeof(AssignExpr));
        assign_expr->left = expr;
		assign_expr->equals_token = equals_token;
        assign_expr->value_expr = value_expr;

        return create_expr(ASSIGN_EXPRTYPE, assign_expr);
    }

    return expr;
}

DynArrPtr *record_key_values(Token *record_token, Parser *parser){
	DynArrPtr *key_values = runtime_dynarr_ptr();

	do{
		if(key_values->used >= 255)
			error(parser, record_token, "Records can have more than 255 key values.");

		Token *key = consume(parser, IDENTIFIER_TOKTYPE, "Expect record key.");
		consume(parser, COLON_TOKTYPE, "Expect ':' after record key.");
		Expr *value = parse_expr(parser);

		RecordExprValue *key_value = (RecordExprValue *)A_COMPILE_ALLOC(sizeof(RecordExprValue));
		key_value->key = key;
		key_value->value = value;

		dynarr_ptr_insert(key_value, key_values);
	}while(match(parser, 1, COMMA_TOKTYPE));

	return key_values;
}


Expr *parse_record(Parser *parser){
	if(match(parser, 1, RECORD_TOKTYPE)){
		Token *record_token = NULL;
		DynArrPtr *key_values = NULL;

		record_token = previous(parser);
		consume(parser, LEFT_BRACKET_TOKTYPE, "Expect '{' after 'record' keyword at start of record body.");

		if(!check(parser, RIGHT_BRACKET_TOKTYPE))
			key_values = record_key_values(record_token, parser);

		consume(parser, RIGHT_BRACKET_TOKTYPE, "Expect '}' at end of record body.");
		
		RecordExpr *record_expr = (RecordExpr *)A_COMPILE_ALLOC(sizeof(RecordExpr));
		record_expr->record_token = record_token;
		record_expr->key_values = key_values;

		return create_expr(RECORD_EXPRTYPE, record_expr);
	}

	return parse_dict(parser);
}

Expr *parse_dict(Parser *parser){
    if(match(parser, 1, DICT_TOKTYPE)){
        Token *dict_token = NULL;
        DynArrPtr *key_values = NULL;

        dict_token = previous(parser);
        consume(parser, LEFT_PAREN_TOKTYPE, "Expect '(' after 'dict' keyword.");
		
		if(!check(parser, RIGHT_PAREN_TOKTYPE)){
			key_values = compile_dynarr_ptr();
			do{
				Expr *key = parse_expr(parser);
                consume(parser, TO_TOKTYPE, "Expect 'to' after keyword.");
                Expr *value = parse_expr(parser);
                
                DictKeyValue *key_value = (DictKeyValue *)A_COMPILE_ALLOC(sizeof(DictKeyValue));
                key_value->key = key;
                key_value->value = value;
				
                dynarr_ptr_insert(key_value, key_values);
			}while(match(parser, 1, COMMA_TOKTYPE));
		}

		consume(parser, RIGHT_PAREN_TOKTYPE, "Expect ')' at end of list expression.");

		DictExpr *dict_expr = (DictExpr *)A_COMPILE_ALLOC(sizeof(DictExpr));
		dict_expr->dict_token = dict_token;
        dict_expr->key_values = key_values;

		return create_expr(DICT_EXPRTYPE, dict_expr);
    }

    return parse_list(parser);
}

Expr *parse_list(Parser *parser){
	if(match(parser, 1, LIST_TOKTYPE)){
		Token *list_token = NULL;
		DynArrPtr *exprs = NULL;
	
		list_token = previous(parser);
		consume(parser, LEFT_PAREN_TOKTYPE, "Expect '(' after 'list' keyword.");
		
		if(!check(parser, RIGHT_PAREN_TOKTYPE)){
			exprs = compile_dynarr_ptr();
			do{
				Expr *expr = parse_expr(parser);
				dynarr_ptr_insert(expr, exprs);
			}while(match(parser, 1, COMMA_TOKTYPE));
		}

		consume(parser, RIGHT_PAREN_TOKTYPE, "Expect ')' at end of list expression.");

		ListExpr *list_expr = (ListExpr *)A_COMPILE_ALLOC(sizeof(ListExpr));
		list_expr->list_token = list_token;
		list_expr->exprs = exprs;

		return create_expr(LIST_EXPRTYPE, list_expr);
	}

	return parse_or(parser);
}

Expr *parse_or(Parser *parser){
    Expr *left = parse_and(parser);

	while(match(parser, 1, OR_TOKTYPE)){
		Token *operator = previous(parser);
		Expr *right = parse_and(parser);

		LogicalExpr *logical_expr = (LogicalExpr *)A_COMPILE_ALLOC(sizeof(LogicalExpr));
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

		LogicalExpr *logical_expr = (LogicalExpr *)A_COMPILE_ALLOC(sizeof(LogicalExpr));
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

		ComparisonExpr *comparison_expr = (ComparisonExpr *)A_COMPILE_ALLOC(sizeof(ComparisonExpr));
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

		BinaryExpr *binary_expr = (BinaryExpr *)A_COMPILE_ALLOC(sizeof(BinaryExpr));
		binary_expr->left = left;
		binary_expr->operator = operator;
		binary_expr->right = right;

		left = create_expr(BINARY_EXPRTYPE, binary_expr);
	}

	return left;
}

Expr *parse_factor(Parser *parser){
	Expr *left = parse_unary(parser);

	while(match(parser, 3, ASTERISK_TOKTYPE, SLASH_TOKTYPE, MOD_TOKTYPE)){
		Token *operator = previous(parser);
		Expr *right = parse_unary(parser);

		BinaryExpr *binary_expr = (BinaryExpr *)A_COMPILE_ALLOC(sizeof(BinaryExpr));
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

        UnaryExpr *unary_expr = (UnaryExpr *)A_COMPILE_ALLOC(sizeof(UnaryExpr));
        unary_expr->operator = operator;
        unary_expr->right = right;

        return create_expr(UNARY_EXPRTYPE, unary_expr);
    }

    return parse_call(parser);
}

Expr *parse_call(Parser *parser){
    Expr *left = parse_literal(parser);

    if(check(parser, DOT_TOKTYPE) ||
       check(parser, LEFT_PAREN_TOKTYPE)){

        while (match(parser, 2, DOT_TOKTYPE, LEFT_PAREN_TOKTYPE)){
            Token *token = previous(parser);

            switch (token->type){
                case DOT_TOKTYPE:{
                    Token *symbol_token = consume(parser, IDENTIFIER_TOKTYPE, "Expect identifier.");
                    
                    AccessExpr *access_expr = (AccessExpr *)A_COMPILE_ALLOC(sizeof(AccessExpr));
                    access_expr->left = left;
                    access_expr->dot_token = token;
                    access_expr->symbol_token = symbol_token;
                    
                    left = create_expr(ACCESS_EXPRTYPE, access_expr);

                    break;
                }
                case LEFT_PAREN_TOKTYPE:{
                    DynArrPtr *args = NULL;
        
                    if(!check(parser, RIGHT_PAREN_TOKTYPE)){
                        args = compile_dynarr_ptr();

                        do{
                            Expr *expr = parse_expr(parser);
                            dynarr_ptr_insert(expr, args);
                        } while (match(parser, 1, COMMA_TOKTYPE));
                    }

                    consume(parser, RIGHT_PAREN_TOKTYPE, "Expect ')' after call arguments.");

                    CallExpr *call_expr = (CallExpr *)A_COMPILE_ALLOC(sizeof(CallExpr));
                    call_expr->left = left;
                    call_expr->left_paren = token;
                    call_expr->args = args;

                    left = create_expr(CALL_EXPRTYPE, call_expr);
                    
                    break;
                }
                default:{
                    assert("Illegal token type");
                }
            }
        }
    }

    return left;
}

Expr *parse_literal(Parser *parser){
    if(match(parser, 1, EMPTY_TOKTYPE)){
        Token *empty_token = previous(parser);
        
        EmptyExpr *empty_expr = (EmptyExpr *)A_COMPILE_ALLOC(sizeof(EmptyExpr));
        empty_expr->empty_token = empty_token;

        return create_expr(EMPTY_EXPRTYPE, empty_expr);
    }

	if(match(parser, 1, FALSE_TOKTYPE)){
        Token *bool_token = previous(parser);

        BoolExpr *bool_expr = (BoolExpr *)A_COMPILE_ALLOC(sizeof(BoolExpr));
        bool_expr->value = 0;
        bool_expr->bool_token = bool_token;

        return create_expr(BOOL_EXPRTYPE, bool_expr);
    }

    if(match(parser, 1, TRUE_TOKTYPE)){
        Token *bool_token = previous(parser);

        BoolExpr *bool_expr = (BoolExpr *)A_COMPILE_ALLOC(sizeof(BoolExpr));
        bool_expr->value = 1;
        bool_expr->bool_token = bool_token;
        
        return create_expr(BOOL_EXPRTYPE, bool_expr);
    }

	if(match(parser, 1, INT_TYPE_TOKTYPE)){
		Token *int_token = previous(parser);

        IntExpr *int_expr = (IntExpr *)A_COMPILE_ALLOC(sizeof(IntExpr));
		int_expr->token = int_token;

        return create_expr(INT_EXPRTYPE, int_expr);
	}
	
	if(match(parser, 1, FLOAT_TYPE_TOKTYPE)){
		Token *float_token = previous(parser);

        FloatExpr *float_expr = (FloatExpr *)A_COMPILE_ALLOC(sizeof(FloatExpr));
		float_expr->token = float_token;

        return create_expr(FLOAT_EXPRTYPE, float_expr);
	}

	if(match(parser, 1, STR_TYPE_TOKTYPE)){
		Token *string_token = previous(parser);
		size_t literal_size = string_token->literal_size;
		
		StringExpr *string_expr = (StringExpr *)A_COMPILE_ALLOC(sizeof(StringExpr));
		string_expr->hash = literal_size > 0 ? *(uint32_t *)string_token->literal : 0;
		string_expr->string_token = string_token;

		return create_expr(STRING_EXPRTYPE, string_expr);
	}

    if(match(parser, 1, LEFT_PAREN_TOKTYPE)){
        Token *left_paren_token = previous(parser);
        Expr *group_sub_expr = parse_expr(parser);

        GroupExpr *group_expr = (GroupExpr *)A_COMPILE_ALLOC(sizeof(GroupExpr));
        group_expr->left_paren_token = left_paren_token;
        group_expr->expr = group_sub_expr;
        
        consume(parser, RIGHT_PAREN_TOKTYPE, "Expect ')' after expression in group expression");
        
        return create_expr(GROUP_EXPRTYPE, group_expr);
    }

    if(match(parser, 1, IDENTIFIER_TOKTYPE)){
        Token *identifier_token = previous(parser);
        
        IdentifierExpr *identifier_expr = (IdentifierExpr *)A_COMPILE_ALLOC(sizeof(IdentifierExpr));
        identifier_expr->identifier_token= identifier_token;
        
        return create_expr(IDENTIFIER_EXPRTYPE, identifier_expr);
    }

	Token *token = peek(parser);
    
    error(
        parser, 
        token, 
        "Expect something: 'false', 'true' or integer, but got '%s'", 
        token->lexeme
    );
    
    return NULL;
}

Stmt *parse_stmt(Parser *parser){
    if(match(parser, 1, PRINT_TOKTYPE))
        return parse_print_stmt(parser);

    if(match(parser, 2, MUT_TOKTYPE, IMUT_TOKTYPE))
        return parse_var_decl_stmt(parser);

    if(match(parser, 1, LEFT_BRACKET_TOKTYPE)){
        DynArrPtr *stmts = parse_block_stmt(parser);
        
        BlockStmt *block_stmt = (BlockStmt *)A_COMPILE_ALLOC(sizeof(BlockStmt));
        block_stmt->stmts = stmts;

        return create_stmt(BLOCK_STMTTYPE, block_stmt);
    }

	if(match(parser, 1, IF_TOKTYPE))
		return parse_if_stmt(parser);

	if(match(parser, 1, WHILE_TOKTYPE))
		return parse_while_stmt(parser);

	if(match(parser, 1, STOP_TOKTYPE)){
		Token *stop_token = previous(parser);
		consume(parser, SEMICOLON_TOKTYPE, "Expect ';' at end of 'stop' statement.");
		
		StopStmt *stop_stmt = (StopStmt *)A_COMPILE_ALLOC(sizeof(StopStmt));
		stop_stmt->stop_token = stop_token;

		return create_stmt(STOP_STMTTYPE, stop_stmt);
	}

    if(match(parser, 1, CONTINUE_TOKTYPE)){
        Token *continue_token = previous(parser);
        consume(parser, SEMICOLON_TOKTYPE, "Expect ';' at end of 'continue' statement.");

        ContinueStmt *continue_stmt = (ContinueStmt *)A_COMPILE_ALLOC(sizeof(ContinueStmt));
        continue_stmt->continue_token = continue_token;

        return create_stmt(CONTINUE_STMTTYPE, continue_stmt);
    }

    if(match(parser, 1, RET_TOKTYPE))
        return parse_return_stmt(parser);

    if(match(parser, 1, PROC_TOKTYPE))
        return parse_function_stmt(parser);

    if(match(parser, 1, IMPORT_TOKTYPE))
        return parse_import_stmt(parser);

    if(match(parser, 1, LOAD_TOKTYPE))
        return parse_load_stmt(parser);

    if(match(parser, 1, THROW_TOKTYPE))
        return parse_throw_stmt(parser);

    if(match(parser, 1, TRY_TOKTYPE))
        return parse_try_stmt(parser);

    return parse_expr_stmt(parser);
}

Stmt *parse_expr_stmt(Parser *parser){
    Expr *expr = parse_expr(parser);
    consume(parser, SEMICOLON_TOKTYPE, "Expect ';' at end of statement expression.");
    
    ExprStmt *expr_stmt = (ExprStmt *)A_COMPILE_ALLOC(sizeof(ExprStmt));
    expr_stmt->expr = expr;

    Stmt *stmt = (Stmt *)A_COMPILE_ALLOC(sizeof(Stmt));
    stmt->type = EXPR_STMTTYPE;
    stmt->sub_stmt = expr_stmt;

    return stmt;
}

Stmt *parse_print_stmt(Parser *parser){
    Token *print_token = previous(parser);
    Expr *expr = parse_expr(parser);

    consume(parser, SEMICOLON_TOKTYPE, "Expect ';' at end of statement expression.");

    PrintStmt *print_stmt = (PrintStmt *)A_COMPILE_ALLOC(sizeof(PrintStmt));
    print_stmt->expr = expr;
    print_stmt->print_token = print_token;

    return create_stmt(PRINT_STMTTYPE, print_stmt);
}

DynArrPtr *parse_block_stmt(Parser *parser){
    DynArrPtr *stmts = compile_dynarr_ptr();

    while (!check(parser, RIGHT_BRACKET_TOKTYPE))
    {
        Stmt *stmt = parse_stmt(parser);
        dynarr_ptr_insert(stmt, stmts);
    }

    consume(parser, RIGHT_BRACKET_TOKTYPE, "Expect '}' at end of block statement.");
    
    return stmts;
}

Stmt *parse_if_stmt(Parser *parser){
    Token *if_token = NULL;
	Expr *if_condition = NULL;
	DynArrPtr *if_stmts = NULL;
	DynArrPtr *else_stmts = NULL;

    if_token = previous(parser);

	consume(parser, LEFT_PAREN_TOKTYPE, "Expect '(' after 'if' keyword.");
	if_condition = parse_expr(parser);
	consume(parser, RIGHT_PAREN_TOKTYPE, "Expect ')' at end of if condition.");

	consume(parser, LEFT_BRACKET_TOKTYPE, "Expect '{' at start of if body.");
	if_stmts = parse_block_stmt(parser);

	if(match(parser, 1, ELSE_TOKTYPE)){
		consume(parser, LEFT_BRACKET_TOKTYPE, "Expect '{' at start of else body.");
		else_stmts = parse_block_stmt(parser);
	}

	IfStmt *if_stmt = (IfStmt *)A_COMPILE_ALLOC(sizeof(IfStmt));
    if_stmt->if_token = if_token;
	if_stmt->if_condition = if_condition;
	if_stmt->if_stmts = if_stmts;
	if_stmt->else_stmts = else_stmts;

	return create_stmt(IF_STMTTYPE, if_stmt);
}

Stmt *parse_while_stmt(Parser *parser){
	Token *while_token = NULL;
    Expr *condition = NULL;
	DynArrPtr *stmts = NULL;

	while_token = previous(parser);
	consume(parser, LEFT_PAREN_TOKTYPE, "Expect '(' after 'while' keyword.");
	condition = parse_expr(parser);
	consume(parser, RIGHT_PAREN_TOKTYPE, "Expect ')' at end of while statement condition.");

	consume(parser, LEFT_BRACKET_TOKTYPE, "Expect '{' at start of while statement body.");
	stmts = parse_block_stmt(parser);

	WhileStmt *while_stmt = (WhileStmt *)A_COMPILE_ALLOC(sizeof(WhileStmt));
    while_stmt->while_token = while_token;
	while_stmt->condition = condition;
	while_stmt->stmts = stmts;

	return create_stmt(WHILE_STMTTYPE, while_stmt);
}

Stmt *parse_throw_stmt(Parser *parser){
    Token *throw_token = NULL;
	Expr *value = NULL;

    throw_token = previous(parser);
  
	if(!check(parser, SEMICOLON_TOKTYPE))
		value = parse_expr(parser);

    consume(parser, SEMICOLON_TOKTYPE, "Expect ';' at end of throw statement.");

    ThrowStmt *throw_stmt = (ThrowStmt *)A_COMPILE_ALLOC(sizeof(ThrowStmt));
    throw_stmt->throw_token = throw_token;
	throw_stmt->value = value;

    return create_stmt(THROW_STMTTYPE, throw_stmt);
}

Stmt *parse_try_stmt(Parser *parser){
    Token *try_token = NULL;
    DynArrPtr *try_stmts = NULL;
	Token *catch_token = NULL;
	Token *err_identifier = NULL;
	DynArrPtr *catch_stmts = NULL;

    try_token = previous(parser);
    consume(parser, LEFT_BRACKET_TOKTYPE, "Expect '{' after 'try' keyword.");
    try_stmts = parse_block_stmt(parser);

	if(match(parser, 1, CATCH_TOKTYPE)){
		catch_token = previous(parser);
		err_identifier = consume(parser, IDENTIFIER_TOKTYPE, "Expect error identifier after 'catch' keyword");
		consume(parser, LEFT_BRACKET_TOKTYPE, "Expect '{' after 'catch' keyword.");
		catch_stmts = parse_block_stmt(parser);
	}

    TryStmt *try_stmt = (TryStmt *)A_COMPILE_ALLOC(sizeof(TryStmt));
    try_stmt->try_token = try_token;
    try_stmt->try_stmts = try_stmts;
	try_stmt->catch_token = catch_token;
	try_stmt->err_identifier = err_identifier;
	try_stmt->catch_stmts = catch_stmts;

    return create_stmt(TRY_STMTTYPE, try_stmt);
}

Stmt *parse_return_stmt(Parser *parser){
    Token *return_token = NULL;
    Expr *value = NULL;

    return_token = previous(parser);
    
    if(!check(parser, SEMICOLON_TOKTYPE))
        value = parse_expr(parser);

    consume(parser, SEMICOLON_TOKTYPE, "Expect ';' at end of return statement.");

    ReturnStmt *return_stmt = (ReturnStmt *)A_COMPILE_ALLOC(sizeof(ReturnStmt));
    return_stmt->return_token = return_token;
    return_stmt->value = value;
    
    return create_stmt(RETURN_STMTTYPE, return_stmt);
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
        "Expect symbol name after 'imut' or 'mut' keyword.");
    
    if(match(parser, 1, EQUALS_TOKTYPE)){
        is_initialized = 1;
        initializer_expr = parse_expr(parser);
    }

    consume(
        parser, 
        SEMICOLON_TOKTYPE, 
        "Expect ';' at end of symbol declaration.");

    VarDeclStmt *var_decl_stmt = (VarDeclStmt *)A_COMPILE_ALLOC(sizeof(VarDeclStmt));
    var_decl_stmt->is_const = is_const;
    var_decl_stmt->is_initialized = is_initialized;
    var_decl_stmt->identifier_token = identifier_token;
    var_decl_stmt->initializer_expr = initializer_expr;

    return create_stmt(VAR_DECL_STMTTYPE, var_decl_stmt);
}

Stmt *parse_function_stmt(Parser *parser){
    Token *name_token = NULL;
    DynArrPtr *params = NULL;
    DynArrPtr *stmts = NULL;

    name_token = consume(parser, IDENTIFIER_TOKTYPE, "Expect function name after 'proc' keyword.");
    consume(parser, LEFT_PAREN_TOKTYPE, "Expect '(' after function name.");

    if(!check(parser, RIGHT_PAREN_TOKTYPE)){
        params = compile_dynarr_ptr();
        
        do{
            Token *param_token = consume(parser, IDENTIFIER_TOKTYPE, "Expect function parameter name.");
            dynarr_ptr_insert(param_token, params);
        } while (match(parser, 1, COMMA_TOKTYPE));
    }

    consume(parser, RIGHT_PAREN_TOKTYPE, "Expect ')' at end of function parameters.");
    consume(parser, LEFT_BRACKET_TOKTYPE, "Expect '{' at start of function body.");
    stmts = parse_block_stmt(parser);

    FunctionStmt *function_stmt = (FunctionStmt *)A_COMPILE_ALLOC(sizeof(FunctionStmt));
    function_stmt->name_token = name_token;
    function_stmt->params = params;
    function_stmt->stmts = stmts;

    return create_stmt(FUNCTION_STMTTYPE, function_stmt);
}

Stmt *parse_import_stmt(Parser *parser){
    Token *import_token = NULL;
    Token *name = NULL;
    Token *alt_name = NULL;

    import_token = previous(parser);
    name = consume(parser, IDENTIFIER_TOKTYPE, "Expect module name");
    
    if(match(parser, 1, AS_TOKTYPE))
        alt_name = consume(parser, IDENTIFIER_TOKTYPE, "Expect module alternative name after 'as' keyword");

    consume(parser, SEMICOLON_TOKTYPE, "Expect ';' at end of import statement.");

    ImportStmt *import_stmt = (ImportStmt *)A_COMPILE_ALLOC(sizeof(ImportStmt));
    
    import_stmt->import_token = import_token;
    import_stmt->name = name;
    import_stmt->alt_name = alt_name;

    return create_stmt(IMPORT_STMTTYPE, import_stmt);
}

Stmt *parse_load_stmt(Parser *parser){
    Token *load_token = NULL;
    Token *path_token = NULL;
    Token *name_token = NULL;

    load_token = previous(parser);
    path_token = consume(parser, STR_TYPE_TOKTYPE, "Expect path after 'load' keyword");
    
    consume(parser, AS_TOKTYPE, "Expect 'as' after native library path");

    name_token = consume(parser, IDENTIFIER_TOKTYPE, "Expect name for native library");
    
    consume(parser, SEMICOLON_TOKTYPE, "Expect ';' at end of load statement");

    LoadStmt *load_stmt = (LoadStmt *)A_COMPILE_ALLOC(sizeof(ImportStmt));
    
    load_stmt->load_token = load_token;
    load_stmt->pathname = path_token;
    load_stmt->name = name_token;

    return create_stmt(LOAD_STMTTYPE, load_stmt);
}

Parser *parser_create(){
	Parser *parser = (Parser *)A_COMPILE_ALLOC(sizeof(Parser));
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
