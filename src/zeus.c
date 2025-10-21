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

#define DEFAULT_INITIAL_COMPILE_TIME_MEMORY 4194304
#define DEFAULT_INITIAL_RUNTIME_MEMORY 8388608
#define DEFAULT_INITIAL_SEARCH_PATHS_BUFF_LEN 256

typedef struct args{
	unsigned char lex;
	unsigned char parse;
	unsigned char compile;
	unsigned char dump;
    unsigned char help;
    char *search_paths;
    char *source_pathname;
}Args;

static LZFList *allocator = NULL;

void *lzalloc_link_flist(size_t size, void *ctx){
    void *ptr = lzflist_alloc(size, ctx);

    if(!ptr){
        fprintf(stderr, "It seems the system ran out of memory");
        lzflist_destroy(allocator);
        exit(EXIT_FAILURE);
    }

    return ptr;
}

void *lzrealloc_link_flist(void *ptr, size_t old_size, size_t new_size, void *ctx){
    void *new_ptr = lzflist_realloc(ptr, new_size, ctx);

    if(!new_ptr){
        fprintf(stderr, "It seems the system ran out of memory");
        lzflist_destroy(allocator);
        exit(EXIT_FAILURE);
    }

    return new_ptr;
}

void lzdealloc_link_flist(void *ptr, size_t size, void *ctx){
    lzflist_dealloc(ptr, ctx);
}

void *lzalloc_link_arena(size_t size, void *ctx){
    return LZARENA_ALLOC(size, ctx);
}

void *lzrealloc_link_arena(void *ptr, size_t old_size, size_t new_size, void *ctx){
    return LZARENA_REALLOC(ptr, old_size, new_size, ctx);
}

void lzdealloc_link_arena(void *ptr, size_t size, void *ctx){
    // nothing to be done
}

static void get_args(int argc, const char *argv[], Args *args){
    unsigned char exclusives = 0;

    for(int i = 1; i < argc; i++){
		const char *arg = argv[i];

		if(strcmp("-l", arg) == 0){
            if(args->lex){
                fprintf(stderr, "-l flag already used");
                exit(EXIT_FAILURE);
            }

            args->lex = 1;
            exclusives |= 0b10000000;
        }else if(strcmp("-p", arg) == 0){
            if(args->parse){
                fprintf(stderr, "-l flag already used");
                exit(EXIT_FAILURE);
            }

            args->parse = 1;
            exclusives |= 0b01000000;
        }else if(strcmp("-c", arg) == 0){
            if(args->compile){
                fprintf(stderr, "-l flag already used");
                exit(EXIT_FAILURE);
            }

            args->compile = 1;
            exclusives |= 0b00100000;
        }else if(strcmp("-d", arg) == 0){
            if(args->dump){
                fprintf(stderr, "-l flag already used");
                exit(EXIT_FAILURE);
            }

            args->dump = 1;
            exclusives |= 0b00010000;
        }else if(strcmp("-h", arg) == 0){
            if(args->help){
                fprintf(stderr, "-h flag already used");
                exit(EXIT_FAILURE);
            }

            args->help = 1;
            exclusives |= 0b00001000;
        }else if(strcmp("--spaths", arg) == 0){
            if(args->search_paths){
                fprintf(stderr, "search path already set");
                exit(EXIT_FAILURE);
            }

            if(i + 1 >= argc){
                fprintf(stderr, "expect search paths string after --spaths flag");
                exit(EXIT_FAILURE);
            }

            args->search_paths = (char *)argv[++i];
        }else{
            if(args->source_pathname){
                fprintf(stderr, "source pathname set");
                exit(EXIT_FAILURE);
            }

            args->source_pathname = (char *)arg;
        }
	}

    if(exclusives >= 136){
        fprintf(stderr, "Can only use one of this flags at time: -l -p -c -d -h\n");
        exit(EXIT_FAILURE);
    }
}

DStr get_cwd(Allocator *allocator){
    char *buff = utils_files_cwd(allocator);
    size_t len = strlen(buff);

    return (DStr){
        .len = len,
        .buff = buff
    };
}

