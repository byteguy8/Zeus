#include "parser.h"
#include "expr.h"
#include "memory.h"
#include "factory.h"
#include "token.h"
#include "stmt.h"
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <setjmp.h>
#include <assert.h>

//> PRIVATE INTERFACE
static void error(Parser *parser, Token *token, char *msg, ...);
static Expr *create_expr(ExprType type, void *sub_expr, Parser *parser);
static Stmt *create_stmt(StmtType type, void *sub_stmt, Parser *parser);
static Token *peek(Parser *parser);
static Token *previous(Parser *parser);
static int is_at_end(Parser *parser);
static int match(Parser *parser, int count, ...);
static int check(Parser *parser, TokenType type);
Token *consume(Parser *parser, TokenType type, char *err_msg, ...);
DynArrPtr *record_key_values(Token *record_token, Parser *parser);
// EXPRESSIONS
Expr *parse_expr(Parser *paser);
Expr *parse_assign(Parser *parser);
Expr *parse_is_expr(Parser *parser);
Expr *parse_tenary_expr(Parser *parser);
Expr *parse_or(Parser *parser);
Expr *parse_and(Parser *parser);
Expr *parse_comparison(Parser *parser);
Expr *parse_term(Parser *parser);
Expr *parse_factor(Parser *parser);
Expr *parse_unary(Parser *parser);
Expr *parse_call(Parser *parser);
Expr *parse_record(Parser *parser);
Expr *parse_dict(Parser *parser);
Expr *parse_list(Parser *parser);
Expr *parse_array(Parser *parser);
Expr *parse_literal(Parser *parser);
// STATEMENTS
Stmt *parse_stmt(Parser *parser);
Stmt *parse_expr_stmt(Parser *parser);
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
Stmt *parse_export_stmt(Parser *parser);
//< PRIVATE INTERFACE

void error(Parser *parser, Token *token, char *msg, ...){
    va_list args;
	va_start(args, msg);

	fprintf(stderr, "Parser error at line %d in file '%s':\n\t", token->line, token->pathname);
	vfprintf(stderr, msg, args);
    fprintf(stderr, "\n");

	va_end(args);

    longjmp(parser->err_jmp, 1);
}

Expr *create_expr(ExprType type, void *sub_expr, Parser *parser){
    Expr *expr = MEMORY_ALLOC(Expr, 1, parser->ctallocator);
    expr->type = type;
    expr->sub_expr = sub_expr;

    return expr;
}

Stmt *create_stmt(StmtType type, void *sub_stmt, Parser *parser){
    Stmt *stmt = MEMORY_ALLOC(Stmt, 1, parser->ctallocator);
    stmt->type = type;
    stmt->sub_stmt = sub_stmt;

    return stmt;
}

Token *peek(Parser *parser){
	DynArrPtr *tokens = parser->tokens;
	return (Token *)DYNARR_PTR_GET(parser->current, tokens);
}

Token *previous(Parser *parser){
	DynArrPtr *tokens = parser->tokens;
	return (Token *)DYNARR_PTR_GET(parser->current - 1, tokens);
}

int is_at_end(Parser *parser){
    Token *token = peek(parser);
	return token->type == EOF_TOKTYPE;
}

