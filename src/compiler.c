#include "compiler.h"
#include "memory.h"
#include "utils.h"
#include "opcode.h"
#include "token.h"
#include "stmt.h"
#include "lexer.h"
#include "parser.h"
#include <stdint.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

static void error(Compiler *compiler, Token *token, char *msg, ...){
    va_list args;
    va_start(args, msg);

    fprintf(stderr, "Compiler error at line %d in file '%s':\n\t", token->line, token->pathname);
    vfprintf(stderr, msg, args);
    fprintf(stderr, "\n");

    va_end(args);
    longjmp(compiler->err_jmp, 1);
}

void descompose_i32(int32_t value, uint8_t *bytes)
{
    uint8_t mask = 0b11111111;

    for (size_t i = 0; i < 4; i++)
    {
        uint8_t r = value & mask;
        value = value >> 8;
        bytes[i] = r;
    }
}

#define WHILE_IN(compiler) (++compiler->while_counter)
#define WHILE_OUT(compiler) (--compiler->while_counter)

static void mark_stop(size_t len, size_t index, Compiler *compiler){
	assert(compiler->stop_ptr < LOOP_MARK_LENGTH);
    LoopMark *loop_mark = &compiler->stops[compiler->stop_ptr++];
    
    loop_mark->id = compiler->while_counter;
	loop_mark->len = len;
    loop_mark->index = index;
}

static void unmark_stop(size_t which, Compiler *compiler){
	assert(which < compiler->stop_ptr);

	for(size_t i = which; i + 1 < compiler->stop_ptr; i++){
        LoopMark *a = &compiler->stops[i];
        LoopMark *b = &compiler->stops[i + 1];

		memcpy(a, b, sizeof(LoopMark));
	}

	compiler->stop_ptr--;
}

static void mark_continue(size_t len, size_t index, Compiler *compiler){
	assert(compiler->stop_ptr < LOOP_MARK_LENGTH);
    LoopMark *loop_mark = &compiler->continues[compiler->continue_ptr++];
    
    loop_mark->id = compiler->while_counter;
	loop_mark->len = len;
    loop_mark->index = index;
}

static void unmark_continue(size_t which, Compiler *compiler){
	assert(which < compiler->continue_ptr);

	for(size_t i = which; i + 1 < compiler->continue_ptr; i++){
        LoopMark *a = &compiler->stops[i];
        LoopMark *b = &compiler->stops[i + 1];

		memcpy(a, b, sizeof(LoopMark));
	}

	compiler->continue_ptr--;
}

Scope *scope_in(ScopeType type, Compiler *compiler){
    Scope *scope = &compiler->scopes[compiler->depth++];
    
    scope->depth = compiler->depth;
    scope->locals = 0;
	scope->type = type;
    scope->symbols_len = 0;
    
    return scope;
}

Scope *scope_in_soft(ScopeType type, Compiler *compiler){
    Scope *enclosing = &compiler->scopes[compiler->depth - 1];
    Scope *scope = &compiler->scopes[compiler->depth++];
    
    scope->depth = compiler->depth;
    scope->locals = enclosing->locals;
	scope->type = type;
    scope->symbols_len = 0;
    
    return scope;
}

Scope *scope_in_fn(char *name, Compiler *compiler, Fn **out_function){
    assert(compiler->fn_ptr < FUNCTIONS_LENGTH);

    Fn *fn = runtime_fn(name, compiler->module);
    compiler->fn_stack[compiler->fn_ptr++] = fn;
    
    Module *module = compiler->module;
	LZHTable *symbols = module->symbols;
	ModuleSymbol *symbol = (ModuleSymbol *)A_RUNTIME_ALLOC(sizeof(ModuleSymbol));

	symbol->type = FUNCTION_MSYMTYPE;
	symbol->value.fn = fn;
    
    lzhtable_put((uint8_t *)name, strlen(name), symbol, symbols, NULL);

    if(out_function) *out_function = fn;

    return scope_in(FUNCTION_SCOPE, compiler);
}


void scope_out(Compiler *compiler){
    compiler->depth--;
}

void scope_out_fn(Compiler *compiler){
    assert(compiler->fn_ptr > 0);
	compiler->fn_ptr--;
    scope_out(compiler);
}

Scope *inside_while(Compiler *compiler){
	assert(compiler->depth > 0);

	for(int i = (int)compiler->depth - 1; i >= 0; i--){
		Scope *scope = compiler->scopes + i;
		if(scope->type == WHILE_SCOPE) return scope;
		if(scope->type == FUNCTION_SCOPE) return NULL;
	}

	return NULL;
}

