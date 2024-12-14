#include "memory.h"
#include "utils.h"

#include "vm_utils.h"
#include "native.h"
#include "native_time.h"
#include "native_io.h"

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

void add_native(char *name, int arity, RawNativeFn raw_native, LZHTable *natives){
    size_t name_len = strlen(name);
    NativeFn *native = (NativeFn *)A_RUNTIME_ALLOC(sizeof(NativeFn));

    native->unique = 1;
    native->arity = arity;
    memcpy(native->name, name, name_len);
    native->name[name_len] = '\0';
    native->name_len = name_len;
    native->target = NULL;
    native->native = raw_native;

    lzhtable_put((uint8_t *)name, name_len, native, natives, NULL);
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

    LZHTable *natives = runtime_lzhtable();
	add_native("is_str_int", 1, native_fn_is_str_int, natives);
	add_native("str_to_int", 1, native_fn_str_to_int, natives);
    add_native("int_to_str", 1, native_fn_int_to_str, natives);
    add_native("time", 0, native_time, natives);
    add_native("sleep", 1, native_sleep, natives);
	add_native("readln", 0, native_readln, natives);

	RawStr *source = utils_read_source(source_path);
    char *module_path = compile_clone_str(source_path);
    LZHTable *keywords = compile_lzhtable();
    
    Module *module = runtime_module("main", module_path);
    LZHTable *modules = compile_lzhtable();
    LZHTable *strings = module->strings;

	DynArrPtr *tokens = compile_dynarr_ptr();
	DynArrPtr *stmts = compile_dynarr_ptr();
    LZHTable *globals = runtime_lzhtable();

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
    add_keyword("as", AS_TOKTYPE, keywords);
	add_keyword("bool", BOOL_TOKTYPE, keywords);
	add_keyword("int", INT_TOKTYPE, keywords);
	add_keyword("str", STR_TOKTYPE, keywords);
	add_keyword("is", IS_TOKTYPE, keywords);
    add_keyword("try", TRY_TOKTYPE, keywords);
    add_keyword("catch", CATCH_TOKTYPE, keywords);
    add_keyword("throw", THROW_TOKTYPE, keywords);

	Lexer *lexer = lexer_create();
	Parser *parser = parser_create();
    Compiler *compiler = compiler_create();
    Dumpper *dumpper = dumpper_create();    
    VM *vm = vm_create();

    if(args.lex){
        if(lexer_scan(source, tokens, strings, keywords, module_path, lexer)) goto CLEAN_UP;
		printf("No errors in lexer phase\n");
    }else if(args.parse){
        if(lexer_scan(source, tokens, strings, keywords, module_path, lexer)) goto CLEAN_UP;
        if(parser_parse(tokens, stmts, parser)) goto CLEAN_UP;
		printf("No errors in parse phase\n");
    }else if(args.compile){
        if(lexer_scan(source, tokens, strings, keywords, module_path, lexer)) goto CLEAN_UP;
        if(parser_parse(tokens, stmts, parser)) goto CLEAN_UP;
        if(compiler_compile(keywords, natives, stmts, module, modules, compiler)) goto CLEAN_UP;
		printf("No errors in compile phase\n");
    }else if(args.dump){
        if(lexer_scan(source, tokens, strings, keywords, module_path, lexer)) goto CLEAN_UP;
        if(parser_parse(tokens, stmts, parser)) goto CLEAN_UP;
        if(compiler_compile(keywords, natives, stmts, module, modules, compiler)) goto CLEAN_UP;
        dumpper_dump(module, dumpper);
    }else{
        if(lexer_scan(source, tokens, strings, keywords, module_path, lexer)) goto CLEAN_UP;
        if(parser_parse(tokens, stmts, parser)) goto CLEAN_UP;
        if(compiler_compile(keywords, natives, stmts, module, modules, compiler)) goto CLEAN_UP;
        memory_free_compile();
        if(vm_execute(natives, globals, module, vm)) goto CLEAN_UP;
    }

	if(args.memuse) memory_report();

CLEAN_UP:
    memory_deinit();
}
