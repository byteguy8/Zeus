#include "lzhtable.h"
#include "memory.h"
#include "utils.h"
#include "factory.h"
#include "token.h"
#include "lexer.h"
#include "parser.h"
#include "compiler.h"
#include "dumpper.h"
#include "native_module/native_module_default.h"
#include "vm.h"
#include "vmu.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

typedef struct args{
	unsigned char lex;
	unsigned char parse;
	unsigned char compile;
	unsigned char dump;
    char *source_pathname;
}Args;

void *lzalloc_link_arena(size_t size, void *ctx){
    return LZARENA_ALLOC(size, ctx);
}

void *lzrealloc_link_arena(void *ptr, size_t old_size, size_t new_size, void *ctx){
    return LZARENA_REALLOC(ptr, old_size, new_size, ctx);
}

void lzdealloc_link_arena(void *ptr, size_t size, void *ctx){
    // nothing to be done
}

void *lzalloc_link_flist(size_t size, void *ctx){
    return lzflist_alloc(size, ctx);
}

void *lzrealloc_link_flist(void *ptr, size_t old_size, size_t new_size, void *ctx){
    return lzflist_realloc(ptr, new_size, ctx);
}

void lzdealloc_link_flist(void *ptr, size_t size, void *ctx){
    lzflist_dealloc(ptr, ctx);
}

static void get_args(int argc, const char *argv[], Args *args){
	for(int i = 1; i < argc; i++){
		const char *arg = argv[i];

		if(strcmp("-l", arg) == 0){
            if(args->lex){
                fprintf(stderr, "-l flag already used");
                exit(EXIT_FAILURE);
            }

            args->lex = 1;
        }else if(strcmp("-p", arg) == 0){
            if(args->parse){
                fprintf(stderr, "-l flag already used");
                exit(EXIT_FAILURE);
            }

            args->parse = 1;
        }else if(strcmp("-c", arg) == 0){
            if(args->compile){
                fprintf(stderr, "-l flag already used");
                exit(EXIT_FAILURE);
            }

            args->compile = 1;
        }else if(strcmp("-d", arg) == 0){
            if(args->dump){
                fprintf(stderr, "-l flag already used");
                exit(EXIT_FAILURE);
            }

            args->dump = 1;
        }else{
            if(args->source_pathname){
                fprintf(stderr, "source pathname set");
                exit(EXIT_FAILURE);
            }

            args->source_pathname = (char *)arg;
        }
	}
}

void add_native(const char *name, int arity, RawNativeFn raw_native, LZOHTable *natives, const Allocator *allocator){
    NativeFn *native_fn = factory_create_native_fn(1, name, arity, raw_native, allocator);
    lzohtable_put_ck(strlen(name), name, native_fn, natives, NULL);
}

void add_keyword(const char *name, TokType type, LZOHTable *keywords, const Allocator *allocator){
    lzohtable_put_ckv(strlen(name), name, sizeof(TokType), &type, keywords, NULL);
}

LZOHTable *create_keywords_table(const Allocator *allocator){
	LZOHTable *keywords = FACTORY_LZOHTABLE_LEN(64, allocator);

	add_keyword("mod", MOD_TOKTYPE, keywords, allocator);
	add_keyword("empty", EMPTY_TOKTYPE, keywords, allocator);
    add_keyword("false", FALSE_TOKTYPE, keywords, allocator);
    add_keyword("true", TRUE_TOKTYPE, keywords, allocator);
    add_keyword("mut", MUT_TOKTYPE, keywords, allocator);
    add_keyword("imut", IMUT_TOKTYPE, keywords, allocator);
    add_keyword("or", OR_TOKTYPE, keywords, allocator);
    add_keyword("and", AND_TOKTYPE, keywords, allocator);
	add_keyword("if", IF_TOKTYPE, keywords, allocator);
    add_keyword("elif", ELIF_TOKTYPE, keywords, allocator);
	add_keyword("else", ELSE_TOKTYPE, keywords, allocator);
	add_keyword("while", WHILE_TOKTYPE, keywords, allocator);
    add_keyword("for", FOR_TOKTYPE, keywords, allocator);
    add_keyword("upto", UPTO_TOKTYPE, keywords, allocator);
    add_keyword("downto", DOWNTO_TOKTYPE, keywords, allocator);
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
    add_keyword("export", EXPORT_TOKTYPE, keywords, allocator);

    return keywords;
}