Scope *inside_function(Compiler *compiler){
	assert(compiler->depth > 0);

	for(int i = (int)compiler->depth - 1; i >= 0; i--){
		Scope *scope = compiler->scopes + i;
		if(scope->type == FUNCTION_SCOPE) return scope;
	}

	return NULL;

}

Scope *current_scope(Compiler *compiler){
    assert(compiler->depth > 0);
    return &compiler->scopes[compiler->depth - 1];
}

Symbol *exists_scope(char *name, Scope *scope, Compiler *compiler){
    for (int i = scope->symbols_len - 1; i >= 0; i--){
        Symbol *symbol = &scope->symbols[i];
        
        if(strcmp(symbol->name, name) == 0)
            return symbol;
    }
    
    return NULL;
}

Symbol *exists_local(char *name, Compiler *compiler){
    Scope *scope = current_scope(compiler);
    return exists_scope(name, scope, compiler);
}

Symbol *get(Token *identifier_token, Compiler *compiler){
    assert(compiler->depth > 0);
    char *identifier = identifier_token->lexeme;

    for (int i = compiler->depth - 1; i >= 0; i--)
    {
        Scope *scope = &compiler->scopes[i];
        Symbol *symbol = exists_scope(identifier, scope, compiler);
        if(symbol != NULL) return symbol;
    }

    error(compiler, identifier_token, "Symbol '%s' doesn't exists.", identifier);

    return NULL;
}

Symbol *declare_native(char *identifier, Compiler *compiler){
    size_t identifier_len = strlen(identifier);
    Scope *scope = current_scope(compiler);
    Symbol *symbol = &scope->symbols[scope->symbols_len++];
    
    symbol->depth = scope->depth;
    symbol->local = -1;
    symbol->type = NATIVE_FN_SYMTYPE;
    symbol->name_len = identifier_len;
    memcpy(symbol->name, identifier, identifier_len);
    symbol->name[identifier_len] = '\0';
    symbol->identifier_token = NULL;

    return symbol;
}

Symbol *declare(SymbolType type, Token *identifier_token, Compiler *compiler){
    char *identifier = identifier_token->lexeme;
    size_t identifier_len = strlen(identifier);

	if(identifier_len + 1 >= SYMBOL_NAME_LENGTH)
		error(compiler, identifier_token, "Symbol name '%s' too long.", identifier);
    if(exists_local(identifier, compiler) != NULL)
        error(compiler, identifier_token, "Already exists a symbol named as '%s'.", identifier);

    int local;
    Scope *scope = current_scope(compiler);
    Symbol *symbol = &scope->symbols[scope->symbols_len++];

	if(type == MUT_SYMTYPE || type == IMUT_SYMTYPE)
		local = scope->depth == 1 ? -1 : scope->locals++;
    
    symbol->depth = scope->depth;
    symbol->local = local;
    symbol->type = type;
    symbol->name_len = identifier_len;
    memcpy(symbol->name, identifier, identifier_len);
    symbol->name[identifier_len] = '\0';
    symbol->identifier_token = identifier_token;

    return symbol;
}

DynArr *current_chunks(Compiler *compiler){
	assert(compiler->fn_ptr > 0 && compiler->fn_ptr < FUNCTIONS_LENGTH);
	Fn *fn = compiler->fn_stack[compiler->fn_ptr - 1];

    return fn->chunks;
}

size_t chunks_len(Compiler *compiler){
	return current_chunks(compiler)->used;
}

size_t write_chunk(uint8_t chunk, Compiler *compiler){
    DynArr *chunks = current_chunks(compiler);
	size_t index = chunks->used;
    dynarr_insert(&chunk, chunks);
	return index;
}

void update_chunk(size_t index, uint8_t chunk, Compiler *compiler){
	DynArr *chunks = current_chunks(compiler);
	dynarr_set(&chunk, index, chunks);
}

size_t write_i32(int32_t i32, Compiler *compiler){
	size_t index = chunks_len(compiler);

	uint8_t bytes[4];
	descompose_i32(i32, bytes);

	for(size_t i = 0; i < 4; i++)
		write_chunk(bytes[i], compiler);

	return index;
}

void update_i32(size_t index, int32_t i32, Compiler *compiler){
	uint8_t bytes[4];
	DynArr *chunks = current_chunks(compiler);

	descompose_i32(i32, bytes);

	for(size_t i = 0; i < 4; i++)
		dynarr_set(bytes + i, index + i, chunks);
}

