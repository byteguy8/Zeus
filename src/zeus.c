#include "memory.h"
#include "utils.h"
#include "lzhtable.h"
#include "token.h"
#include "lexer.h"
#include "parser.h"
#include "compiler.h"
#include "dumpper.h"
#include "vm.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

typedef struct args{
	unsigned char lex;
	unsigned char parse;
	unsigned char compile;
	unsigned char dump;
	unsigned char memuse;
    char *source_path;
}Args;

static void get_args(int argc, char const *argv[], Args *args){
	for(int i = 1; i < argc; i++){
		const char *arg = argv[i];
		if(strcmp("-l", arg) == 0) args->lex = 1;
		else if(strcmp("-p", arg) == 0) args->parse = 1;
		else if(strcmp("-c", arg) == 0) args->compile = 1;
		else if(strcmp("-d", arg) == 0) args->dump = 1;
		else if(strcmp("-u", arg) == 0) args->memuse = 1;
        else args->source_path = (char *)arg;
	}
}

void add_keyword(char *name, TokenType type, LZHTable *keywords){
    TokenType *ctype = (TokenType *)A_COMPILE_ALLOC(sizeof(TokenType));
    *ctype = type;
    lzhtable_put((uint8_t *)name, strlen(name), ctype, keywords, NULL);
}

int main(int argc, char const *argv[]){
	Args args = {0};
	get_args(argc, argv, &args);

	 char *source_path = args.source_path;

	if(!source_path){
		fprintf(stderr, "Expect path to input source file\n");
		exit(EXIT_FAILURE);
	}

	if(!UTILS_EXISTS_FILE(source_path)){
		fprintf(stderr, "File at '%s' do not exists.\n", source_path);
		exit(EXIT_FAILURE);
	}

	if(!utils_is_reg(source_path)){
		fprintf(stderr, "File at '%s' is not a regular file.\n", source_path);
		exit(EXIT_FAILURE);
	}

	memory_init();

	RawStr *source = utils_read_source(source_path);
	DynArrPtr *tokens = compile_dynarr_ptr();
	LZHTable *strings = runtime_lzhtable();
	LZHTable *keywords = compile_lzhtable();
	DynArrPtr *stmts = compile_dynarr_ptr();
    DynArr *constants = runtime_dynarr(sizeof(int64_t));
    DynArrPtr *functions = runtime_dynarr_ptr();

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
    add_keyword("continue", CONTINUE_TOKTYPE, keywords);
	add_keyword("list", LIST_TOKTYPE, keywords);
    add_keyword("to", TO_TOKTYPE, keywords);
    add_keyword("dict", DICT_TOKTYPE, keywords);
    add_keyword("proc", PROC_TOKTYPE, keywords);
    add_keyword("ret", RET_TOKTYPE, keywords);
    add_keyword("import", IMPORT_TOKTYPE, keywords);

	Lexer *lexer = lexer_create();
	Parser *parser = parser_create();
    Compiler *compiler = compiler_create();
    Dumpper *dumpper = dumpper_create();    
    VM *vm = vm_create();

    if(args.lex){
        if(lexer_scan(source, tokens, strings, keywords, lexer)) goto CLEAN_UP;
		printf("No errors in lexer phase\n");
    }else if(args.parse){
        if(lexer_scan(source, tokens, strings, keywords, lexer)) goto CLEAN_UP;
        if(parser_parse(tokens, stmts, parser)) goto CLEAN_UP;
		printf("No errors in parse phase\n");
    }else if(args.compile){
        if(lexer_scan(source, tokens, strings, keywords, lexer)) goto CLEAN_UP;
        if(parser_parse(tokens, stmts, parser)) goto CLEAN_UP;
        if(compiler_compile(keywords, constants, strings, functions, stmts, compiler)) goto CLEAN_UP;
		printf("No errors in compile phase\n");
    }else if(args.dump){
        if(lexer_scan(source, tokens, strings, keywords, lexer)) goto CLEAN_UP;
        if(parser_parse(tokens, stmts, parser)) goto CLEAN_UP;
        if(compiler_compile(keywords, constants, strings, functions, stmts, compiler)) goto CLEAN_UP;
        dumpper_dump(constants, strings, functions, dumpper);
    }else{
        if(lexer_scan(source, tokens, strings, keywords, lexer)) goto CLEAN_UP;
        if(parser_parse(tokens, stmts, parser)) goto CLEAN_UP;
        if(compiler_compile(keywords, constants, strings, functions, stmts, compiler)) goto CLEAN_UP;
        memory_free_compile();
        if(vm_execute(constants, strings, functions, vm)) goto CLEAN_UP;
    }

	if(args.memuse) memory_report();

CLEAN_UP:
    memory_deinit();
}