int match(Parser *parser, int count, ...){
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

int check(Parser *parser, TokenType type){
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

    longjmp(parser->err_jmp, 1);
}

DynArrPtr *record_key_values(Token *record_token, Parser *parser){
	DynArrPtr *key_values = FACTORY_DYNARR_PTR(parser->ctallocator);

	do{
		if(key_values->used >= 255){
            error(parser, record_token, "Record expressions only accept up to %d values", 255);
        }

		Token *key = consume(parser, IDENTIFIER_TOKTYPE, "Expect record key.");
		consume(parser, COLON_TOKTYPE, "Expect ':' after record key.");
		Expr *value = parse_expr(parser);

		RecordExprValue *key_value = MEMORY_ALLOC(RecordExprValue, 1, parser->ctallocator);

		key_value->key = key;
		key_value->value = value;

		dynarr_ptr_insert(key_value, key_values);
	}while(match(parser, 1, COMMA_TOKTYPE));

	return key_values;
}

Expr *parse_expr(Parser *parser){
	return parse_assign(parser);
}

Expr *parse_assign(Parser *parser){
    Expr *expr = parse_tenary_expr(parser);

	if(match(parser, 4,
		COMPOUND_ADD_TOKTYPE,
		COMPOUND_SUB_TOKTYPE,
		COMPOUND_MUL_TOKTYPE,
		COMPOUND_DIV_TOKTYPE
    )){
		if(expr->type != IDENTIFIER_EXPRTYPE)
            error(
                parser,
                peek(parser),
                "Expect identifier in left side of assignment expression.");

		IdentifierExpr *identifier_expr = (IdentifierExpr *)expr->sub_expr;
		Token *identifier_token = identifier_expr->identifier_token;
		Token *operator = previous(parser);
		Expr *right = parse_assign(parser);

		CompoundExpr *compound_expr = MEMORY_ALLOC(CompoundExpr, 1, parser->ctallocator);

		compound_expr->identifier_token = identifier_token;
		compound_expr->operator = operator;
		compound_expr->right = right;

		return create_expr(COMPOUND_EXPRTYPE, compound_expr, parser);
	}

    if(match(parser, 1, EQUALS_TOKTYPE)){
		Token *equals_token = previous(parser);
        Expr *value_expr = parse_assign(parser);

        AssignExpr *assign_expr = MEMORY_ALLOC(AssignExpr, 1, parser->ctallocator);

        assign_expr->left = expr;
		assign_expr->equals_token = equals_token;
        assign_expr->value_expr = value_expr;

        return create_expr(ASSIGN_EXPRTYPE, assign_expr, parser);
    }

    return expr;
}

Expr *parse_tenary_expr(Parser *parser){
    Expr *condition = parse_is_expr(parser);

    if(match(parser, 1, QUESTION_MARK_TOKTYPE)){
        Token *mark_token = previous(parser);
        Expr *left = parse_tenary_expr(parser);
        consume(parser, COLON_TOKTYPE, "Expect ':' after left side expression");
        Expr *right = parse_tenary_expr(parser);

        TenaryExpr *tenary_expr = MEMORY_ALLOC(TenaryExpr, 1, parser->ctallocator);

        tenary_expr->condition = condition;
        tenary_expr->left = left;
        tenary_expr->mark_token = mark_token;
        tenary_expr->right = right;

        return create_expr(TENARY_EXPRTYPE, tenary_expr, parser);
    }

    return condition;
}

Expr *parse_is_expr(Parser *parser){
	Expr *left = parse_or(parser);

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
            ARRAY_TOKTYPE,
			LIST_TOKTYPE,
			DICT_TOKTYPE,
            RECORD_TOKTYPE
        )){
			type_token = previous(parser);
		}

		if(!type_token)
			error(parser, is_token, "Expect type after 'is' keyword.");

		IsExpr *is_expr = MEMORY_ALLOC(IsExpr, 1, parser->ctallocator);

		is_expr->left_expr = left;
		is_expr->is_token = is_token;
		is_expr->type_token = type_token;

		return create_expr(IS_EXPRTYPE, is_expr, parser);
	}

	return left;
}

Expr *parse_or(Parser *parser){
    Expr *left = parse_and(parser);

	while(match(parser, 1, OR_TOKTYPE)){
		Token *operator = previous(parser);
		Expr *right = parse_and(parser);

		LogicalExpr *logical_expr = MEMORY_ALLOC(LogicalExpr, 1, parser->ctallocator);

		logical_expr->left = left;
		logical_expr->operator = operator;
		logical_expr->right = right;

		left = create_expr(LOGICAL_EXPRTYPE, logical_expr, parser);
	}

    return left;
}

Expr *parse_and(Parser *parser){
    Expr *left = parse_comparison(parser);

	while(match(parser, 1, AND_TOKTYPE)){
		Token *operator = previous(parser);
		Expr *right = parse_comparison(parser);

		LogicalExpr *logical_expr = MEMORY_ALLOC(LogicalExpr, 1, parser->ctallocator);

		logical_expr->left = left;
		logical_expr->operator = operator;
		logical_expr->right = right;

		left = create_expr(LOGICAL_EXPRTYPE, logical_expr, parser);
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
        NOT_EQUALS_TOKTYPE
    )){
		Token *operator = previous(parser);
		Expr *right = parse_term(parser);

		ComparisonExpr *comparison_expr = MEMORY_ALLOC(ComparisonExpr, 1, parser->ctallocator);

		comparison_expr->left = left;
		comparison_expr->operator = operator;
		comparison_expr->right = right;

		left = create_expr(COMPARISON_EXPRTYPE, comparison_expr, parser);
	}

    return left;
}