size_t write_i64_const(int64_t i64, Compiler *compiler){
    Module *module = compiler->module;
    DynArr *constants = module->constants;
    int32_t index = (int32_t) constants->used;

    dynarr_insert(&i64, constants);
	return write_i32(index, compiler);
}

void write_str(char *rstr, Compiler *compiler){
    uint8_t *key = (uint8_t *)rstr;
    size_t key_size = strlen(rstr);
    uint32_t hash = lzhtable_hash(key, key_size);

    Module *module = compiler->module;
    LZHTable *strings = module->strings;

    if(!lzhtable_hash_contains(hash, strings, NULL)){
        char *str = runtime_clone_str(rstr);
        lzhtable_hash_put(hash, str, strings);
    }

    write_i32((int32_t)hash, compiler);
}

void compile_expr(Expr *expr, Compiler *compiler){
    switch (expr->type){
        case EMPTY_EXPRTYPE:{
            write_chunk(EMPTY_OPCODE, compiler);
            break;
        }
        case BOOL_EXPRTYPE:{
            BoolExpr *bool_expr = (BoolExpr *)expr->sub_expr;
            uint8_t value = bool_expr->value;

            write_chunk(value ? TRUE_OPCODE : FALSE_OPCODE, compiler);

            break;
        }
        case INT_EXPRTYPE:{
            IntExpr *int_expr = (IntExpr *)expr->sub_expr;
            Token *token = int_expr->token;
            int64_t value = *(int64_t *)token->literal;

            write_chunk(INT_OPCODE, compiler);
            write_i64_const(value, compiler);

            break;
        }
		case STRING_EXPRTYPE:{
			StringExpr *string_expr = (StringExpr *)expr->sub_expr;
			uint32_t hash = string_expr->hash;

			write_chunk(STRING_OPCODE, compiler);
			write_i32((int32_t) hash, compiler);

			break;
		}
        case IDENTIFIER_EXPRTYPE:{
            IdentifierExpr *identifier_expr = (IdentifierExpr *)expr->sub_expr;
            Token *identifier_token = identifier_expr->identifier_token;

            Symbol *symbol = get(identifier_token, compiler);

            if(symbol->type == MUT_SYMTYPE || symbol->type == IMUT_SYMTYPE){
                if(symbol->depth == 1){
                    write_chunk(GGET_OPCODE, compiler);
                    write_str(identifier_token->lexeme, compiler);
                }else{
                    write_chunk(LGET_OPCODE, compiler);
                    write_chunk((uint8_t)symbol->local, compiler);
                }
            }

			if(symbol->type == NATIVE_FN_SYMTYPE){
                write_chunk(NGET_OPCODE, compiler);
                write_str(identifier_token->lexeme, compiler);
            }

			if(symbol->type == FN_SYMTYPE || symbol->type == MODULE_SYMTYPE){
                write_chunk(SGET_OPCODE, compiler);
                write_str(identifier_token->lexeme, compiler);
            }

            break;
        }
        case GROUP_EXPRTYPE:{
            GroupExpr *group_expr = (GroupExpr *)expr->sub_expr;
            Expr *expr = group_expr->expr;

            compile_expr(expr, compiler);
            
            break;
        }
        case CALL_EXPRTYPE:{
            CallExpr *call_expr = (CallExpr *)expr->sub_expr;
            Expr *left = call_expr->left;
            DynArrPtr *args = call_expr->args;

            compile_expr(left, compiler);

            if(args){
                for (size_t i = 0; i < args->used; i++){
                    Expr *expr = (Expr *)DYNARR_PTR_GET(i, args);
                    compile_expr(expr, compiler);
                }
            }

            write_chunk(CALL_OPCODE, compiler);
            
            uint8_t args_count = args ? (uint8_t)args->used : 0;
            write_chunk(args_count, compiler);

            break;
        }
        case ACCESS_EXPRTYPE:{
            AccessExpr *access_expr = (AccessExpr *)expr->sub_expr;
            Expr *left = access_expr->left;
            Token *symbol_token = access_expr->symbol_token;

            compile_expr(left, compiler);
            write_chunk(ACCESS_OPCODE, compiler);
            write_str(symbol_token->lexeme, compiler);

            break;
        }
        case UNARY_EXPRTYPE:{
            UnaryExpr *unary_expr = (UnaryExpr *)expr->sub_expr;
            Token *operator = unary_expr->operator;
            Expr *right = unary_expr->right;

            compile_expr(right, compiler);

            switch (operator->type){
                case MINUS_TOKTYPE:{
                    write_chunk(NNOT_OPCODE, compiler);
                    break;
                }
                case EXCLAMATION_TOKTYPE:{
                    write_chunk(NOT_OPCODE, compiler);
                    break;
                }
                default:{
                    assert("Illegal token type");
                }
            }

            break;
        }
        case BINARY_EXPRTYPE:{
            BinaryExpr *binary_expr = (BinaryExpr *)expr->sub_expr;
            Expr *left = binary_expr->left;
            Token *operator = binary_expr->operator;
            Expr *right = binary_expr->right;

            compile_expr(left, compiler);
            compile_expr(right, compiler);

            switch (operator->type){
                case PLUS_TOKTYPE:{
                    write_chunk(ADD_OPCODE, compiler);
                    break;
                }
                case MINUS_TOKTYPE:{
                    write_chunk(SUB_OPCODE, compiler);
                    break;
                }
                case ASTERISK_TOKTYPE:{
                    write_chunk(MUL_OPCODE, compiler);
                    break;
                }
                case SLASH_TOKTYPE:{
                    write_chunk(DIV_OPCODE, compiler);
                    break;
                }
				case MOD_TOKTYPE:{
					write_chunk(MOD_OPCODE, compiler);
					break;
				}
                default:{
                    assert("Illegal token type");
                }
            }

            break;
        }
        case COMPARISON_EXPRTYPE:{
            ComparisonExpr *comparison_expr = (ComparisonExpr *)expr->sub_expr;
            Expr *left = comparison_expr->left;
            Token *operator = comparison_expr->operator;
            Expr *right = comparison_expr->right;

            compile_expr(left, compiler);
            compile_expr(right, compiler);

            switch (operator->type){
                case LESS_TOKTYPE:{
                    write_chunk(LT_OPCODE, compiler);
                    break;
                }
                case GREATER_TOKTYPE:{
                    write_chunk(GT_OPCODE, compiler);
                    break;
                }
                case LESS_EQUALS_TOKTYPE:{
                    write_chunk(LE_OPCODE, compiler);
                    break;
                }
                case GREATER_EQUALS_TOKTYPE:{
                    write_chunk(GE_OPCODE, compiler);
                    break;
                }
                case EQUALS_EQUALS_TOKTYPE:{
                    write_chunk(EQ_OPCODE, compiler);
                    break;
                }
                case NOT_EQUALS_TOKTYPE:{
                    write_chunk(NE_OPCODE, compiler);
                    break;
                }
                default:{
                    assert("Illegal token type");
                }
            }

            break;
        }
        case LOGICAL_EXPRTYPE:{
            LogicalExpr *logical_expr = (LogicalExpr *)expr->sub_expr;
            Expr *left = logical_expr->left;
            Token *operator = logical_expr->operator;
            Expr *right = logical_expr->right;

            compile_expr(left, compiler);
            compile_expr(right, compiler);

            switch (operator->type){
                case OR_TOKTYPE:{
                    write_chunk(OR_OPCODE, compiler);
                    break;
                }
                case AND_TOKTYPE:{
                    write_chunk(AND_OPCODE, compiler);
                    break;
                }
                default:{
                    assert("Illegal token type");
                }
            }

            break;
        }
        case ASSIGN_EXPRTYPE:{
            AssignExpr *assign_expr = (AssignExpr *)expr->sub_expr;
            Token *identifier_token = assign_expr->identifier_token;
            Expr *value_expr = assign_expr->value_expr;

            Symbol *symbol = get(identifier_token, compiler);
            
            if(symbol->type == IMUT_SYMTYPE)
                error(compiler, identifier_token, "'%s' declared as constant. Can't change its value.", identifier_token->lexeme);
            
            compile_expr(value_expr, compiler);

            if(symbol->depth == 1){
                write_chunk(GSET_OPCODE, compiler);
                write_str(identifier_token->lexeme, compiler);
            }else{
                write_chunk(LSET_OPCODE, compiler);
                write_chunk(symbol->local, compiler);
            }

            break;
        }
		case COMPOUND_EXPRTYPE:{
			CompoundExpr *compound_expr = (CompoundExpr *)expr->sub_expr;
			Token *identifier_token = compound_expr->identifier_token;
			Token *operator = compound_expr->operator;
			Expr *right = compound_expr->right;

			Symbol *symbol = get(identifier_token, compiler);

			if(symbol->type == IMUT_SYMTYPE)
				error(compiler, identifier_token, "'%s' declared as constant. Can't change its value.", identifier_token->lexeme);
			
			if(symbol->depth == 1){
                write_chunk(GGET_OPCODE, compiler);
			    write_str(identifier_token->lexeme, compiler);
            }else{
                write_chunk(LGET_OPCODE, compiler);
			    write_chunk(symbol->local, compiler);
            }

			compile_expr(right, compiler);			

			switch(operator->type){
				case COMPOUND_ADD_TOKTYPE:{
					write_chunk(ADD_OPCODE, compiler);
					break;
				}
				case COMPOUND_SUB_TOKTYPE:{
					write_chunk(SUB_OPCODE, compiler);
					break;
				}
				case COMPOUND_MUL_TOKTYPE:{
					write_chunk(MUL_OPCODE, compiler);
					break;
				}
				case COMPOUND_DIV_TOKTYPE:{
					write_chunk(DIV_OPCODE, compiler);
					break;
				}
				default:{
					assert("Illegal compound type");
				}
			}

			if(symbol->depth == 1){
                write_chunk(GSET_OPCODE, compiler);
			    write_str(identifier_token->lexeme, compiler);
            }else{
                write_chunk(LSET_OPCODE, compiler);
			    write_chunk(symbol->local, compiler);
            }
	
			break;
		}
		case LIST_EXPRTYPE:{
			ListExpr *list_expr = (ListExpr *)expr->sub_expr;
			Token *list_token = list_expr->list_token;
			DynArrPtr *exprs = list_expr->exprs;

            if(exprs){
				for(int i = exprs->used - 1; i >= 0; i--){
					Expr *expr = (Expr *)DYNARR_PTR_GET((size_t)i, exprs);
					compile_expr(expr, compiler);
				}
			}

            size_t len = exprs ? (int32_t)exprs->used : 0;
			
            write_chunk(LIST_OPCODE, compiler);
            write_i32(len, compiler);

			break;
		}
        case DICT_EXPRTYPE:{
            DictExpr *dict_expr = (DictExpr *)expr->sub_expr;
            DynArrPtr *key_values = dict_expr->key_values;

            if(key_values){
                for (int i = key_values->used - 1; i >= 0; i--){
                    DictKeyValue *key_value = (DictKeyValue *)DYNARR_PTR_GET(i, key_values);
                    Expr *key = key_value->key;
                    Expr *value = key_value->value;

                    compile_expr(key, compiler);
                    compile_expr(value, compiler);
                }
            }

            int32_t len = (int32_t)(key_values ? key_values->used : 0);
            
            write_chunk(DICT_OPCODE, compiler);
            write_i32(len, compiler);

            break;
        }
        default:{
            assert("Illegal expression type");
        }
    }
}

