#include "dumpper.h"
#include "memory.h"
#include "opcode.h"
#include "closure.h"
#include <stdio.h>
#include <assert.h>

#ifdef _WIN32
    #define SIZE_T_FORMAT "%zu"
#elif __linux__
    #define SIZE_T_FORMAT "%ld"
#endif

static int16_t compose_i16(uint8_t *bytes){
    return (int16_t)((uint16_t)bytes[1] << 8) | ((uint16_t)bytes[0]);
}

static int32_t compose_i32(uint8_t *bytes){
    return (int32_t)((uint32_t)bytes[3] << 24) | ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[0]);
}

#define CURRENT_MODULE(d)(d->current_module)
#define CURRENT_SUBMODULE(d)(CURRENT_MODULE(d)->submodule)
#define CURRENT_STRINGS(d)(CURRENT_SUBMODULE(d)->strings)
#define CURRENT_FN(d)(d->current_fn)
#define CURRENT_CONSTANTS(d)(CURRENT_FN(d)->integers)
#define CURRENT_FLOAT_VALUES(d)(CURRENT_FN(d)->floats)
#define CURRENT_CHUNKS(d)(CURRENT_FN(d)->chunks)

static int is_at_end(Dumpper *dumpper){
    DynArr *chunks = CURRENT_CHUNKS(dumpper);
    return dumpper->ip >= chunks->used;
}

static uint8_t advance(Dumpper *dumpper){
    DynArr *chunks = CURRENT_CHUNKS(dumpper);
    return DYNARR_GET_AS(uint8_t, dumpper->ip++, chunks);
}

static int16_t read_i16(Dumpper *dumpper){
    uint8_t bytes[2];

	for(size_t i = 0; i < 2; i++)
		bytes[i] = advance(dumpper);

	return compose_i16(bytes);
}

static int32_t read_i32(Dumpper *dumpper){
	uint8_t bytes[4];

	for(size_t i = 0; i < 4; i++)
		bytes[i] = advance(dumpper);

	return compose_i32(bytes);
}

static int64_t read_i64_const(Dumpper *dumpper){
    DynArr *constants = CURRENT_CONSTANTS(dumpper);
    size_t index = (size_t)read_i16(dumpper);
    return DYNARR_GET_AS(int64_t, index, constants);
}

static double read_float_const(Dumpper *dumpper){
    DynArr *float_values = CURRENT_FLOAT_VALUES(dumpper);
    size_t index = (size_t)read_i16(dumpper);
    return DYNARR_GET_AS(double, index, float_values);
}

static char *read_str(Dumpper *dumpper, uint32_t *out_hash){
    LZHTable *strings = CURRENT_STRINGS(dumpper);
    uint32_t hash = (uint32_t)read_i32(dumpper);
    if(out_hash) *out_hash = hash;
    return lzhtable_hash_get(hash, strings);
}