Expr *parse_term(Parser *parser){
	Expr *left = parse_factor(parser);

	while(match(parser, 2, PLUS_TOKTYPE, MINUS_TOKTYPE)){
		Token *operator = previous(parser);
		Expr *right = parse_factor(parser);

		BinaryExpr *binary_expr = MEMORY_ALLOC(BinaryExpr, 1, parser->ctallocator);

		binary_expr->left = left;
		binary_expr->operator = operator;
		binary_expr->right = right;

		left = create_expr(BINARY_EXPRTYPE, binary_expr, parser);
	}

	return left;
}

Expr *parse_factor(Parser *parser){
	Expr *left = parse_unary(parser);

	while(match(parser, 3, ASTERISK_TOKTYPE, SLASH_TOKTYPE, MOD_TOKTYPE)){
		Token *operator = previous(parser);
		Expr *right = parse_unary(parser);

		BinaryExpr *binary_expr = MEMORY_ALLOC(BinaryExpr, 1, parser->ctallocator);

		binary_expr->left = left;
		binary_expr->operator = operator;
		binary_expr->right = right;

		left = create_expr(BINARY_EXPRTYPE, binary_expr, parser);
	}

	return left;
}

Expr *parse_unary(Parser *parser){
    if(match(parser, 2, MINUS_TOKTYPE, EXCLAMATION_TOKTYPE)){
        Token *operator = previous(parser);
        Expr *right = parse_unary(parser);

        UnaryExpr *unary_expr = MEMORY_ALLOC(UnaryExpr, 1, parser->ctallocator);

        unary_expr->operator = operator;
        unary_expr->right = right;

        return create_expr(UNARY_EXPRTYPE, unary_expr, parser);
    }

    return parse_call(parser);
}

Expr *parse_call(Parser *parser){
    Expr *left = parse_record(parser);

    if(check(parser, DOT_TOKTYPE) ||
       check(parser, LEFT_PAREN_TOKTYPE) ||
       check(parser, LEFT_SQUARE_TOKTYPE)
    ){
        while (match(parser,3,
            DOT_TOKTYPE,
            LEFT_PAREN_TOKTYPE,
            LEFT_SQUARE_TOKTYPE
        )){
            Token *token = previous(parser);

            switch (token->type){
                case DOT_TOKTYPE:{
                    Token *symbol_token = consume(parser, IDENTIFIER_TOKTYPE, "Expect identifier.");

                    AccessExpr *access_expr = MEMORY_ALLOC(AccessExpr, 1, parser->ctallocator);

                    access_expr->left = left;
                    access_expr->dot_token = token;
                    access_expr->symbol_token = symbol_token;

                    left = create_expr(ACCESS_EXPRTYPE, access_expr, parser);

                    break;
                }case LEFT_PAREN_TOKTYPE:{
                    DynArrPtr *args = NULL;

                    if(!check(parser, RIGHT_PAREN_TOKTYPE)){
                        args = FACTORY_DYNARR_PTR(parser->ctallocator);

                        do{
                            Expr *expr = parse_expr(parser);
                            dynarr_ptr_insert(expr, args);
                        } while (match(parser, 1, COMMA_TOKTYPE));
                    }

                    consume(parser, RIGHT_PAREN_TOKTYPE, "Expect ')' after call arguments.");

                    CallExpr *call_expr = MEMORY_ALLOC(CallExpr, 1, parser->ctallocator);

                    call_expr->left = left;
                    call_expr->left_paren = token;
                    call_expr->args = args;

                    left = create_expr(CALL_EXPRTYPE, call_expr, parser);

                    break;
                }case LEFT_SQUARE_TOKTYPE:{
                    Expr *index = parse_expr(parser);

                    consume(parser, RIGHT_SQUARE_TOKTYPE, "Expect ']' after index expression");

                    IndexExpr *index_expr = MEMORY_ALLOC(IndexExpr, 1, parser->ctallocator);

                    index_expr->target = left;
                    index_expr->left_square_token = token;
                    index_expr->index_expr = index;

                    left = create_expr(INDEX_EXPRTYPE, index_expr, parser);

                    break;
                }default:{
                    assert("Illegal token type");
                }
            }
        }
    }

    return left;
}