static void print_help(){
    fprintf(stderr, "Usage: zeus /path/to/source/file.ze [-[l | p | c | d]]\n");

    fprintf(stderr, "\n    The Zeus Programming Language\n");
    fprintf(stderr, "        Zeus is a dynamic programming language made for learning purposes\n\n");

    fprintf(stderr, "Options:\n");

    fprintf(stderr, "    -l\n");
    fprintf(stderr, "                      Just run the lexer\n");

    fprintf(stderr, "    -p\n");
    fprintf(stderr, "                      Just run the lexer and parser\n");

    fprintf(stderr, "    -c\n");
    fprintf(stderr, "                      Just run the lexer, parser and compiler\n");

    fprintf(stderr, "    -d\n");
    fprintf(stderr, "                      Run the disassembler (executing: lexer, parser and compiler)\n");

    exit(EXIT_FAILURE);
}

static void print_size(size_t size){
    if(size < 1024){
        printf("%zu B", size);
        return;
    }

    size /= 1024;

    if(size < 1024){
        printf("%zu KiB", size);
        return;
    }

    size /= 1024;

    if(size < 1024){
        printf("%zu MiB", size);
        return;
    }

    size /= 1024;

    if(size < 1024){
        printf("%zu GiB", size);
        return;
    }
}

int main(int argc, const char *argv[]){
	int result = 0;
    Args args = {0};

	get_args(argc, argv, &args);

	char *source_pathname = args.source_pathname;

	if(!source_pathname){
		print_help();
	}

	if(!UTILS_FILES_CAN_READ(source_pathname)){
		fprintf(stderr, "File at '%s' do not exists or cannot be read.\n", source_pathname);
		exit(EXIT_FAILURE);
	}

	if(!utils_files_is_regular(source_pathname)){
		fprintf(stderr, "File at '%s' is not a regular file.\n", source_pathname);
		exit(EXIT_FAILURE);
	}

    LZArena *ctarena = lzarena_create(NULL);
    LZFList *rtflist = lzflist_create();

    if(!ctarena || !rtflist){
        lzarena_destroy(ctarena);
        lzflist_destroy(rtflist);
        exit(EXIT_FAILURE);
    }

    Allocator ctallocator = {0};
    Allocator rtallocator = {0};

    MEMORY_INIT_ALLOCATOR(
        ctarena,
        lzalloc_link_arena,
        lzrealloc_link_arena,
        lzdealloc_link_arena,
        &ctallocator
    );

    MEMORY_INIT_ALLOCATOR(
        rtflist,
        lzalloc_link_flist,
        lzrealloc_link_flist,
        lzdealloc_link_flist,
        &rtallocator
    );

    LZOHTable *natives_fns = FACTORY_LZOHTABLE_LEN(64, &rtallocator);

    add_native("exit", 1, native_fn_exit, natives_fns, &rtallocator);
	add_native("assert", 1, native_fn_assert, natives_fns, &rtallocator);
    add_native("assertm", 2, native_fn_assertm, natives_fns, &rtallocator);

    add_native("is_str_int", 1, native_fn_is_str_int, natives_fns, &rtallocator);
    add_native("is_str_float", 1, native_fn_is_str_float, natives_fns, &rtallocator);
    add_native("to_str", 1, native_fn_to_str, natives_fns, &rtallocator);
    add_native("to_int", 1, native_fn_to_int, natives_fns, &rtallocator);
    add_native("to_float", 1, native_fn_to_float, natives_fns, &rtallocator);

    add_native("print", 1, native_fn_print, natives_fns, &rtallocator);
    add_native("println", 1, native_fn_println, natives_fns, &rtallocator);
    add_native("eprint", 1, native_fn_eprint, natives_fns, &rtallocator);
    add_native("eprintln", 1, native_fn_eprintln, natives_fns, &rtallocator);
    add_native("print_stack", 0, native_fn_print_stack, natives_fns, &rtallocator);
    add_native("readln", 0, native_fn_readln, natives_fns, &rtallocator);

    add_native("gc", 0, native_fn_gc, natives_fns, &rtallocator);
    add_native("halt", 0, native_fn_halt, natives_fns, &rtallocator);

    LZOHTable *keywords = create_keywords_table(&rtallocator);
	RawStr *source = utils_read_source(source_pathname, &rtallocator);
    char *module_path = factory_clone_raw_str(source_pathname, &rtallocator, NULL);
    Module *module = factory_create_module("main", module_path, &rtallocator);

    LZHTable *modules = FACTORY_LZHTABLE(&ctallocator);
	DynArr *tokens = FACTORY_DYNARR_PTR(&ctallocator);
	DynArr *stmts = FACTORY_DYNARR_PTR(&ctallocator);
	Lexer *lexer = lexer_create(&ctallocator, &rtallocator);
	Parser *parser = parser_create(&ctallocator);
    Compiler *compiler = compiler_create(&ctallocator, &rtallocator);
    Dumpper *dumpper = dumpper_create(&ctallocator);

    VM *vm = vm_create(&rtallocator);

    if(module_path[0] == '/'){
        char *cloned_module_path = factory_clone_raw_str(module_path, &rtallocator, NULL);
		compiler->paths[compiler->paths_len++] = utils_files_parent_pathname(cloned_module_path);
	}else{
		compiler->paths[compiler->paths_len++] = utils_files_cwd(&ctallocator);
	}

    if(args.lex){
        if(lexer_scan(source, tokens, keywords, module_path, lexer)){
            result = 1;
			goto CLEAN_UP;
		}

        printf("STATISTICS:\n");
		printf("    %-20s%zu\n", "Tokens:", DYNARR_LEN(tokens));
        printf("    %-20s", "Memory usage:");
        print_size(ctarena->allocted_bytes);
        printf("\n");
    }else if(args.parse){
        if(lexer_scan(source, tokens, keywords, module_path, lexer)){
            result = 1;
			goto CLEAN_UP;
		}
        if(parser_parse(tokens, stmts, parser)){
            result = 1;
			goto CLEAN_UP;
		}

		printf("STATISTICS:\n");
		printf("    %-20s%zu\n", "Tokens:", DYNARR_LEN(tokens));
        printf("    %-20s%zu\n", "Statements:", DYNARR_LEN(stmts));
        printf("    %-20s", "Memory usage:");
        print_size(ctarena->allocted_bytes);
        printf("\n");
    }else if(args.compile){
        if(lexer_scan(source, tokens, keywords, module_path, lexer)){
            result = 1;
			goto CLEAN_UP;
		}
        if(parser_parse(tokens, stmts, parser)){
            result = 1;
			goto CLEAN_UP;
		}
        if(compiler_compile(keywords, natives_fns, stmts, module, modules, compiler)){
            result = 1;
			goto CLEAN_UP;
		}

		printf("STATISTICS:\n");
		printf("    %-20s%zu\n", "Tokens:", DYNARR_LEN(tokens));
        printf("    %-20s%zu\n", "Statements:", DYNARR_LEN(stmts));
        printf("    %-20s%zu\n", "Symbols:", DYNARR_LEN(MODULE_SYMBOLS(module)));
        printf("    %-20s", "Memory usage:");
        print_size(ctarena->allocted_bytes);
        printf("\n");
    }else if(args.dump){
        if(lexer_scan(source, tokens, keywords, module_path, lexer)){
            result = 1;
			goto CLEAN_UP;
		}
        if(parser_parse(tokens, stmts, parser)){
            result = 1;
			goto CLEAN_UP;
		}
        if(compiler_compile(keywords, natives_fns, stmts, module, modules, compiler)){
            result = 1;
			goto CLEAN_UP;
		}

        dumpper_dump(modules, module, dumpper);

        printf("\nSTATISTICS:\n");
		printf("    %-20s%zu\n", "Tokens:", DYNARR_LEN(tokens));
        printf("    %-20s%zu\n", "Statements:", DYNARR_LEN(stmts));
        printf("    %-20s%zu\n", "Symbols:", DYNARR_LEN(MODULE_SYMBOLS(module)));
        printf("    %-20s", "Memory usage:");
        print_size(ctarena->allocted_bytes);
        printf("\n");
    }else{
        if(lexer_scan(source, tokens, keywords, module_path, lexer)){
			result = 1;
            goto CLEAN_UP;
		}
        if(parser_parse(tokens, stmts, parser)){
            result = 1;
			goto CLEAN_UP;
		}
        if(compiler_compile(keywords, natives_fns, stmts, module, modules, compiler)){
            result = 1;
			goto CLEAN_UP;
		}

        lzarena_destroy(ctarena);
        vm_initialize(vm);

        result = vm_execute(natives_fns, module, vm);

        goto CLEAN_UP_RUNTIME;
    }

CLEAN_UP:
//    vm_destroy(vm);
    lzarena_destroy(ctarena);
CLEAN_UP_RUNTIME:
    lzflist_destroy(rtflist);

    return result;
}