DynArr *parse_search_paths(char *source_pathname, char *raw_search_paths, Allocator *allocator){
    DynArr *search_paths = DYNARR_CREATE_TYPE_BY(
        DStr,
        DEFAULT_INITIAL_SEARCH_PATHS_BUFF_LEN,
        (DynArrAllocator *)allocator
    );

    DStr cwd_rstr = get_cwd(allocator);

    dynarr_insert(&cwd_rstr, search_paths);

    size_t source_pathname_len = strlen(source_pathname);
    size_t result_pathname_len = cwd_rstr.len + 1 + source_pathname_len;
    char result_pathname[result_pathname_len + 1];

    memcpy(result_pathname, cwd_rstr.buff, cwd_rstr.len);
    memcpy(result_pathname + cwd_rstr.len, "/", 1);
    memcpy(result_pathname + cwd_rstr.len + 1, source_pathname, source_pathname_len);
    result_pathname[result_pathname_len] = 0;

    if(!UTILS_FILES_EXISTS(result_pathname)){
        char *buff = utils_files_parent_pathname(source_pathname, allocator);
        size_t len = strlen(buff);
        DStr parent_rstr = {
            .len = len,
            .buff = buff
        };

        dynarr_insert(&parent_rstr, search_paths);
    }

    if(!raw_search_paths){
        return search_paths;
    }

    size_t a_idx = 0;
    size_t b_idx = 0;
    size_t len = strlen(raw_search_paths);

    for (size_t i = 0; i < len; i++){
        char c = raw_search_paths[i];

        if(c != OS_PATH_SEPARATOR){
            b_idx = i;

            if(i + 1 < len){
                continue;
            }
        }

        size_t buff_len = b_idx - a_idx + 1;
        char *buff = MEMORY_ALLOC(allocator, char, buff_len + 1);

        memcpy(buff, raw_search_paths + a_idx, buff_len);
        buff[buff_len] = 0;

        DStr rstr = (DStr){.len = buff_len, .buff = buff};
        dynarr_insert(&rstr, search_paths);

        a_idx = i + 1;
    }

    return search_paths;
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

    fprintf(stderr, "    --spaths\n");
    fprintf(stderr, "                      Make compiler aware of the paths it must look for imports.\n");
    fprintf(stderr, "                      The paths must be separated by the OS;s paths separator.\n");
    fprintf(stderr, "                      In Windows is ;, while in Linux is :. For example:\n");
    fprintf(stderr, "                          Windows:\n");
    fprintf(stderr, "                              D:\\path\\a;D:\\path\\b;D:\\path\\c\n");
    fprintf(stderr, "                          Linux:\n");
    fprintf(stderr, "                              /path/a:path/b:path/c\n");

    exit(EXIT_FAILURE);
}

static void init_memory(LZArena **out_ctarena, LZFList **out_rtflist, Allocator *ctallocator, Allocator *rtallocator){
    LZFList *rtflist = lzflist_create(NULL);

    if(!rtflist || lzflist_prealloc(DEFAULT_INITIAL_RUNTIME_MEMORY, rtflist)){
        fprintf(stderr, "Failed to init memory");
        lzflist_destroy(rtflist);
        exit(EXIT_FAILURE);
    }

    MEMORY_INIT_ALLOCATOR(
        rtflist,
        lzalloc_link_flist,
        lzrealloc_link_flist,
        lzdealloc_link_flist,
        rtallocator
    );

    LZArena *ctarena = lzarena_create((LZArenaAllocator *)rtallocator);

    lzarena_append_region(DEFAULT_INITIAL_COMPILE_TIME_MEMORY, ctarena);

    MEMORY_INIT_ALLOCATOR(
        ctarena,
        lzalloc_link_arena,
        lzrealloc_link_arena,
        lzdealloc_link_arena,
        ctallocator
    );

    *out_ctarena = ctarena;
    *out_rtflist = rtflist;
}

