#include "memory.h"
#include "utils.h"
#include "lzhtable.h"
#include "token.h"
#include "lexer.h"
#include "parser.h"
#include "compiler.h"
#include "vm.h"
#include <stdio.h>

void add_keyword(char *name, TokenType type, LZHTable *keywords){
    TokenType *ctype = (TokenType *)memory_alloc(sizeof(TokenType));
    *ctype = type;
    lzhtable_put((uint8_t *)name, strlen(name), ctype, keywords, NULL);
}

int main(int argc, char const *argv[]){
    if(argc != 2){
        fprintf(stderr, "Expect source path\n");
        return 1;
    }

	memory_init();

    char *source_path = (char *)argv[1];
	RawStr *source = utils_read_source(source_path);
	DynArrPtr *tokens = memory_dynarr_ptr();
	LZHTable *strings = memory_lzhtable();
	LZHTable *keywords = memory_lzhtable();
	DynArrPtr *stmts = memory_dynarr_ptr();
    DynArr *constants = memory_dynarr(sizeof(int64_t));
    DynArrPtr *functions = memory_dynarr_ptr();

	add_keyword("mod", MOD_TOKTYPE, keywords);
	add_keyword("empty", EMPTY_TOKTYPE, keywords);
    add_keyword("false", FALSE_TOKTYPE, keywords);
    add_keyword("true", TRUE_TOKTYPE, keywords);
    add_keyword("print", PRINT_TOKTYPE, keywords);
    add_keyword("mut", MUT_TOKTYPE, keywords);
    add_keyword("imut", IMUT_TOKTYPE, keywords);
    add_keyword("or", OR_TOKTYPE, keywords);
    add_keyword("and", AND_TOKTYPE, keywords);
	add_keyword("if", IF_TOKTYPE, keywords);
	add_keyword("else", ELSE_TOKTYPE, keywords);
	add_keyword("while", WHILE_TOKTYPE, keywords);
	add_keyword("stop", STOP_TOKTYPE, keywords);

	Lexer *lexer = lexer_create();
	Parser *parser = parser_create();
    Compiler *compiler = compiler_create();
    VM *vm = vm_create();

	if(lexer_scan(source, tokens, strings, keywords, lexer)) goto CLEAN_UP;
    if(parser_parse(tokens, stmts, parser)) goto CLEAN_UP;
    if(compiler_compile(constants, functions, stmts, compiler)) goto CLEAN_UP;
    if(vm_execute(constants, strings, functions, vm)) goto CLEAN_UP;

	memory_report();

CLEAN_UP:
    memory_deinit();
}
