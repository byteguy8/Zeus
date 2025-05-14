#include "memory.h"
#include "factory.h"
#include "utils.h"

#include "vmu.h"
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
    char *source_path;
}Args;

static void get_args(int argc, char const *argv[], Args *args){
	for(int i = 1; i < argc; i++){
		const char *arg = argv[i];
		if(strcmp("-l", arg) == 0) args->lex = 1;
		else if(strcmp("-p", arg) == 0) args->parse = 1;
		else if(strcmp("-c", arg) == 0) args->compile = 1;
		else if(strcmp("-d", arg) == 0) args->dump = 1;
        else args->source_path = (char *)arg;
	}
}

void add_native(char *name, int arity, RawNativeFn raw_native, LZHTable *natives, Allocator *allocator){
    NativeFn *native = factory_create_native_fn(1, name, arity, NULL, raw_native, allocator);
    lzhtable_put((uint8_t *)name, strlen(name), native, natives, NULL);
}

void add_keyword(char *name, TokType type, LZHTable *keywords, Allocator *allocator){
    TokType *ctype = (TokType *)MEMORY_ALLOC(TokType, 1, allocator);
    *ctype = type;
    lzhtable_put((uint8_t *)name, strlen(name), ctype, keywords, NULL);
}

LZHTable *create_keywords_table(Allocator *allocator){
	LZHTable *keywords = FACTORY_LZHTABLE(allocator);

	add_keyword("mod", MOD_TOKTYPE, keywords, allocator);
	add_keyword("empty", EMPTY_TOKTYPE, keywords, allocator);
    add_keyword("false", FALSE_TOKTYPE, keywords, allocator);
    add_keyword("true", TRUE_TOKTYPE, keywords, allocator);
    add_keyword("mut", MUT_TOKTYPE, keywords, allocator);
    add_keyword("imut", IMUT_TOKTYPE, keywords, allocator);
    add_keyword("or", OR_TOKTYPE, keywords, allocator);
    add_keyword("and", AND_TOKTYPE, keywords, allocator);
	add_keyword("if", IF_TOKTYPE, keywords, allocator);
	add_keyword("else", ELSE_TOKTYPE, keywords, allocator);
	add_keyword("while", WHILE_TOKTYPE, keywords, allocator);
	add_keyword("stop", STOP_TOKTYPE, keywords, allocator);
    add_keyword("continue", CONTINUE_TOKTYPE, keywords, allocator);
    add_keyword("array", ARRAY_TOKTYPE, keywords, allocator);
	add_keyword("list", LIST_TOKTYPE, keywords, allocator);
    add_keyword("to", TO_TOKTYPE, keywords, allocator);
    add_keyword("dict", DICT_TOKTYPE, keywords, allocator);
	add_keyword("record", RECORD_TOKTYPE, keywords, allocator);
    add_keyword("proc", PROC_TOKTYPE, keywords, allocator);
    add_keyword("anon", ANON_TOKTYPE, keywords, allocator);
    add_keyword("ret", RET_TOKTYPE, keywords, allocator);
    add_keyword("import", IMPORT_TOKTYPE, keywords, allocator);
    add_keyword("as", AS_TOKTYPE, keywords, allocator);
	add_keyword("bool", BOOL_TOKTYPE, keywords, allocator);
	add_keyword("int", INT_TOKTYPE, keywords, allocator);
	add_keyword("float", FLOAT_TOKTYPE, keywords, allocator);
	add_keyword("str", STR_TOKTYPE, keywords, allocator);
	add_keyword("is", IS_TOKTYPE, keywords, allocator);
    add_keyword("try", TRY_TOKTYPE, keywords, allocator);
    add_keyword("catch", CATCH_TOKTYPE, keywords, allocator);
    add_keyword("throw", THROW_TOKTYPE, keywords, allocator);
    add_keyword("load", LOAD_TOKTYPE, keywords, allocator);
    add_keyword("export", EXPORT_TOKTYPE, keywords, allocator);

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

	if(!utils_files_is_regular(source_path)){
		fprintf(stderr, "File at '%s' is not a regular file.\n", source_path);
		exit(EXIT_FAILURE);
	}

	memory_init();

    LZArena *compile_arena = NULL;
    Allocator *ctallocator = memory_arena_allocator(&compile_arena);
    Allocator *rtallocator = memory_allocator();

    LZHTable *natives = FACTORY_LZHTABLE(rtallocator);

    add_native("print_stack", 0, native_fn_print_stack, natives, rtallocator);
    add_native("ls", 1, native_fn_ls, natives, rtallocator);
    add_native("exit", 1, native_fn_exit, natives, rtallocator);
	add_native("assert", 1, native_fn_assert, natives, rtallocator);
    add_native("assertm", 2, native_fn_assertm, natives, rtallocator);
    add_native("is_str_int", 1, native_fn_is_str_int, natives, rtallocator);
    add_native("is_str_float", 1, native_fn_is_str_float, natives, rtallocator);
	add_native("str_to_int", 1, native_fn_str_to_int, natives, rtallocator);
    add_native("str_to_float", 1, native_fn_str_to_float, natives, rtallocator);
    add_native("int_to_str", 1, native_fn_int_to_str, natives, rtallocator);
    add_native("int_to_float", 1, native_fn_int_to_float, natives, rtallocator);
    add_native("float_to_int", 1, native_fn_float_to_int, natives, rtallocator);
    add_native("float_to_str", 1, native_fn_float_to_str, natives, rtallocator);
    add_native("print", 1, native_fn_print, natives, rtallocator);
    add_native("println", 1, native_fn_println, natives, rtallocator);
    add_native("eprint", 1, native_fn_eprint, natives, rtallocator);
    add_native("eprintln", 1, native_fn_eprintln, natives, rtallocator);

	RawStr *source = utils_read_source(source_path, rtallocator);
    char *module_path = factory_clone_raw_str(source_path, rtallocator);

    Module *module = factory_module("main", module_path, rtallocator);
    LZHTable *modules = FACTORY_LZHTABLE(ctallocator);

    SubModule *submodule = module->submodule;
    LZHTable *strings = submodule->strings;

	DynArr *tokens = FACTORY_DYNARR_PTR(ctallocator);
	DynArr *stmts = FACTORY_DYNARR_PTR(ctallocator);

	LZHTable *keywords = create_keywords_table(rtallocator);

	Lexer *lexer = lexer_create(rtallocator);
	Parser *parser = parser_create(ctallocator);
    Compiler *compiler = compiler_create(memory_arena_allocator(NULL), rtallocator);
    Dumpper *dumpper = dumpper_create(ctallocator);
    VM *vm = vm_create(rtallocator);

    if(module_path[0] == '/'){
        char *cloned_module_path = factory_clone_raw_str(module_path, rtallocator);
		compiler->paths[compiler->paths_len++] = utils_files_parent_pathname(cloned_module_path);
	}else{
		compiler->paths[compiler->paths_len++] = utils_files_cwd(ctallocator);
	}

    if(args.lex){
        if(lexer_scan(source, tokens, strings, keywords, module_path, lexer)){
            result = 1;
			goto CLEAN_UP;
		}

		printf("No errors during lexing\n");
    }else if(args.parse){
        if(lexer_scan(source, tokens, strings, keywords, module_path, lexer)){
            result = 1;
			goto CLEAN_UP;
		}
        if(parser_parse(tokens, stmts, parser)){
            result = 1;
			goto CLEAN_UP;
		}

		printf("No errors during parsing\n");
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

		printf("No errors during compilation\n");
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

        if((result = vm_execute(natives, module, vm))){
            goto CLEAN_UP;
        }
    }

CLEAN_UP:
    memory_deinit();

    return result;
}
