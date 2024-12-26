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

void descompose_i16(int16_t value, uint8_t *bytes){
    uint8_t mask = 0b11111111;

    for (size_t i = 0; i < 2; i++){
        uint8_t r = value & mask;
        value = value >> 8;
        bytes[i] = r;
    }
}

void descompose_i32(int32_t value, uint8_t *bytes){
    uint8_t mask = 0b11111111;

    for (size_t i = 0; i < 4; i++){
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
    scope->try = NULL;
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

    Fn *fn = runtime_fn(name, compiler->current_module);
    compiler->fn_stack[compiler->fn_ptr++] = fn;
    
    Module *module = compiler->current_module;
    SubModule *submodule = module->submodule;
	LZHTable *symbols = submodule->symbols;
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
		if(scope->type == FUNCTION_SCOPE){
            if(i == 0) return NULL;
            return scope;
        }
	}

	return NULL;

}

Scope *current_scope(Compiler *compiler){
    assert(compiler->depth > 0);
    return &compiler->scopes[compiler->depth - 1];
}

Scope *previous_scope(Scope *scope, Compiler *compiler){
    if(scope->depth == 1) return NULL;
    return &compiler->scopes[scope->depth - 1];
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

    int local = 0;
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

DynArr *current_locations(Compiler *compiler){
	assert(compiler->fn_ptr > 0 && compiler->fn_ptr < FUNCTIONS_LENGTH);
	Fn *fn = compiler->fn_stack[compiler->fn_ptr - 1];

    return fn->locations;
}

DynArr *current_constants(Compiler *compiler){
    assert(compiler->fn_ptr > 0 && compiler->fn_ptr < FUNCTIONS_LENGTH);
    Fn *fn = compiler->fn_stack[compiler->fn_ptr - 1];
    return fn->constants;
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

void write_location(Token *token, Compiler *compiler){
	DynArr *chunks = current_chunks(compiler);
	DynArr *locations = current_locations(compiler);
	OPCodeLocation location = {0};
	
	location.offset = chunks->used - 1;
	location.line = token->line;
    location.filepath = runtime_clone_str(token->pathname);

	dynarr_insert(&location, locations);
}

void update_chunk(size_t index, uint8_t chunk, Compiler *compiler){
	DynArr *chunks = current_chunks(compiler);
    DYNARR_SET(&chunk, index, chunks);
}

size_t write_i16(int16_t i16, Compiler *compiler){
    size_t index = chunks_len(compiler);

	uint8_t bytes[2];
	descompose_i16(i16, bytes);

	for(size_t i = 0; i < 2; i++)
		write_chunk(bytes[i], compiler);

	return index;
}

size_t write_i32(int32_t i32, Compiler *compiler){
	size_t index = chunks_len(compiler);

	uint8_t bytes[4];
	descompose_i32(i32, bytes);

	for(size_t i = 0; i < 4; i++)
		write_chunk(bytes[i], compiler);

	return index;
}

void update_i16(size_t index, int32_t i16, Compiler *compiler){
    uint8_t bytes[2];
	DynArr *chunks = current_chunks(compiler);

	descompose_i16(i16, bytes);

	for(size_t i = 0; i < 2; i++)
        DYNARR_SET(bytes + i, index + i, chunks);
}

void update_i32(size_t index, int32_t i32, Compiler *compiler){
	uint8_t bytes[4];
	DynArr *chunks = current_chunks(compiler);

	descompose_i32(i32, bytes);

	for(size_t i = 0; i < 4; i++)
        DYNARR_SET(bytes + i, index + i, chunks);
}

size_t write_i64_const(int64_t i64, Compiler *compiler){
    DynArr *constants = current_constants(compiler);
    int32_t used = (int32_t) constants->used;

    assert(used < 32767 && "Too many constants");

    dynarr_insert(&i64, constants);

	return write_i16(used, compiler);
}

void write_str(char *rstr, Compiler *compiler){
    uint8_t *key = (uint8_t *)rstr;
    size_t key_size = strlen(rstr);
    uint32_t hash = lzhtable_hash(key, key_size);

    Module *module = compiler->current_module;
    SubModule *submodule = module->submodule;
    LZHTable *strings = submodule->strings;

    if(!lzhtable_hash_contains(hash, strings, NULL)){
        char *str = runtime_clone_str(rstr);
        lzhtable_hash_put(hash, str, strings);
    }

    write_i32((int32_t)hash, compiler);
}

void compile_expr(Expr *expr, Compiler *compiler){
    switch (expr->type){
        case EMPTY_EXPRTYPE:{
			EmptyExpr *empty_expr = (EmptyExpr *)expr->sub_expr;
            write_chunk(EMPTY_OPCODE, compiler);
			write_location(empty_expr->empty_token, compiler);
            break;
        }
        case BOOL_EXPRTYPE:{
            BoolExpr *bool_expr = (BoolExpr *)expr->sub_expr;
            uint8_t value = bool_expr->value;

            write_chunk(value ? TRUE_OPCODE : FALSE_OPCODE, compiler);
			write_location(bool_expr->bool_token, compiler);

            break;
        }
        case INT_EXPRTYPE:{
            IntExpr *int_expr = (IntExpr *)expr->sub_expr;
            Token *token = int_expr->token;
            int64_t value = *(int64_t *)token->literal;

            if((value >= -128 && value <= 127) ||
            (value >= 0 && value <= 255)){
                write_chunk(CINT_OPCODE, compiler);
                write_location(token, compiler);
                
                write_chunk((uint8_t)value, compiler);

                break;
            }

            write_chunk(INT_OPCODE, compiler);
			write_location(token, compiler);

            write_i64_const(value, compiler);

            break;
        }
		case STRING_EXPRTYPE:{
			StringExpr *string_expr = (StringExpr *)expr->sub_expr;
			uint32_t hash = string_expr->hash;

			write_chunk(STRING_OPCODE, compiler);
			write_location(string_expr->string_token, compiler);

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
					write_location(identifier_token, compiler);

                    write_str(identifier_token->lexeme, compiler);
                }else{
                    write_chunk(LGET_OPCODE, compiler);
					write_location(identifier_token, compiler);

                    write_chunk((uint8_t)symbol->local, compiler);
                }
            }

			if(symbol->type == NATIVE_FN_SYMTYPE){
                write_chunk(NGET_OPCODE, compiler);
				write_location(identifier_token, compiler);
           
		        write_str(identifier_token->lexeme, compiler);
            }

			if(symbol->type == FN_SYMTYPE || symbol->type == MODULE_SYMTYPE){
                write_chunk(SGET_OPCODE, compiler);
          		write_location(identifier_token, compiler);
			
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
			write_location(call_expr->left_paren, compiler);
            
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
			write_location(access_expr->dot_token, compiler);

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

			write_location(operator, compiler);

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

			write_location(operator, compiler);

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

			write_location(operator, compiler);

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

			write_location(operator, compiler);

            break;
        }
        case ASSIGN_EXPRTYPE:{
            AssignExpr *assign_expr = (AssignExpr *)expr->sub_expr;
			Expr *left = assign_expr->left;
			Token *equals_token = assign_expr->equals_token;
            Expr *value_expr = assign_expr->value_expr;

			if(left->type == IDENTIFIER_EXPRTYPE){
				IdentifierExpr *identifier_expr = (IdentifierExpr *)left->sub_expr;
				Token *identifier_token = identifier_expr->identifier_token;
	            Symbol *symbol = get(identifier_token, compiler);
            
    	        if(symbol->type == IMUT_SYMTYPE)
        	        error(compiler, identifier_token, "'%s' declared as constant. Can't change its value.", identifier_token->lexeme);
            
            	compile_expr(value_expr, compiler);

	            if(symbol->depth == 1){
                	write_chunk(GSET_OPCODE, compiler);
   					write_location(equals_token, compiler);

                	write_str(identifier_token->lexeme, compiler);
            	}else{
                	write_chunk(LSET_OPCODE, compiler);
					write_location(equals_token, compiler);

                	write_chunk(symbol->local, compiler);
            	}

				break;
			}

			if(left->type == ACCESS_EXPRTYPE){
				AccessExpr *access_expr = (AccessExpr *)left->sub_expr;
				Expr *left = access_expr->left;
				Token *symbol_token = access_expr->symbol_token;
	
				compile_expr(value_expr, compiler);
				compile_expr(left, compiler);
				
				write_chunk(PUT_OPCODE, compiler);
				write_location(equals_token, compiler);

				write_str(symbol_token->lexeme, compiler);

				break;
			}

			error(compiler, equals_token, "Illegal assign target");

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

			write_location(operator, compiler);

			if(symbol->depth == 1){
                write_chunk(GSET_OPCODE, compiler);
				write_location(identifier_token, compiler);

			    write_str(identifier_token->lexeme, compiler);
            }else{
                write_chunk(LSET_OPCODE, compiler);
				write_location(identifier_token, compiler);

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
			write_location(list_token, compiler);

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
			write_location(dict_expr->dict_token, compiler);

            write_i32(len, compiler);

            break;
        }
		case RECORD_EXPRTYPE:{
			RecordExpr *record_expr = (RecordExpr *)expr->sub_expr;
			DynArrPtr *key_values = record_expr->key_values;

			if(key_values){
				for(int i = (int)(key_values->used - 1); i >= 0; i--){
					RecordExprValue *key_value = (RecordExprValue *)DYNARR_PTR_GET(i, key_values);
					compile_expr(key_value->value, compiler);
				}
			}

			write_chunk(RECORD_OPCODE, compiler);
			write_location(record_expr->record_token, compiler);

			write_chunk((uint8_t)(key_values ? key_values->used : 0), compiler);

			if(key_values){
				for(size_t i = 0; i < key_values->used; i++){
					RecordExprValue *key_value = (RecordExprValue *)DYNARR_PTR_GET(i, key_values);
					write_str(key_value->key->lexeme, compiler);
				}
			}

			break;
		}
		case IS_EXPRTYPE:{
			IsExpr *is_expr = (IsExpr *)expr->sub_expr;
			Expr *left_expr = is_expr->left_expr;
			Token *is_token = is_expr->is_token;
			Token *type_token = is_expr->type_token;

			compile_expr(left_expr, compiler);

			write_chunk(IS_OPCODE, compiler);
			write_location(is_token, compiler);

			switch(type_token->type){
				case EMPTY_TOKTYPE:{
					write_chunk(0, compiler);
					break;
				}case BOOL_TOKTYPE:{
					write_chunk(1, compiler);
					break;
				}case INT_TOKTYPE:{
					write_chunk(2, compiler);
					break;
				}case STR_TOKTYPE:{
					write_chunk(3, compiler);
					break;
				}case LIST_TOKTYPE:{
					write_chunk(4, compiler);
					break;
				}case DICT_TOKTYPE:{
					write_chunk(5, compiler);
					break;
				}case RECORD_TOKTYPE:{
                    write_chunk(6, compiler);
					break;
                } default:{
					assert("Illegal type value");
				}
			}

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
            write_location(print_stmt->print_token, compiler);

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
				write_location(identifier_token, compiler);

                write_str(identifier_token->lexeme, compiler);
            }else{
                write_chunk(LSET_OPCODE, compiler);
				write_location(identifier_token, compiler);

                write_chunk((uint8_t)symbol->local, compiler);
            }

            write_chunk(POP_OPCODE, compiler);
			write_location(identifier_token, compiler);

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
            write_location(if_stmt->if_token, compiler);
			size_t jif_index = write_i16(0, compiler);
			
			//> if branch body
			size_t len_bef_if = chunks_len(compiler);
			scope_in_soft(BLOCK_SCOPE, compiler);

			for(size_t i = 0; i < if_stmts->used; i++){
				Stmt *stmt = (Stmt *)DYNARR_PTR_GET(i, if_stmts);
				compile_stmt(stmt, compiler);
			}

			scope_out(compiler);
            
            write_chunk(JMP_OPCODE, compiler);
            write_location(if_stmt->if_token, compiler);
            size_t jmp_index = write_i16(0, compiler);

			size_t len_af_if = chunks_len(compiler);
			size_t if_len = len_af_if - len_bef_if;
			//< if branch body
			
			update_i16(jif_index, (int32_t)if_len + 1, compiler);

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

				update_i16(jmp_index, (int16_t)else_len + 1, compiler);
            }
            
			break;
		}
		case WHILE_STMTTYPE:{
			WhileStmt *while_stmt = (WhileStmt *)stmt->sub_stmt;
			Expr *condition = while_stmt->condition;
			DynArrPtr *stmts = while_stmt->stmts;

			write_chunk(JMP_OPCODE, compiler);
            write_location(while_stmt->while_token, compiler);
			size_t jmp_index = write_i16(0, compiler);

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

            update_i16(jmp_index, (int32_t)body_len + 1, compiler);

			compile_expr(condition, compiler);
            size_t len_af_while = chunks_len(compiler);
			size_t while_len = len_af_while - len_bef_body;

			write_chunk(JIT_OPCODE, compiler);
            write_location(while_stmt->while_token, compiler);
			write_i16(-((int16_t)while_len), compiler);

			size_t af_header = chunks_len(compiler);
			size_t ptr = 0;

			while (compiler->stop_ptr > 0 && ptr < compiler->stop_ptr){
				LoopMark *loop_mark = &compiler->stops[ptr];

				if(loop_mark->id != while_id){
					ptr++;
					continue;
				}

				update_i16(loop_mark->index, af_header - loop_mark->len + 1, compiler);
				unmark_stop(ptr, compiler);
			}

            ptr = 0;

            while (compiler->continue_ptr > 0 && ptr < compiler->continue_ptr){
				LoopMark *loop_mark = &compiler->continues[ptr];

				if(loop_mark->id != while_id){
					ptr++;
					continue;
				}

				update_i16(loop_mark->index, len_af_body - loop_mark->len + 1, compiler);
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
            write_location(stop_token, compiler);
			size_t index = write_i16(0, compiler);

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
            write_location(continue_token, compiler);
            size_t index = write_i16(0, compiler);

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
            else {
                write_chunk(EMPTY_OPCODE, compiler);
                write_location(return_token, compiler);
            }

            write_chunk(RET_OPCODE, compiler);
            write_location(return_token, compiler);

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
                write_location(name_token, compiler);

                write_chunk(RET_OPCODE, compiler);
                write_location(name_token, compiler);
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
            Module *current_module = compiler->current_module;
            Module *previous_module = compiler->previous_module;
            LZHTable *current_symbols = MODULE_SYMBOLS(current_module);
            LZHTable *current_strings = MODULE_STRINGS(current_module);
            
            char *module_name = name_token->lexeme;
            size_t module_name_size = strlen(module_name);
            uint32_t module_path_hash = *(uint32_t *) path_token->literal;
            char *module_path = (char *)lzhtable_hash_get(module_path_hash, current_strings);

            char *clone_module_path = compile_clone_str(module_path);
            size_t module_path_size = strlen(clone_module_path);

            // detect self module import
            if(strcmp(module_path, current_module->filepath) == 0)
                error(compiler, import_token, "Trying to import the current module '%s'", module_path);

            if(previous_module){
                // detect circular dependency
                if(strcmp(module_path, previous_module->filepath) == 0)
                    error(compiler, import_token, "Circular dependency between '%s' and '%s'", current_module->filepath, previous_module->filepath);
            }

            if(!UTILS_EXISTS_FILE(clone_module_path))
                error(compiler, import_token, "File at '%s' do not exists.\n", clone_module_path);

            if(!utils_is_reg(clone_module_path))
                error(compiler, import_token, "File at '%s' is not a regular file.\n", clone_module_path);

            LZHTableNode *node = NULL;

            if(lzhtable_contains((uint8_t *)module_path, module_path_size, modules, &node)){
                Module *imported_module = (Module *)node->value;
                Module *cloned_module = runtime_clone_module(module_name, module_path, imported_module);
                
                add_module_symbol(module_name, module_name_size, cloned_module, current_symbols);
                declare(MODULE_SYMTYPE, name_token, compiler);
                
                break;
            }

            RawStr *source = utils_read_source(clone_module_path);
            
            LZHTable *keywords = compiler->keywords;
            LZHTable *natives = compiler->natives;

            DynArrPtr *tokens = compile_dynarr_ptr();
            DynArrPtr *stmts = compile_dynarr_ptr();
            
            Module *module = runtime_module(module_name, clone_module_path);
            SubModule *submodule = module->submodule;

            Lexer *lexer = lexer_create();
	        Parser *parser = parser_create();
            Compiler *import_compiler = compiler_create();

            if(lexer_scan(source, tokens, submodule->strings, keywords, clone_module_path, lexer)){
				compiler->is_err = 1;
				break;
			}

            if(parser_parse(tokens, stmts, parser)){
				compiler->is_err = 1;
				break;
			}

            if(compiler_import(keywords, natives, stmts, current_module, module, modules, import_compiler)){
                compiler->is_err = 1;
				break;
            }

            add_module_symbol(module_name, module_name_size, module, current_symbols);
            lzhtable_put((uint8_t *)clone_module_path, module_path_size, module, modules, NULL);
            declare(MODULE_SYMTYPE, name_token, compiler);
            
            break;
        }
        case THROW_STMTTYPE:{
            ThrowStmt *throw_stmt = (ThrowStmt *)stmt->sub_stmt;
            Token *throw_token = throw_stmt->throw_token;
			Expr *value = throw_stmt->value;

            Scope *scope = current_scope(compiler);
            
            if(!inside_function(compiler))
                error(compiler, throw_token, "Can not use 'throw' statement in global scope.");
            if(scope->type == CATCH_SCOPE)
                error(compiler, throw_token, "Can not use 'throw' statement inside catch blocks.");

			if(value) compile_expr(value, compiler);
			else {
                write_chunk(EMPTY_OPCODE, compiler);
                write_location(throw_token, compiler);
            }

            write_chunk(THROW_OPCODE, compiler);
            write_location(throw_token, compiler);

            break;
        }
        case TRY_STMTTYPE:{
            TryStmt *try_stmt = (TryStmt *)stmt->sub_stmt;
            DynArrPtr *try_stmts = try_stmt->try_stmts;
			Token *err_identifier = try_stmt->err_identifier;
			DynArrPtr *catch_stmts = try_stmt->catch_stmts;

            size_t try_jmp_index = 0;
            TryBlock *try_block = (TryBlock *)A_RUNTIME_ALLOC(sizeof(TryBlock));
			memset(try_block, 0, sizeof(TryBlock));

            if(try_stmts){
                size_t start = chunks_len(compiler);
                try_block->try = start;

                Scope *scope = scope_in(TRY_SCOPE, compiler);
                scope->try = try_block;

                Scope *previous = previous_scope(scope, compiler);
                
                if(previous && previous->try)
                    try_block->outer = previous->try;

                for (size_t i = 0; i < try_stmts->used; i++){
                    Stmt *stmt = (Stmt *)DYNARR_PTR_GET(i, try_stmts);
                    compile_stmt(stmt, compiler);
                }

                scope_out(compiler);

				write_chunk(JMP_OPCODE, compiler);
                write_location(try_stmt->try_token, compiler);
				try_jmp_index = write_i16(0, compiler);
            }

			if(catch_stmts){
				size_t start = chunks_len(compiler);
                try_block->catch = start;

                scope_in(CATCH_SCOPE, compiler);
				Symbol *symbol = declare(IMUT_SYMTYPE, err_identifier, compiler);

				try_block->local = symbol->local;

				for (size_t i = 0; i < catch_stmts->used; i++){
                    Stmt *stmt = (Stmt *)DYNARR_PTR_GET(i, catch_stmts);
                    compile_stmt(stmt, compiler);
                }
                
                scope_out(compiler);

				size_t end = chunks_len(compiler);
				update_i16(try_jmp_index, end - start + 1, compiler);
			}

            Module *module = compiler->current_module;
            SubModule *submodule = module->submodule;
            LZHTable *fn_tries = submodule->tries;
            Fn *fn = compiler->fn_stack[compiler->fn_ptr - 1];

			uint8_t *key = (uint8_t *)fn;
			size_t key_size = sizeof(Fn);
			LZHTableNode *node = NULL;
			DynArrPtr *tries = NULL;

			if(lzhtable_contains(key, key_size, fn_tries, &node)){
				tries = (DynArrPtr *)node->value;
			}else{
				tries = (DynArrPtr *)runtime_dynarr_ptr();
				lzhtable_put(key, key_size, tries, fn_tries, NULL);
			}

			dynarr_ptr_insert(try_block, tries);

            break;
        }
        default:{
            assert("Illegal stmt type");
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
    Module *current_module,
    LZHTable *modules,
    Compiler *compiler
){
    memset(compiler, 0, sizeof(Compiler));
    
    if(setjmp(compiler->err_jmp) == 1) return 1;
    else{
        compiler->symbols = 1;
        compiler->keywords = keywords;
        compiler->natives = natives;
        compiler->current_module = current_module;
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
    Module *current_module,
    LZHTable *modules,
    Compiler *compiler
){
    memset(compiler, 0, sizeof(Compiler));
    
    if(setjmp(compiler->err_jmp) == 1) return 1;
    else{
        compiler->keywords = keywords;
        compiler->natives = natives;
        compiler->stmts = stmts;
        compiler->previous_module = previous_module;
        compiler->current_module = current_module;
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
