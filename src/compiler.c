#include "compiler.h"
#include "memory.h"
#include "opcode.h"
#include "token.h"
#include "stmt.h"
#include <stdint.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <setjmp.h>

static jmp_buf err_jmp;

static void error(Compiler *compiler, char *msg, ...){
    va_list args;
    va_start(args, msg);

    fprintf(stderr, "Compiler error:\n\t");
    vfprintf(stderr, msg, args);
    fprintf(stderr, "\n");

    va_end(args);
    longjmp(err_jmp, 1);
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

Scope *scope_in(Compiler *compiler){
    Scope *scope = &compiler->scopes[compiler->depth++];
    
    scope->depth = compiler->depth;
    scope->locals = 0;
    scope->symbols_len = 0;
    
    return scope;
}

Scope *scope_in_soft(Compiler *compiler){
    Scope *enclosing = &compiler->scopes[compiler->depth - 1];
    Scope *scope = &compiler->scopes[compiler->depth++];
    
    scope->depth = compiler->depth;
    scope->locals = enclosing->locals;
    scope->symbols_len = 0;
    
    return scope;
}

void scope_out(Compiler *compiler){
    compiler->depth--;
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

    error(compiler, "Symbol '%s' doesn't exists.", identifier);
    return NULL;
}

Symbol *declare(Token *identifier_token, Compiler *compiler){
    char *identifier = identifier_token->lexeme;
    size_t identifier_len = strlen(identifier);

    if(exists_local(identifier, compiler) != NULL)
        error(compiler, "Already exists a symbol named as '%s'.", identifier);

    Scope *scope = current_scope(compiler);
    Symbol *symbol = &scope->symbols[scope->symbols_len++];
    
    symbol->local = scope->locals++;
    memcpy(symbol->name, identifier, identifier_len);
    symbol->name[identifier_len] = '\0';
    symbol->name_len = identifier_len;

    return symbol;
}

void write_chunk(uint8_t chunk, Compiler *compiler){
    DynArr *chunks = compiler->chunks;
    dynarr_insert(&chunk, chunks);
}

void write_i64(int64_t i64, Compiler *compiler){
    DynArr *constants = compiler->constants;
    int32_t index = (int32_t) constants->used;

    dynarr_insert(&i64, constants);
    
    uint8_t bytes[4];
    descompose_i32(index, bytes);

    for (size_t i = 0; i < 4; i++)
        write_chunk(bytes[i], compiler);
}

void compile_expr(Expr *expr, Compiler *compiler){
    switch (expr->type){
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
            write_i64(value, compiler);

            break;
        }
        case IDENTIFIER_EXPRTYPE:{
            IdentifierExpr *identifier_expr = (IdentifierExpr *)expr->sub_expr;
            Token *identifier_token = identifier_expr->identifier_token;

            Symbol *symbol = get(identifier_token, compiler);
            uint8_t local = (uint8_t)symbol->local;
            
            write_chunk(LGET_OPCODE, compiler);
            write_chunk(local, compiler);

            break;
        }
        case GROUP_EXPRTYPE:{
            GroupExpr *group_expr = (GroupExpr *)expr->sub_expr;
            Expr *expr = group_expr->expr;

            compile_expr(expr, compiler);
            
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
            
            compile_expr(value_expr, compiler);
            write_chunk(LSET_OPCODE, compiler);
            write_chunk(symbol->local, compiler);

            break;
        }
        default:{
            assert("Illegal expression type");
        }
    }
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
            Token *identifier_token = var_decl_stmt->identifier_token;
            Expr *initializer_expr = var_decl_stmt->initializer_expr;

            Symbol *symbol = declare(identifier_token, compiler);
            
            if(initializer_expr == NULL) write_chunk(NULL_OPCODE, compiler);
            else compile_expr(initializer_expr, compiler);

            write_chunk(LSET_OPCODE, compiler);
            write_chunk((uint8_t)symbol->local, compiler);
            write_chunk(POP_OPCODE, compiler);

            break;
        }
        case BLOCK_STMTTYPE:{
            BlockStmt *block_stmt = (BlockStmt *)stmt->sub_stmt;
            DynArrPtr *stmts = block_stmt->stmts;

            scope_in_soft(compiler);

            for (size_t i = 0; i < stmts->used; i++)
            {
                Stmt *stmt = (Stmt *)DYNARR_PTR_GET(i, stmts);
                compile_stmt(stmt, compiler);
            }

            scope_out(compiler);
            
            break;
        }
        default:{
            assert("Illegal stmt type");
            break;
        }
    }
}

Compiler *compiler_create(){
    Compiler *compiler = (Compiler *)memory_alloc(sizeof(Compiler));
    memset(compiler, 0, sizeof(Compiler));
    return compiler;
}

int compiler_compile(
    DynArr *constants,
    DynArr *chunks,
    DynArrPtr *stmts, 
    Compiler *compiler
){
    if(setjmp(err_jmp) == 1) return 1;
    else{
        compiler->constants = constants;
        compiler->chunks = chunks;
        compiler->stmts = stmts;

        scope_in(compiler);

        for (size_t i = 0; i < stmts->used; i++){
            Stmt *stmt = (Stmt *)DYNARR_PTR_GET(i, stmts);
            compile_stmt(stmt, compiler);
        }

        scope_out(compiler);
        
        return 0;
    }
}