Expr *parse_record(Parser *parser){
    if(match(parser, 1, RECORD_TOKTYPE)){
        Token *record_token = NULL;
        DynArrPtr *key_values = NULL;

        record_token = previous(parser);
        consume(parser, LEFT_BRACKET_TOKTYPE, "Expect '{' after 'record' keyword at start of record body.");

        if(!check(parser, RIGHT_BRACKET_TOKTYPE)){
            key_values = record_key_values(record_token, parser);
        }

        consume(parser, RIGHT_BRACKET_TOKTYPE, "Expect '}' at end of record body.");

        RecordExpr *record_expr = MEMORY_ALLOC(RecordExpr, 1, parser->ctallocator);

        record_expr->record_token = record_token;
        record_expr->key_values = key_values;

        return create_expr(RECORD_EXPRTYPE, record_expr, parser);
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
            key_values = FACTORY_DYNARR_PTR(parser->ctallocator);

            do{
                if(DYNARR_PTR_LEN(key_values) >= INT16_MAX){
                    error(parser, dict_token, "Dict expressions only accept up to %d values", INT16_MAX);
                }

                Expr *key = parse_expr(parser);
                consume(parser, TO_TOKTYPE, "Expect 'to' after keyword.");
                Expr *value = parse_expr(parser);

                DictKeyValue *key_value = MEMORY_ALLOC(DictKeyValue, 1, parser->ctallocator);

                key_value->key = key;
                key_value->value = value;

                dynarr_ptr_insert(key_value, key_values);
            }while(match(parser, 1, COMMA_TOKTYPE));
        }

        consume(parser, RIGHT_PAREN_TOKTYPE, "Expect ')' at end of list expression.");

        DictExpr *dict_expr = MEMORY_ALLOC(DictExpr, 1, parser->ctallocator);

        dict_expr->dict_token = dict_token;
        dict_expr->key_values = key_values;

        return create_expr(DICT_EXPRTYPE, dict_expr, parser);
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
            exprs = FACTORY_DYNARR_PTR(parser->ctallocator);

            do{
                if(DYNARR_PTR_LEN(exprs) >= INT16_MAX){
                    error(parser, list_token, "List expressions only accept up to %d values", INT16_MAX);
                }

                Expr *expr = parse_expr(parser);
                dynarr_ptr_insert(expr, exprs);
            }while(match(parser, 1, COMMA_TOKTYPE));
        }

        consume(parser, RIGHT_PAREN_TOKTYPE, "Expect ')' at end of list expression.");

        ListExpr *list_expr = MEMORY_ALLOC(ListExpr, 1, parser->ctallocator);

        list_expr->list_token = list_token;
        list_expr->exprs = exprs;

        return create_expr(LIST_EXPRTYPE, list_expr, parser);
    }

    return parse_array(parser);
}

Expr *parse_array(Parser *parser){
    if(match(parser, 1, ARRAY_TOKTYPE)){
        Token *array_token = NULL;
        Expr *len_expr = NULL;
        DynArrPtr *values = NULL;

        array_token = previous(parser);

        if(match(parser, 1, LEFT_SQUARE_TOKTYPE)){
            len_expr = parse_expr(parser);
            consume(parser, RIGHT_SQUARE_TOKTYPE, "Expect ']' after array length expression");
        }else{
            consume(parser, LEFT_PAREN_TOKTYPE, "Expect '(' after 'array' keyword");

            if(!check(parser, RIGHT_PAREN_TOKTYPE)){
                values = FACTORY_DYNARR_PTR(parser->ctallocator);

                do{
                    if(DYNARR_PTR_LEN(values) >= INT32_MAX){
                        error(parser, array_token, "Array expressions only accept up to %d values", INT16_MAX);
                    }

                    Expr *expr = parse_expr(parser);
                    dynarr_ptr_insert(expr, values);
                } while (match(parser, 1, COMMA_TOKTYPE));
            }

            consume(parser, RIGHT_PAREN_TOKTYPE, "Expect ')' at end of array elements");
        }

        ArrayExpr *array_expr = MEMORY_ALLOC(ArrayExpr, 1, parser->ctallocator);

        array_expr->array_token = array_token;
        array_expr->len_expr = len_expr;
        array_expr->values = values;

        return create_expr(ARRAY_EXPRTYPE, array_expr, parser);
    }

    return parse_literal(parser);
}

