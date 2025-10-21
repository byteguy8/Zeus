#include "dumpper.h"
#include "memory.h"
#include "opcode.h"
#include "closure.h"
#include <stdio.h>
#include <assert.h>
#include <inttypes.h>

static int16_t compose_i16(uint8_t *bytes){
    return (int16_t)((uint16_t)bytes[1] << 8) | ((uint16_t)bytes[0]);
}

static int32_t compose_i32(uint8_t *bytes){
    return (int32_t)((uint32_t)bytes[3] << 24) | ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[0]);
}

#define CURRENT_MODULE(d)(d->current_module)
#define CURRENT_SUBMODULE(d)(CURRENT_MODULE(d)->submodule)
#define CURRENT_STRINGS(d)(CURRENT_SUBMODULE(d)->static_strs)
#define CURRENT_FN(d)(d->current_fn)
#define CURRENT_CONSTANTS(d)(CURRENT_FN(d)->iconsts)
#define CURRENT_FLOAT_VALUES(d)(CURRENT_FN(d)->fconsts)
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
    size_t idx = (size_t)read_i16(dumpper);
    return DYNARR_GET_AS(double, idx, float_values);
}

static char *read_str(Dumpper *dumpper, size_t *out_len){
    DynArr *static_strs = CURRENT_STRINGS(dumpper);
    size_t idx = (size_t)read_i16(dumpper);
    RawStr str = DYNARR_GET_AS(RawStr, idx, static_strs);

    if(out_len){
        *out_len = str.len;
    }

    return str.buff;
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
            size_t len = 0;
            char *raw_str = read_str(dumpper, &len);
            size_t end = dumpper->ip;

            printf("%8.8s %.7zu", "STRING", end - start);
            printf(" | '");

            for (size_t i = 0; i < len; i++){
                char c = raw_str[i];

                if(i == 16){
                    printf("...");
                    break;
                }

                if(c == '\n'){
                    continue;
                }

                printf("%c", c);
            }

            printf("'\n");

            break;
        }case STTE_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7zu\n", "STTE", end - start);
            break;
        }case ETTE_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7zu\n", "ETTE", end - start);
            break;
        }case CONCAT_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7zu\n", "CONCAT", end - start);
            break;
        }case MULSTR_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7zu\n", "MULSTR", end - start);
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
        }case RSET_OPCODE:{
			char *target = read_str(dumpper, NULL);
            size_t end = dumpper->ip;

			printf("%8.8s %.7zu", "RSET", end - start);
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
            size_t end = dumpper->ip;
			printf("%8.8s %.7zu\n", "ARRAY", end - start);
            break;
		}case LIST_OPCODE:{
            size_t end = dumpper->ip;
			printf("%8.8s %.7zu\n", "LIST", end - start);
            break;
		}case DICT_OPCODE:{
            size_t end = dumpper->ip;
			printf("%8.8s %.7zu\n", "DICT", end - start);
            break;
        }case RECORD_OPCODE:{
			uint16_t len = (uint16_t)read_i16(dumpper);
            size_t end = dumpper->ip;

			printf("%8.8s %.7zu", "RECORD", end - start);
            printf(" | length: %" PRIu16 "\n", len);

            break;
		}case WTTE_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7zu\n", "WTTE", end - start);
            break;
        }case IARRAY_OPCODE:{
            int16_t idx = read_i16(dumpper);
            size_t end = dumpper->ip;

            printf("%8.8s %.7zu", "IARRAY", end - start);
            printf(" | at: %" PRId16 "\n", idx);

            break;
        }case ILIST_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7zu\n", "ILIST", end - start);
            break;
        }case IDICT_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7zu\n", "IDICT", end - start);
            break;
        }case IRECORD_OPCODE:{
            char *key = read_str(dumpper, NULL);
            size_t end = dumpper->ip;

            printf("%8.8s %.7zu", "IRECORD", end - start);
            printf(" | key: '%s'\n", key);

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
		}case TRYO_OPCODE:{
            size_t catch_ip = (size_t)read_i16(dumpper);
            size_t end = dumpper->ip;
            printf("%8.8s %.7zu", "TRYO", end - start);
            printf(" | catch_ip: %zu\n", catch_ip);
            break;
        }case TRYC_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7zu\n", "TRYC", end - start);
            break;
        }case THROW_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7zu\n", "THROW", end - start);
            break;
        }case HLT_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7zu\n", "HLT", end - start);
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

        if(symbol.type == FUNCTION_SUBMODULE_SYM_TYPE){
            Fn *fn = symbol.value;
            dump_function(fn, dumpper);
        }

        if(symbol.type == CLOSURE_SUBMODULE_SYM_TYPE){
            MetaClosure *meta_closure = symbol.value;
            Fn *fn = meta_closure->fn;
            dump_function(fn, dumpper);
        }
    }

    dumpper->current_module = prev;
}

Dumpper *dumpper_create(Allocator *allocator){
	Dumpper *dumpper = MEMORY_ALLOC(allocator, Dumpper, 1);

    if(!dumpper){
        return NULL;
    }

	memset(dumpper, 0, sizeof(Dumpper));
    dumpper->allocator = allocator;

	return dumpper;
}

void dumpper_dump(LZOHTable *modules, Module *main_module, Dumpper *dumpper){
	memset(dumpper, 0, sizeof(Dumpper));

    dumpper->ip = 0;
    dumpper->modules = modules;
    dumpper->main_module = main_module;
    dumpper->current_fn = NULL;

    dump_module(main_module, dumpper);

    size_t m = modules->m;

    for (size_t i = 0; i < m; i++){
        LZOHTableSlot slot = modules->slots[i];

        if(!slot.used){
            continue;
        }

        Module *module = (Module *)slot.value;

        dump_module(module, dumpper);
    }
}