void add_module_symbol(char *key, size_t key_size, Module *module, LZHTable *symbols){
	ModuleSymbol *symbol = (ModuleSymbol *)A_RUNTIME_ALLOC(sizeof(ModuleSymbol));
	symbol->type = MODULE_MSYMTYPE;
	symbol->value.module = module;
    lzhtable_put((uint8_t *)key, key_size, symbol, symbols, NULL);
}

void compile_stmt(Stmt *stmt, Compiler *compiler){
    switch (stmt->type){
        case EXPR_STMTTYPE:{
            ExprStmt *expr_stmt = (ExprStmt *)stmt->sub_stmt;
            Expr *expr = expr_stmt->expr;

            compile_expr(expr, compiler);
            write_chunk(POP_OPCODE, compiler);

            break;
        }
        case PRINT_STMTTYPE:{
            PrintStmt *print_stmt = (PrintStmt *)stmt->sub_stmt;
            Expr *expr = print_stmt->expr;

            compile_expr(expr, compiler);
            write_chunk(PRT_OPCODE, compiler);

            break;
        }
        case VAR_DECL_STMTTYPE:{
            VarDeclStmt *var_decl_stmt = (VarDeclStmt *)stmt->sub_stmt;
            char is_const = var_decl_stmt->is_const;
            char is_initialized = var_decl_stmt->is_initialized;
            Token *identifier_token = var_decl_stmt->identifier_token;
            Expr *initializer_expr = var_decl_stmt->initializer_expr;

            if(is_const && !is_initialized)
                error(compiler, identifier_token, "'%s' declared as constant, but is not being initialized.", identifier_token->lexeme);

            Symbol *symbol = declare(is_const ? IMUT_SYMTYPE : MUT_SYMTYPE, identifier_token, compiler);
            
            if(initializer_expr == NULL) write_chunk(EMPTY_OPCODE, compiler);
            else compile_expr(initializer_expr, compiler);

            if(symbol->depth == 1){
                write_chunk(GSET_OPCODE, compiler);
                write_str(identifier_token->lexeme, compiler);
            }else{
                write_chunk(LSET_OPCODE, compiler);
                write_chunk((uint8_t)symbol->local, compiler);
            }

            write_chunk(POP_OPCODE, compiler);

            break;
        }
        case BLOCK_STMTTYPE:{
            BlockStmt *block_stmt = (BlockStmt *)stmt->sub_stmt;
            DynArrPtr *stmts = block_stmt->stmts;

            scope_in_soft(BLOCK_SCOPE, compiler);

            for (size_t i = 0; i < stmts->used; i++)
            {
                Stmt *stmt = (Stmt *)DYNARR_PTR_GET(i, stmts);
                compile_stmt(stmt, compiler);
            }

            scope_out(compiler);
            
            break;
        }
		case IF_STMTTYPE:{
			IfStmt *if_stmt = (IfStmt *)stmt->sub_stmt;
			Expr *if_condition = if_stmt->if_condition;
			DynArrPtr *if_stmts = if_stmt->if_stmts;
			DynArrPtr *else_stmts = if_stmt->else_stmts;

			compile_expr(if_condition, compiler);
			
			write_chunk(JIF_OPCODE, compiler);
			size_t jif_index = write_i32(0, compiler);
			
			//> if branch body
			size_t len_bef_if = chunks_len(compiler);
			scope_in_soft(BLOCK_SCOPE, compiler);

			for(size_t i = 0; i < if_stmts->used; i++){
				Stmt *stmt = (Stmt *)DYNARR_PTR_GET(i, if_stmts);
				compile_stmt(stmt, compiler);
			}

			scope_out(compiler);
            
            write_chunk(JMP_OPCODE, compiler);
            size_t jmp_index = write_i32(0, compiler);

			size_t len_af_if = chunks_len(compiler);
			size_t if_len = len_af_if - len_bef_if;
			//< if branch body
			
			update_i32(jif_index, (int32_t)if_len + 1, compiler);

            if(else_stmts){
				//> else body
                size_t len_bef_else = chunks_len(compiler);
                scope_in_soft(BLOCK_SCOPE, compiler);

                for(size_t i = 0; i < else_stmts->used; i++){
                    Stmt *stmt = (Stmt *)DYNARR_PTR_GET(i, else_stmts);
                    compile_stmt(stmt, compiler);
                }

			    scope_out(compiler);
                size_t len_af_else = chunks_len(compiler);
                size_t else_len = len_af_else - len_bef_else;
				//< else body

				update_i32(jmp_index, (int32_t)else_len + 1, compiler);
            }
            
			break;
		}
		case WHILE_STMTTYPE:{
			WhileStmt *while_stmt = (WhileStmt *)stmt->sub_stmt;
			Expr *condition = while_stmt->condition;
			DynArrPtr *stmts = while_stmt->stmts;

			write_chunk(JMP_OPCODE, compiler);
			size_t jmp_index = write_i32(0, compiler);

			size_t len_bef_body = chunks_len(compiler);

			scope_in_soft(WHILE_SCOPE, compiler);
			char while_id = WHILE_IN(compiler);

			for(size_t i = 0; i < stmts->used; i++){
				Stmt *stmt = (Stmt *)DYNARR_PTR_GET(i, stmts);
				compile_stmt(stmt, compiler);
			}

			WHILE_OUT(compiler);
			scope_out(compiler);

            size_t len_af_body = chunks_len(compiler);
			size_t body_len = len_af_body - len_bef_body;

            update_i32(jmp_index, (int32_t)body_len + 1, compiler);

			compile_expr(condition, compiler);
            size_t len_af_while = chunks_len(compiler);
			size_t while_len = len_af_while - len_bef_body;

			write_chunk(JIT_OPCODE, compiler);
			write_i32(-((int32_t)while_len), compiler);

			size_t af_header = chunks_len(compiler);
			size_t ptr = 0;

			while (compiler->stop_ptr > 0 && ptr < compiler->stop_ptr){
				LoopMark *loop_mark = &compiler->stops[ptr];

				if(loop_mark->id != while_id){
					ptr++;
					continue;
				}

				update_i32(loop_mark->index, af_header - loop_mark->len + 1, compiler);
				unmark_stop(ptr, compiler);
			}

            ptr = 0;

            while (compiler->continue_ptr > 0 && ptr < compiler->continue_ptr){
				LoopMark *loop_mark = &compiler->continues[ptr];

				if(loop_mark->id != while_id){
					ptr++;
					continue;
				}

				update_i32(loop_mark->index, len_af_body - loop_mark->len + 1, compiler);
                unmark_continue(ptr, compiler);
			}
		
			break;
		}
		case STOP_STMTTYPE:{
			StopStmt *stop_stmt = (StopStmt *)stmt->sub_stmt;
			Token *stop_token = stop_stmt->stop_token;

			if(!inside_while(compiler))
				error(compiler, stop_token, "Can't use 'stop' statement outside loops.");

			write_chunk(JMP_OPCODE, compiler);
			size_t index = write_i32(0, compiler);

            size_t len = chunks_len(compiler);
			mark_stop(len, index, compiler);

			break;
		}
        case CONTINUE_STMTTYPE:{
            ContinueStmt *continue_stmt = (ContinueStmt *)stmt->sub_stmt;
            Token *continue_token = continue_stmt->continue_token;

            if(!inside_while(compiler))
                error(compiler, continue_token, "Can't use 'continue' statement outside loops.");

            write_chunk(JMP_OPCODE, compiler);
            size_t index = write_i32(0, compiler);

            size_t len = chunks_len(compiler);
            mark_continue(len, index, compiler);

            break;
        }
        case RETURN_STMTTYPE:{
            ReturnStmt *return_stmt = (ReturnStmt *)stmt->sub_stmt;
			Token *return_token = return_stmt->return_token;
            Expr *value = return_stmt->value;

			Scope *scope = inside_function(compiler);
			if(!scope || scope->depth == 1)
				error(compiler, return_token, "Can't use 'return' statement outside functions.");

            if(value) compile_expr(value, compiler);
            else write_chunk(EMPTY_OPCODE, compiler);

            write_chunk(RET_OPCODE, compiler);

            break;
        }
        case FUNCTION_STMTTYPE:{
            FunctionStmt *function_stmt = (FunctionStmt *)stmt->sub_stmt;
            Token *name_token = function_stmt->name_token;
            DynArrPtr *params = function_stmt->params;
            DynArrPtr *stmts = function_stmt->stmts;

            Scope *previous_scope = inside_function(compiler);
            
            if(previous_scope && previous_scope->depth > 1)
                error(compiler, name_token, "Can not declare a function inside another function.");

            Fn *fn = NULL;

            declare(FN_SYMTYPE, name_token, compiler);
            scope_in_fn(name_token->lexeme, compiler, &fn);

            if(params){
                for (size_t i = 0; i < params->used; i++){
                    Token *param_token = (Token *)DYNARR_PTR_GET(i, params);
                    declare(MUT_SYMTYPE, param_token, compiler);
                    
                    char *name = runtime_clone_str(param_token->lexeme);
                    dynarr_ptr_insert(name, fn->params);
                }
            }

            char returned = 0;

            for (size_t i = 0; i < stmts->used; i++){
                Stmt *stmt = (Stmt *)DYNARR_PTR_GET(i, stmts);
                compile_stmt(stmt, compiler);
                
                if(i + 1 >= stmts->used && stmt->type == RETURN_STMTTYPE)
                    returned = 1;
            }

            if(!returned){
                write_chunk(EMPTY_OPCODE, compiler);
                write_chunk(RET_OPCODE, compiler);
            }
            
            scope_out_fn(compiler);

            break;
        }
        case IMPORT_STMTTYPE:{
            ImportStmt *import_stmt = (ImportStmt *)stmt->sub_stmt;
            Token *import_token = import_stmt->import_token;
            Token *path_token = import_stmt->path_token;
            Token *name_token = import_stmt->name_token;

            LZHTable *modules = compiler->modules;
            Module *current_module = compiler->module;
            LZHTable *current_strings = current_module->strings;
            LZHTable *current_symbols = current_module->symbols;
            
            uint32_t module_path_hash = *(uint32_t *) path_token->literal;
            char *module_name = name_token->lexeme;
            size_t module_name_size = strlen(module_name);
            char *module_path = (char *)lzhtable_hash_get(module_path_hash, current_strings);
            char *clone_module_path = compile_clone_str(module_path);
            size_t module_path_size = strlen(clone_module_path);

            if(compiler->previos_module){
                if(strcmp(clone_module_path, current_module->filepath) == 0)
				    error(compiler, import_token, "You are trying to import module '%s' from the module '%s'.", clone_module_path, current_module->filepath);

                Module *previous_module = compiler->previos_module;
                
                if(strcmp(clone_module_path, previous_module->filepath) == 0)
                    error(compiler, import_token, "Module '%s' import this module '%s', but this module also import '%s'.", previous_module->filepath, clone_module_path, previous_module->filepath);
            }

            if(!UTILS_EXISTS_FILE(clone_module_path))
                error(compiler, import_token, "File at '%s' do not exists.\n", clone_module_path);

            if(!utils_is_reg(clone_module_path))
                error(compiler, import_token, "File at '%s' is not a regular file.\n", clone_module_path);

            LZHTableNode *node = NULL;

            if(lzhtable_contains((uint8_t *)clone_module_path, module_path_size, modules, &node)){
                Module *imported_module = (Module *)node->value;
                add_module_symbol(module_name, module_name_size, imported_module, current_symbols);
                declare(MODULE_SYMTYPE, name_token, compiler);
                break;
            }

            RawStr *source = utils_read_source(clone_module_path);
            
            LZHTable *keywords = compiler->keywords;
            LZHTable *natives = compiler->natives;

            DynArrPtr *tokens = compile_dynarr_ptr();
            DynArrPtr *stmts = compile_dynarr_ptr();
            
            Module *module = runtime_module(module_name, clone_module_path);

            Lexer *lexer = lexer_create();
	        Parser *parser = parser_create();
            Compiler *import_compiler = compiler_create();

            if(lexer_scan(source, tokens, module->strings, keywords, clone_module_path, lexer)){
				compiler->is_err = 1;
				break;
			}

            if(parser_parse(tokens, stmts, parser)){
				compiler->is_err = 1;
				break;
			}

            if(compiler_import(
                keywords,
                natives,
                stmts,
                current_module,
                module,
                modules,
                import_compiler
            )){
                compiler->is_err = 1;
				break;
            }

            add_module_symbol(module_name, module_name_size, module, current_symbols);
            lzhtable_put((uint8_t *)clone_module_path, module_path_size, module, modules, NULL);
            declare(MODULE_SYMTYPE, name_token, compiler);
            
            break;
        }
        default:{
            assert("Illegal stmt type");
            break;
        }
    }
}

