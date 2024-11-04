#include "compiler.h"
#include "memory.h"
#include "opcode.h"
#include "token.h"
#include "stmt.h"
#include <stdint.h>
#include <assert.h>

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
    compiler->constants = constants;
    compiler->chunks = chunks;
    compiler->stmts = stmts;

    for (size_t i = 0; i < stmts->used; i++){
        Stmt *stmt = (Stmt *)DYNARR_PTR_GET(i, stmts);
        compile_stmt(stmt, compiler);
    }
    
    return 0;
}