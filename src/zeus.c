#include "memory.h"
#include "utils.h"

#include "vm_utils.h"
#include "native.h"

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

void add_native(char *name, int arity, RawNativeFn raw_native, LZHTable *natives){
    size_t name_len = strlen(name);
    NativeFn *native = (NativeFn *)A_RUNTIME_ALLOC(sizeof(NativeFn));

    native->unique = 1;
    native->arity = arity;
    memcpy(native->name, name, name_len);
    native->name[name_len] = '\0';
    native->name_len = name_len;
    native->target = NULL;
    native->raw_fn = raw_native;

    lzhtable_put((uint8_t *)name, name_len, native, natives, NULL);
}

void add_keyword(char *name, TokenType type, LZHTable *keywords){
    TokenType *ctype = (TokenType *)A_COMPILE_ALLOC(sizeof(TokenType));
    *ctype = type;
    lzhtable_put((uint8_t *)name, strlen(name), ctype, keywords, NULL);
}

LZHTable *create_keywords_table(){
	LZHTable *keywords = compile_lzhtable();
	
	add_keyword("mod", MOD_TOKTYPE, keywords);
	add_keyword("empty", EMPTY_TOKTYPE, keywords);
    add_keyword("false", FALSE_TOKTYPE, keywords);
    add_keyword("true", TRUE_TOKTYPE, keywords);
    add_keyword("mut", MUT_TOKTYPE, keywords);
    add_keyword("imut", IMUT_TOKTYPE, keywords);
    add_keyword("or", OR_TOKTYPE, keywords);
    add_keyword("and", AND_TOKTYPE, keywords);
	add_keyword("if", IF_TOKTYPE, keywords);
	add_keyword("else", ELSE_TOKTYPE, keywords);
	add_keyword("while", WHILE_TOKTYPE, keywords);
	add_keyword("stop", STOP_TOKTYPE, keywords);
    add_keyword("continue", CONTINUE_TOKTYPE, keywords);
    add_keyword("array", ARRAY_TOKTYPE, keywords);
	add_keyword("list", LIST_TOKTYPE, keywords);
    add_keyword("to", TO_TOKTYPE, keywords);
    add_keyword("dict", DICT_TOKTYPE, keywords);
	add_keyword("record", RECORD_TOKTYPE, keywords);
    add_keyword("proc", PROC_TOKTYPE, keywords);
    add_keyword("anon", ANON_TOKTYPE, keywords);
    add_keyword("ret", RET_TOKTYPE, keywords);
    add_keyword("import", IMPORT_TOKTYPE, keywords);
    add_keyword("as", AS_TOKTYPE, keywords);
	add_keyword("bool", BOOL_TOKTYPE, keywords);
	add_keyword("int", INT_TOKTYPE, keywords);
	add_keyword("float", FLOAT_TOKTYPE, keywords);
	add_keyword("str", STR_TOKTYPE, keywords);
	add_keyword("is", IS_TOKTYPE, keywords);
    add_keyword("try", TRY_TOKTYPE, keywords);
    add_keyword("catch", CATCH_TOKTYPE, keywords);
    add_keyword("throw", THROW_TOKTYPE, keywords);
    add_keyword("load", LOAD_TOKTYPE, keywords);
    add_keyword("export", EXPORT_TOKTYPE, keywords);
    
    return keywords;
}