static void execute(uint8_t chunk, Dumpper *dumpper){
    size_t start = dumpper->ip - 1;
	printf("%.7zu ", start);

    switch (chunk){
        case EMPTY_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7zu\n", "EMPTY", end - start);
            break;
        }case FALSE_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7zu\n", "FALSE", end - start);
            break;
        }case TRUE_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7zu\n", "TRUE", end - start);
            break;
        }case CINT_OPCODE:{
            int64_t value = (int64_t)advance(dumpper);
            size_t end = dumpper->ip;

            printf("%8.8s %.7zu", "CINT", end - start);
            printf(" | value: %zu\n", value);

            break;
        }case INT_OPCODE:{
            int64_t value = read_i64_const(dumpper);
            size_t end = dumpper->ip;

            printf("%8.8s %.7zu", "INT", end - start);
            printf(" | value: %zu\n", value);

            break;
        }case FLOAT_OPCODE:{
			double value = read_float_const(dumpper);
			size_t end = dumpper->ip;

			printf("%8.8s %.7zu", "FLOAT", end - start);
            printf(" | value: %.8f\n", value);

			break;
		}case STRING_OPCODE:{
            uint32_t hash = 0;
            char *value = read_str(dumpper, &hash);
            size_t end = dumpper->ip;

            printf("%8.8s %.7zu", "STRING", end - start);
            printf(" | hash: %u value: '%s'\n", hash, value);

            break;
        }case TEMPLATE_OPCODE:{
            int16_t len = read_i16(dumpper);
            size_t end = dumpper->ip;

            printf("%8.8s %.7zu", "TEMPLATE", end - start);
            printf(" | length: %d\n", len);

            break;
        }case ADD_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7zu\n", "ADD", end - start);
            break;
        }case SUB_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7zu\n", "SUB", end - start);
            break;
        }case MUL_OPCODE:{
            size_t end = dumpper->ip;
			printf("%8.8s %.7zu\n", "MUL", end - start);
            break;
        }case DIV_OPCODE:{
            size_t end = dumpper->ip;
			printf("%8.8s %.7zu\n", "DIV", end - start);
            break;
        }case MOD_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7zu\n", "MOD", end - start);
            break;
		}case BNOT_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7zu\n", "BNOT", end - start);
            break;
		}case LSH_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7zu\n", "LSH", end - start);
            break;
		}case RSH_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7zu\n", "RSH", end - start);
            break;
		}case BAND_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7zu\n", "BAND", end - start);
            break;
		}case BXOR_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7zu\n", "BXOR", end - start);
            break;
		}case BOR_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7zu\n", "BOR", end - start);
            break;
		}case LT_OPCODE:{
            size_t end = dumpper->ip;
			printf("%8.8s %.7zu\n", "LT", end - start);
            break;
        }case GT_OPCODE:{
            size_t end = dumpper->ip;
			printf("%8.8s %.7zu\n", "GT", end - start);
           	break;
        }case LE_OPCODE:{
            size_t end = dumpper->ip;
			printf("%8.8s %.7zu\n", "LE", end - start);
          	break;
        }case GE_OPCODE:{
            size_t end = dumpper->ip;
			printf("%8.8s %.7zu\n", "GE", end - start);
           	break;
        }case EQ_OPCODE:{
            size_t end = dumpper->ip;
			printf("%8.8s %.7zu\n", "EQ", end - start);
           	break;
        }case NE_OPCODE:{
            size_t end = dumpper->ip;
			printf("%8.8s %.7zu\n", "NE", end - start);
          	break;
        }case LSET_OPCODE:{
			uint8_t slot = advance(dumpper);
            size_t end = dumpper->ip;

			printf("%8.8s %.7zu", "LSET", end - start);
            printf(" | slot: %d\n", slot);

            break;
        }case LGET_OPCODE:{
			uint8_t slot = advance(dumpper);
            size_t end = dumpper->ip;

			printf("%8.8s %.7zu", "LGET", end - start);
            printf(" | slot: %d\n", slot);

            break;
        }case OSET_OPCODE:{
            uint8_t slot = advance(dumpper);
            size_t end = dumpper->ip;

			printf("%8.8s %.7zu", "OSET", end - start);
            printf(" | slot: %d\n", slot);

            break;
        }case OGET_OPCODE:{
            uint8_t slot = advance(dumpper);
            size_t end = dumpper->ip;

			printf("%8.8s %.7zu", "OGET", end - start);
            printf(" | slot: %d\n", slot);

            break;
        }case GDEF_OPCODE:{
            char *value = read_str(dumpper, NULL);
            size_t end = dumpper->ip;

            printf("%8.8s %.7zu", "GDEF", end - start);
            printf(" | '%s'\n", value);

            break;
        }case GSET_OPCODE:{
            char *value = read_str(dumpper, NULL);
            size_t end = dumpper->ip;

            printf("%8.8s %.7zu", "GSET", end - start);
            printf(" | '%s'\n", value);

            break;
        }case GGET_OPCODE:{
            char *value = read_str(dumpper, NULL);
            size_t end = dumpper->ip;

            printf("%8.8s %.7zu", "GGET", end - start);
            printf(" | '%s'\n", value);

            break;
        }case GASET_OPCODE:{
            char *value = read_str(dumpper, NULL);
            uint8_t access_type = advance(dumpper);
            size_t end = dumpper->ip;

            printf("%8.8s %.7zu", "GASET", end - start);
            printf(" | '%s' %s\n", value, access_type == 0 ? "private" : "public");

            break;
        }case NGET_OPCODE:{
            char *value = read_str(dumpper, NULL);
            size_t end = dumpper->ip;

            printf("%8.8s %.7zu", "NGET", end - start);
            printf(" | '%s'\n", value);

            break;
        }case SGET_OPCODE:{
            int32_t symbol_index = read_i32(dumpper);
            size_t end = dumpper->ip;

            printf("%8.8s %.7zu", "SGET", end - start);
            printf(" | symbol: '%d'\n", symbol_index);

            break;
        }case ASET_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7zu\n", "ASET", end - start);
            break;
        }case PUT_OPCODE:{
			char *target = read_str(dumpper, NULL);
            size_t end = dumpper->ip;

			printf("%8.8s %.7zu", "PUT", end - start);
            printf(" | target: '%s'\n", target);

            break;
		}case OR_OPCODE:{
            size_t end = dumpper->ip;
			printf("%8.8s %.7zu\n", "OR", end - start);
           	break;
        }case AND_OPCODE:{
            size_t end = dumpper->ip;
			printf("%8.8s %.7zu\n", "AND", end - start);
            break;
        }case NNOT_OPCODE:{
            size_t end = dumpper->ip;
			printf("%8.8s %.7zu\n", "NNOT", end - start);
            break;
        }case NOT_OPCODE:{
            size_t end = dumpper->ip;
			printf("%8.8s %.7zu\n", "NOT", end - start);
          	break;
        }case POP_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7zu\n", "POP", end - start);
            break;
        }case JMP_OPCODE:{
			int16_t value = read_i16(dumpper);
			size_t to = dumpper->ip;
            size_t end = dumpper->ip;

			if(value == 0) to -= 3;
			else if(value > 0) to += value - 1;
			else to += value - 3;

			printf("%8.8s %.7zu", "JMP", end - start);
            printf(" | value: %d to: %zu\n", value, to);

            break;
        }case JIF_OPCODE:{
			int16_t value = read_i16(dumpper);
			size_t to = dumpper->ip;
            size_t end = dumpper->ip;

			if(value == 0) to -= 3;
			else if(value > 0) to += value - 1;
			else to += value - 3;

			printf("%8.8s %.7zu", "JIF", end - start);
            printf(" | value: %d to: %zu\n", value, to);

			break;
		}case JIT_OPCODE:{
            int16_t value = read_i16(dumpper);
			size_t to = dumpper->ip;
            size_t end = dumpper->ip;

			if(value == 0) to -= 3;
			else if(value > 0) to += value - 1;
			else to += value - 3;

			printf("%8.8s %.7zu", "JIT", end - start);
            printf(" | value: %d to: %zu\n", value, to);

			break;
		}case ARRAY_OPCODE:{
			uint8_t parameter = advance(dumpper);
            int32_t index = read_i32(dumpper);
            size_t end = dumpper->ip;

			printf("%8.8s %.7zu", "ARRAY", end - start);
            printf(" | parameter: %d index: %d\n", parameter, index);

            break;
		}case LIST_OPCODE:{
			int16_t len = read_i16(dumpper);
            size_t end = dumpper->ip;

			printf("%8.8s %.7zu", "LIST", end - start);
            printf(" | length: %d\n", len);

            break;
		}case DICT_OPCODE:{
            int16_t len = read_i16(dumpper);
            size_t end = dumpper->ip;

			printf("%8.8s %.7zu", "DICT", end - start);
            printf(" | length: %d\n", len);

            break;
        }case RECORD_OPCODE:{
			uint8_t len = advance(dumpper);

            for (uint8_t i = 0; i < len; i++)
                read_str(dumpper, NULL);

            size_t end = dumpper->ip;

			printf("%8.8s %.7zu", "RECORD", end - start);
            printf(" | length: %d\n", len);

            break;
		}case CALL_OPCODE:{
            uint8_t args_count = advance(dumpper);
            size_t end = dumpper->ip;

            printf("%8.8s %.7zu", "CALL", end - start);
            printf(" | arguments: %d\n", args_count);

            break;
        }case ACCESS_OPCODE:{
            char *symbol = read_str(dumpper, NULL);
            size_t end = dumpper->ip;

            printf("%8.8s %.7zu", "ACCESS", end - start);
            printf(" | value: %s\n", symbol);

            break;
        }case INDEX_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7zu\n", "INDEX", end - start);
            break;
        }case RET_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7zu\n", "RET", end - start);
            break;
        }case IS_OPCODE:{
			uint8_t type = advance(dumpper);
            size_t end = dumpper->ip;

			printf("%8.8s %.7zu", "IS", end - start);
            printf(" | type: %d\n", type);

            break;
		}case THROW_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7zu\n", "THROW", end - start);
            break;
        }case LOAD_OPCODE:{
            char *path = read_str(dumpper, NULL);
            size_t end = dumpper->ip;

            printf("%8.8s %.7zu", "LOAD", end - start);
            printf(" | path: %s\n", path);

            break;
        }default:{
            assert("Illegal opcode\n");
        }
    }
}