static LZOHTable *init_default_native_fns(Allocator *rtallocator){
    LZOHTable *native_fns = FACTORY_LZOHTABLE_LEN(64, rtallocator);

    add_native("exit", 1, native_fn_exit, native_fns, rtallocator);
	add_native("assert", 1, native_fn_assert, native_fns, rtallocator);
    add_native("assertm", 2, native_fn_assertm, native_fns, rtallocator);

    add_native("is_str_int", 1, native_fn_is_str_int, native_fns, rtallocator);
    add_native("is_str_float", 1, native_fn_is_str_float, native_fns, rtallocator);
    add_native("to_str", 1, native_fn_to_str, native_fns, rtallocator);
    add_native("to_json", 1, native_fn_to_json, native_fns, rtallocator);
    add_native("to_int", 1, native_fn_to_int, native_fns, rtallocator);
    add_native("to_float", 1, native_fn_to_float, native_fns, rtallocator);

    add_native("print", 1, native_fn_print, native_fns, rtallocator);
    add_native("println", 1, native_fn_println, native_fns, rtallocator);
    add_native("eprint", 1, native_fn_eprint, native_fns, rtallocator);
    add_native("eprintln", 1, native_fn_eprintln, native_fns, rtallocator);
    add_native("print_stack", 0, native_fn_print_stack, native_fns, rtallocator);
    add_native("readln", 0, native_fn_readln, native_fns, rtallocator);

    add_native("gc", 0, native_fn_gc, native_fns, rtallocator);
    add_native("halt", 0, native_fn_halt, native_fns, rtallocator);

    return native_fns;
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
		fprintf(stderr, "File at '%s' do not exists or cannot be read\n", source_pathname);
		exit(EXIT_FAILURE);
	}

	if(!utils_files_is_regular(source_pathname)){
		fprintf(stderr, "File at '%s' is not a regular file\n", source_pathname);
		exit(EXIT_FAILURE);
	}

    LZArena *ctarena = NULL;
    LZFList *rtflist = NULL;
    Allocator ctallocator = {0};
    Allocator rtallocator = {0};

    init_memory(&ctarena, &rtflist, &ctallocator, &rtallocator);

    DynArr *search_paths = parse_search_paths(source_pathname, args.search_paths, &ctallocator);
    LZOHTable *import_paths = FACTORY_LZOHTABLE(&ctallocator);
    LZOHTable *keywords = create_keywords_table(&ctallocator);
	DStr *source = utils_read_source(source_pathname, &ctallocator);
    char *module_path = factory_clone_raw_str(source_pathname, &ctallocator, NULL);

    LZOHTable *modules = FACTORY_LZOHTABLE(&ctallocator);
	DynArr *tokens = FACTORY_DYNARR_PTR(&ctallocator);
    DynArr *fns_prototypes = FACTORY_DYNARR_PTR(&ctallocator);
	DynArr *stmts = FACTORY_DYNARR_PTR(&ctallocator);
	Lexer *lexer = lexer_create(&ctallocator, &rtallocator);
	Parser *parser = parser_create(&ctallocator);
    Compiler *compiler = compiler_create(&ctallocator, &rtallocator);
    Dumpper *dumpper = dumpper_create(&ctallocator);

    LZOHTable *native_fns = init_default_native_fns(&rtallocator);
    Module *module = factory_create_module("main", module_path, &rtallocator);
    VM *vm = vm_create(&rtallocator);

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
        if(parser_parse(tokens, fns_prototypes, stmts, parser)){
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
        if(parser_parse(tokens, fns_prototypes, stmts, parser)){
            result = 1;
			goto CLEAN_UP;
		}
        if(compiler_compile(
            search_paths,
            import_paths,
            keywords,
            native_fns,
            fns_prototypes,
            stmts,
            module,
            modules,
            compiler
        )){
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
        if(parser_parse(tokens, fns_prototypes, stmts, parser)){
            result = 1;
			goto CLEAN_UP;
		}
        if(compiler_compile(
            search_paths,
            import_paths,
            keywords,
            native_fns,
            fns_prototypes,
            stmts,
            module,
            modules,
            compiler
        )){
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
        if(parser_parse(tokens, fns_prototypes, stmts, parser)){
            result = 1;
			goto CLEAN_UP;
		}
        if(compiler_compile(
            search_paths,
            import_paths,
            keywords,
            native_fns,
            fns_prototypes,
            stmts,
            module,
            modules,
            compiler
        )){
            result = 1;
			goto CLEAN_UP;
		}

        lzarena_destroy(ctarena);
        vm_initialize(vm);

        result = vm_execute(native_fns, module, vm);

        goto CLEAN_UP_RUNTIME;
    }

CLEAN_UP:
//    vm_destroy(vm);
    lzarena_destroy(ctarena);
CLEAN_UP_RUNTIME:
    lzflist_destroy(rtflist);

    return result;
}