int main(int argc, char const *argv[]){
	int result = 0;

    Args args = {0};
	get_args(argc, argv, &args);

	char *source_path = args.source_path;

	if(!source_path){
		fprintf(stderr, "Expect path to input source file\n");
		exit(EXIT_FAILURE);
	}

	if(!UTILS_FILE_CAN_READ(source_path)){
		fprintf(stderr, "File at '%s' do not exists or cannot be read.\n", source_path);
		exit(EXIT_FAILURE);
	}

	if(!utils_file_is_regular(source_path)){
		fprintf(stderr, "File at '%s' is not a regular file.\n", source_path);
		exit(EXIT_FAILURE);
	}

	memory_init();

    LZHTable *natives = runtime_lzhtable();
    
    add_native("ls", 1, native_fn_ls, natives);
    add_native("exit", 1, native_fn_exit, natives);
	add_native("assert", 1, native_fn_assert, natives);
    add_native("assertm", 2, native_fn_assertm, natives);
    add_native("is_str_int", 1, native_fn_is_str_int, natives);
    add_native("is_str_float", 1, native_fn_is_str_float, natives);
	add_native("str_to_int", 1, native_fn_str_to_int, natives);
    add_native("str_to_float", 1, native_fn_str_to_float, natives);
    add_native("int_to_str", 1, native_fn_int_to_str, natives);
    add_native("int_to_float", 1, native_fn_int_to_float, natives);
    add_native("float_to_int", 1, native_fn_float_to_int, natives);
    add_native("float_to_str", 1, native_fn_float_to_str, natives);
    add_native("print", 1, native_fn_print, natives);
    add_native("printerr", 1, native_fn_printerr, natives);

	RawStr *source = compile_read_source(source_path);
    char *module_path = compile_clone_str(source_path);
    
    Module *module = runtime_module("main", module_path);
    LZHTable *modules = compile_lzhtable();
    SubModule *submodule = module->submodule;
    LZHTable *strings = submodule->strings;

	DynArrPtr *tokens = compile_dynarr_ptr();
	DynArrPtr *stmts = compile_dynarr_ptr();

	LZHTable *keywords = create_keywords_table();

	Lexer *lexer = lexer_create();
	Parser *parser = parser_create();
    Compiler *compiler = compiler_create();
    Dumpper *dumpper = dumpper_create();    
    VM *vm = vm_create();

    if(module_path[0] == '/'){
		compiler->paths[compiler->paths_len++] = utils_parent_pathname(compile_clone_str(module_path));
	}else{
		compiler->paths[compiler->paths_len++] = compile_cwd();
	}

    if(args.lex){
        if(lexer_scan(source, tokens, strings, keywords, module_path, lexer)){
            result = 1;
			goto CLEAN_UP;
		}
		
		printf("no errors in lexer phase\n");
    }else if(args.parse){
        if(lexer_scan(source, tokens, strings, keywords, module_path, lexer)){
            result = 1;
			goto CLEAN_UP;
		}
        if(parser_parse(tokens, stmts, parser)){
            result = 1;
			goto CLEAN_UP;
		}
		
		printf("no errors in parse phase\n");
    }else if(args.compile){
        if(lexer_scan(source, tokens, strings, keywords, module_path, lexer)){
            result = 1;
			goto CLEAN_UP;
		}
        if(parser_parse(tokens, stmts, parser)){
            result = 1;
			goto CLEAN_UP;
		}
        if(compiler_compile(keywords, natives, stmts, module, modules, compiler)){
            result = 1;
			goto CLEAN_UP;
		}
		
		printf("no errors in compile phase\n");
    }else if(args.dump){
        if(lexer_scan(source, tokens, strings, keywords, module_path, lexer)){
            result = 1;
			goto CLEAN_UP;
		}
        if(parser_parse(tokens, stmts, parser)){
            result = 1;
			goto CLEAN_UP;
		}
        if(compiler_compile(keywords, natives, stmts, module, modules, compiler)){
            result = 1;
			goto CLEAN_UP;
		}
        
        dumpper_dump(modules, module, dumpper);
    }else{
        if(lexer_scan(source, tokens, strings, keywords, module_path, lexer)){
			result = 1;
            goto CLEAN_UP;
		}
        if(parser_parse(tokens, stmts, parser)){
            result = 1;
			goto CLEAN_UP;
		}
        if(compiler_compile(keywords, natives, stmts, module, modules, compiler)){
            result = 1;
			goto CLEAN_UP;
		}
        
        memory_free_compile();
        
        result = vm_execute(natives, module, vm);
        if(result){goto CLEAN_UP;}
    }

	if(args.memuse){
		memory_report();
	}

CLEAN_UP:
    memory_deinit();

    return result;
}