static void dump_function(Fn *function, Dumpper *dumpper){
    dumpper->ip = 0;
    dumpper->current_fn = function;

    printf("    Function '%s':\n", function->name);

    char empty = 1;

    while(!is_at_end(dumpper)){
        if(empty) empty = 0;

        printf("        ");
        execute(advance(dumpper), dumpper);
    }

    if(empty)
        printf("        ***empty***\n");
}

static void dump_module(Module *module, Dumpper *dumpper){
    printf("Module '%s':\n", module->name);

    SubModule *submodule = module->submodule;
    Module *prev = dumpper->current_module;

    dumpper->current_module = module;

    DynArr *symbols = submodule->symbols;

    for (size_t i = 0; i < DYNARR_LEN(symbols); i++){
        SubModuleSymbol symbol = DYNARR_GET_AS(SubModuleSymbol, i, symbols);

        if(symbol.type == FUNCTION_MSYMTYPE){
            Fn *fn = symbol.value;
            dump_function(fn, dumpper);
        }

        if(symbol.type == CLOSURE_MSYMTYPE){
            MetaClosure *meta_closure = symbol.value;
            Fn *fn = meta_closure->fn;
            dump_function(fn, dumpper);
        }
    }

    dumpper->current_module = prev;
}

Dumpper *dumpper_create(Allocator *allocator){
	Dumpper *dumpper = MEMORY_ALLOC(Dumpper, 1, allocator);
    if(!dumpper){return NULL;}
	memset(dumpper, 0, sizeof(Dumpper));
    dumpper->allocator = allocator;
	return dumpper;
}

void dumpper_dump(LZHTable *modules, Module *main_module, Dumpper *dumpper){
	memset(dumpper, 0, sizeof(Dumpper));

    dumpper->ip = 0;
    dumpper->modules = modules;
    dumpper->main_module = main_module;
    dumpper->current_fn = NULL;

    dump_module(main_module, dumpper);

    LZHTableNode *node = modules->head;

    while (node){
        LZHTableNode *next = node->next_table_node;
        Module *module = (Module *)node->value;

        dump_module(module, dumpper);

        node = next;
    }
}