void define_natives(Compiler *compiler){
    assert(compiler->depth > 0);
    LZHTableNode *node = compiler->natives->head;
    
    while (node){
        LZHTableNode *next = node->next_table_node;
        NativeFn *native = (NativeFn *)node->value;
        declare_native(native->name, compiler);
        node = next;
    }
}

Compiler *compiler_create(){
    Compiler *compiler = (Compiler *)A_COMPILE_ALLOC(sizeof(Compiler));
    memset(compiler, 0, sizeof(Compiler));
    return compiler;
}

int compiler_compile(
    LZHTable *keywords,
    LZHTable *natives,
    DynArrPtr *stmts,
    Module *module,
    LZHTable *modules,
    Compiler *compiler
){
    memset(compiler, 0, sizeof(Compiler));
    
    if(setjmp(compiler->err_jmp) == 1) return 1;
    else{
        compiler->symbols = 1;
        compiler->keywords = keywords;
        compiler->natives = natives;
        compiler->module = module;
        compiler->modules = modules;
        compiler->stmts = stmts;

        scope_in_fn("main", compiler, NULL);
        define_natives(compiler);

        for (size_t i = 0; i < stmts->used; i++){
            Stmt *stmt = (Stmt *)DYNARR_PTR_GET(i, stmts);
            compile_stmt(stmt, compiler);
        }

        scope_out_fn(compiler);
        
        return compiler->is_err;
    }
}

int compiler_import(
    LZHTable *keywords,
    LZHTable *natives,
    DynArrPtr *stmts,
    Module *previous_module,
    Module *module,
    LZHTable *modules,
    Compiler *compiler
){
    memset(compiler, 0, sizeof(Compiler));
    
    if(setjmp(compiler->err_jmp) == 1) return 1;
    else{
        compiler->keywords = keywords;
        compiler->natives = natives;
        compiler->stmts = stmts;
        compiler->previos_module = previous_module;
        compiler->module = module;
        compiler->modules = modules;

        scope_in_fn("import", compiler, NULL);
        define_natives(compiler);

        for (size_t i = 0; i < stmts->used; i++){
            Stmt *stmt = (Stmt *)DYNARR_PTR_GET(i, stmts);
            compile_stmt(stmt, compiler);
        }

		scope_out(compiler);
        
        return compiler->is_err;
    }
}