Expr *parse_literal(Parser *parser){
    if(match(parser, 1, EMPTY_TOKTYPE)){
        Token *empty_token = previous(parser);

        EmptyExpr *empty_expr = MEMORY_ALLOC(EmptyExpr, 1, parser->ctallocator);

        empty_expr->empty_token = empty_token;

        return create_expr(EMPTY_EXPRTYPE, empty_expr, parser);
    }

	if(match(parser, 1, FALSE_TOKTYPE)){
        Token *bool_token = previous(parser);

        BoolExpr *bool_expr = MEMORY_ALLOC(BoolExpr, 1, parser->ctallocator);

        bool_expr->value = 0;
        bool_expr->bool_token = bool_token;

        return create_expr(BOOL_EXPRTYPE, bool_expr, parser);
    }

    if(match(parser, 1, TRUE_TOKTYPE)){
        Token *bool_token = previous(parser);

        BoolExpr *bool_expr = MEMORY_ALLOC(BoolExpr, 1, parser->ctallocator);

        bool_expr->value = 1;
        bool_expr->bool_token = bool_token;

        return create_expr(BOOL_EXPRTYPE, bool_expr, parser);
    }

	if(match(parser, 1, INT_TYPE_TOKTYPE)){
		Token *int_token = previous(parser);

        IntExpr *int_expr = MEMORY_ALLOC(IntExpr, 1, parser->ctallocator);

		int_expr->token = int_token;

        return create_expr(INT_EXPRTYPE, int_expr, parser);
	}

	if(match(parser, 1, FLOAT_TYPE_TOKTYPE)){
		Token *float_token = previous(parser);

        FloatExpr *float_expr = MEMORY_ALLOC(FloatExpr, 1, parser->ctallocator);

		float_expr->token = float_token;

        return create_expr(FLOAT_EXPRTYPE, float_expr, parser);
	}

	if(match(parser, 1, STR_TYPE_TOKTYPE)){
		Token *string_token = previous(parser);
		size_t literal_size = string_token->literal_size;

		StringExpr *string_expr = MEMORY_ALLOC(StringExpr, 1, parser->ctallocator);

		string_expr->hash = literal_size > 0 ? *(uint32_t *)string_token->literal : 0;
		string_expr->string_token = string_token;

		return create_expr(STRING_EXPRTYPE, string_expr, parser);
	}

    if(match(parser, 1, TEMPLATE_TYPE_TOKTYPE)){
        Token *template_token = previous(parser);
        DynArrPtr *tokens = (DynArrPtr *)template_token->extra;
        DynArrPtr *exprs = FACTORY_DYNARR_PTR(parser->ctallocator);
        Parser *template_parser = parser_create(parser->ctallocator);

        if(parser_parse_template(tokens, exprs, template_parser)){
            error(parser, template_token, "failed to parse template");
        }

        TemplateExpr *template_expr = MEMORY_ALLOC(TemplateExpr, 1, parser->ctallocator);

        template_expr->template_token = template_token;
        template_expr->exprs = exprs;

        return create_expr(TEMPLATE_EXPRTYPE, template_expr, parser);
    }

    if(match(parser, 1, ANON_TOKTYPE)){
        Token *anon_token = NULL;
        DynArrPtr *params = NULL;
        DynArrPtr *stmts = NULL;

        anon_token = previous(parser);
        consume(parser, LEFT_PAREN_TOKTYPE, "Expect '(' after 'anon' keyword");

        if(!check(parser, RIGHT_PAREN_TOKTYPE)){
            params = FACTORY_DYNARR_PTR(parser->ctallocator);

            do{
                Token *param_identifier = consume(parser, IDENTIFIER_TOKTYPE, "Expect parameter identifier");
                dynarr_ptr_insert(param_identifier, params);
            } while (match(parser, 1, COMMA_TOKTYPE));
        }

        consume(parser, RIGHT_PAREN_TOKTYPE, "Expect ')' at end of function parameters");
        consume(parser, LEFT_BRACKET_TOKTYPE, "Expect '{' at start of function body");
        stmts = parse_block_stmt(parser);

        AnonExpr *anon_expr = MEMORY_ALLOC(AnonExpr, 1, parser->ctallocator);

        anon_expr->anon_token = anon_token;
        anon_expr->params = params;
        anon_expr->stmts = stmts;

        return create_expr(ANON_EXPRTYPE, anon_expr, parser);
    }

    if(match(parser, 1, LEFT_PAREN_TOKTYPE)){
        Token *left_paren_token = previous(parser);
        Expr *group_sub_expr = parse_expr(parser);

        GroupExpr *group_expr = MEMORY_ALLOC(GroupExpr, 1, parser->ctallocator);

        group_expr->left_paren_token = left_paren_token;
        group_expr->expr = group_sub_expr;

        consume(parser, RIGHT_PAREN_TOKTYPE, "Expect ')' after expression in group expression");

        return create_expr(GROUP_EXPRTYPE, group_expr, parser);
    }

    if(match(parser, 1, IDENTIFIER_TOKTYPE)){
        Token *identifier_token = previous(parser);

        IdentifierExpr *identifier_expr = MEMORY_ALLOC(IdentifierExpr, 1, parser->ctallocator);

        identifier_expr->identifier_token= identifier_token;

        return create_expr(IDENTIFIER_EXPRTYPE, identifier_expr,parser);
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
    if(match(parser, 2, MUT_TOKTYPE, IMUT_TOKTYPE))
        return parse_var_decl_stmt(parser);

    if(match(parser, 1, LEFT_BRACKET_TOKTYPE)){
        DynArrPtr *stmts = parse_block_stmt(parser);

        BlockStmt *block_stmt = MEMORY_ALLOC(BlockStmt, 1, parser->ctallocator);

        block_stmt->stmts = stmts;

        return create_stmt(BLOCK_STMTTYPE, block_stmt, parser);
    }

	if(match(parser, 1, IF_TOKTYPE))
		return parse_if_stmt(parser);

	if(match(parser, 1, WHILE_TOKTYPE))
		return parse_while_stmt(parser);

	if(match(parser, 1, STOP_TOKTYPE)){
		Token *stop_token = previous(parser);
		consume(parser, SEMICOLON_TOKTYPE, "Expect ';' at end of 'stop' statement.");

		StopStmt *stop_stmt = MEMORY_ALLOC(StopStmt, 1, parser->ctallocator);

		stop_stmt->stop_token = stop_token;

		return create_stmt(STOP_STMTTYPE, stop_stmt, parser);
	}

    if(match(parser, 1, CONTINUE_TOKTYPE)){
        Token *continue_token = previous(parser);
        consume(parser, SEMICOLON_TOKTYPE, "Expect ';' at end of 'continue' statement.");

        ContinueStmt *continue_stmt = MEMORY_ALLOC(ContinueStmt, 1, parser->ctallocator);

        continue_stmt->continue_token = continue_token;

        return create_stmt(CONTINUE_STMTTYPE, continue_stmt, parser);
    }

    if(match(parser, 1, RET_TOKTYPE))
        return parse_return_stmt(parser);

    if(match(parser, 1, PROC_TOKTYPE))
        return parse_function_stmt(parser);

    if(match(parser, 1, IMPORT_TOKTYPE))
        return parse_import_stmt(parser);

    if(match(parser, 1, LOAD_TOKTYPE))
        return parse_load_stmt(parser);

    if(match(parser, 1, EXPORT_TOKTYPE))
        return parse_export_stmt(parser);

    if(match(parser, 1, THROW_TOKTYPE))
        return parse_throw_stmt(parser);

    if(match(parser, 1, TRY_TOKTYPE))
        return parse_try_stmt(parser);

    return parse_expr_stmt(parser);
}

Stmt *parse_expr_stmt(Parser *parser){
    Expr *expr = parse_expr(parser);
    consume(parser, SEMICOLON_TOKTYPE, "Expect ';' at end of statement expression.");

    ExprStmt *expr_stmt = MEMORY_ALLOC(ExprStmt, 1, parser->ctallocator);

    expr_stmt->expr = expr;

    Stmt *stmt = MEMORY_ALLOC(Stmt, 1, parser->ctallocator);

    stmt->type = EXPR_STMTTYPE;
    stmt->sub_stmt = expr_stmt;

    return stmt;
}

DynArrPtr *parse_block_stmt(Parser *parser){
    DynArrPtr *stmts = FACTORY_DYNARR_PTR(parser->ctallocator);

    while (!check(parser, RIGHT_BRACKET_TOKTYPE)){
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

	if(match(parser, 1, COLON_TOKTYPE)){
		if_stmts = FACTORY_DYNARR_PTR(parser->ctallocator);
		Stmt *stmt = parse_stmt(parser);
		dynarr_ptr_insert(stmt, if_stmts);
	}else{
		consume(parser, LEFT_BRACKET_TOKTYPE, "Expect '{' at start of if body.");
		if_stmts = parse_block_stmt(parser);
	}

	if(match(parser, 1, ELSE_TOKTYPE)){
		if(match(parser, 1, COLON_TOKTYPE)){
			else_stmts = FACTORY_DYNARR_PTR(parser->ctallocator);
			Stmt *stmt = parse_stmt(parser);
			dynarr_ptr_insert(stmt, else_stmts);
		}else{
			consume(parser, LEFT_BRACKET_TOKTYPE, "Expect '{' at start of else body.");
			else_stmts = parse_block_stmt(parser);
		}
	}

	IfStmt *if_stmt = MEMORY_ALLOC(IfStmt, 1, parser->ctallocator);

    if_stmt->if_token = if_token;
	if_stmt->if_condition = if_condition;
	if_stmt->if_stmts = if_stmts;
	if_stmt->else_stmts = else_stmts;

	return create_stmt(IF_STMTTYPE, if_stmt, parser);
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

	WhileStmt *while_stmt = MEMORY_ALLOC(WhileStmt, 1, parser->ctallocator);

    while_stmt->while_token = while_token;
	while_stmt->condition = condition;
	while_stmt->stmts = stmts;

	return create_stmt(WHILE_STMTTYPE, while_stmt, parser);
}

Stmt *parse_throw_stmt(Parser *parser){
    Token *throw_token = NULL;
	Expr *value = NULL;

    throw_token = previous(parser);

	if(!check(parser, SEMICOLON_TOKTYPE))
		value = parse_expr(parser);

    consume(parser, SEMICOLON_TOKTYPE, "Expect ';' at end of throw statement.");

    ThrowStmt *throw_stmt = MEMORY_ALLOC(ThrowStmt, 1, parser->ctallocator);

    throw_stmt->throw_token = throw_token;
	throw_stmt->value = value;

    return create_stmt(THROW_STMTTYPE, throw_stmt, parser);
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

    TryStmt *try_stmt = MEMORY_ALLOC(TryStmt, 1, parser->ctallocator);

    try_stmt->try_token = try_token;
    try_stmt->try_stmts = try_stmts;
	try_stmt->catch_token = catch_token;
	try_stmt->err_identifier = err_identifier;
	try_stmt->catch_stmts = catch_stmts;

    return create_stmt(TRY_STMTTYPE, try_stmt, parser);
}

Stmt *parse_return_stmt(Parser *parser){
    Token *return_token = NULL;
    Expr *value = NULL;

    return_token = previous(parser);

    if(!check(parser, SEMICOLON_TOKTYPE))
        value = parse_expr(parser);

    consume(parser, SEMICOLON_TOKTYPE, "Expect ';' at end of return statement.");

    ReturnStmt *return_stmt = MEMORY_ALLOC(ReturnStmt, 1, parser->ctallocator);

    return_stmt->return_token = return_token;
    return_stmt->value = value;

    return create_stmt(RETURN_STMTTYPE, return_stmt, parser);
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

    VarDeclStmt *var_decl_stmt = MEMORY_ALLOC(VarDeclStmt, 1, parser->ctallocator);

    var_decl_stmt->is_const = is_const;
    var_decl_stmt->is_initialized = is_initialized;
    var_decl_stmt->identifier_token = identifier_token;
    var_decl_stmt->initializer_expr = initializer_expr;

    return create_stmt(VAR_DECL_STMTTYPE, var_decl_stmt, parser);
}

Stmt *parse_function_stmt(Parser *parser){
    Token *name_token = NULL;
    DynArrPtr *params = NULL;
    DynArrPtr *stmts = NULL;

    name_token = consume(parser, IDENTIFIER_TOKTYPE, "Expect function name after 'proc' keyword.");
    consume(parser, LEFT_PAREN_TOKTYPE, "Expect '(' after function name.");

    if(!check(parser, RIGHT_PAREN_TOKTYPE)){
        params = FACTORY_DYNARR_PTR(parser->ctallocator);

        do{
            Token *param_token = consume(parser, IDENTIFIER_TOKTYPE, "Expect function parameter name.");
            dynarr_ptr_insert(param_token, params);
        } while (match(parser, 1, COMMA_TOKTYPE));
    }

    consume(parser, RIGHT_PAREN_TOKTYPE, "Expect ')' at end of function parameters.");

    if(match(parser, 1, COLON_TOKTYPE)){
		stmts = FACTORY_DYNARR_PTR(parser->ctallocator);
		Stmt *stmt = parse_return_stmt(parser);
		dynarr_ptr_insert(stmt, stmts);
	}else{
		consume(parser, LEFT_BRACKET_TOKTYPE, "Expect '{' at start of function body.");
		stmts = parse_block_stmt(parser);
	}

    FunctionStmt *function_stmt = MEMORY_ALLOC(FunctionStmt, 1, parser->ctallocator);

    function_stmt->identifier_token = name_token;
    function_stmt->params = params;
    function_stmt->stmts = stmts;

    return create_stmt(FUNCTION_STMTTYPE, function_stmt, parser);
}

Stmt *parse_import_stmt(Parser *parser){
    Token *import_token = NULL;
    DynArr *names = NULL;
    Token *alt_name = NULL;

    import_token = previous(parser);
    names = FACTORY_DYNARR(sizeof(Token), parser->ctallocator);

    do{
        Token *name = consume(parser, IDENTIFIER_TOKTYPE, "Expect module name");
        dynarr_insert(name, names);
    } while (match(parser, 1, DOT_TOKTYPE));

    if(match(parser, 1, AS_TOKTYPE))
        alt_name = consume(parser, IDENTIFIER_TOKTYPE, "Expect module alternative name after 'as' keyword");

    consume(parser, SEMICOLON_TOKTYPE, "Expect ';' at end of import statement.");

    ImportStmt *import_stmt = MEMORY_ALLOC(ImportStmt, 1, parser->ctallocator);

    import_stmt->import_token = import_token;
    import_stmt->names = names;
    import_stmt->alt_name = alt_name;

    return create_stmt(IMPORT_STMTTYPE, import_stmt, parser);
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

    LoadStmt *load_stmt = MEMORY_ALLOC(LoadStmt, 1, parser->ctallocator);

    load_stmt->load_token = load_token;
    load_stmt->pathname = path_token;
    load_stmt->name = name_token;

    return create_stmt(LOAD_STMTTYPE, load_stmt, parser);
}

Stmt *parse_export_stmt(Parser *parser){
    Token *export_token = NULL;
    DynArrPtr *symbols = NULL;

    export_token = previous(parser);
    symbols = FACTORY_DYNARR_PTR(parser->ctallocator);

    consume(parser, LEFT_BRACKET_TOKTYPE, "Expect '{' at start of export symbols");

    do{
        Token *identifier = consume(parser, IDENTIFIER_TOKTYPE, "Expect symbol name");
        dynarr_ptr_insert(identifier, symbols);
    } while (match(parser, 1, COMMA_TOKTYPE));

    consume(parser, RIGHT_BRACKET_TOKTYPE, "Expect '}' at end of export symbols");

    ExportStmt *export_stmt = MEMORY_ALLOC(ExportStmt, 1, parser->ctallocator);

    export_stmt->export_token = export_token;
    export_stmt->symbols = symbols;

    return create_stmt(EXPORT_STMTTYPE, export_stmt, parser);
}

Parser *parser_create(Allocator *allocator){
    Parser *parser = MEMORY_ALLOC(Parser, 1, allocator);
    if(!parser){return NULL;}
	memset(parser, 0, sizeof(Parser));
    parser->ctallocator = allocator;
	return parser;
}

int parser_parse(DynArrPtr *tokens, DynArrPtr *stmts, Parser *parser){
	if(setjmp(parser->err_jmp) == 1) return 1;
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

int parser_parse_template(DynArrPtr *tokens, DynArrPtr *exprs, Parser *parser){
    if(setjmp(parser->err_jmp) == 1) return 1;
    else {
        parser->current = 0;
        parser->tokens = tokens;
        parser->stmt = exprs;

        while(!is_at_end(parser)){
            Expr *expr = parse_is_expr(parser);
            dynarr_ptr_insert(expr, exprs);
        }

        return 0;
    }
}
