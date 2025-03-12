#include "dumpper.h"
#include "memory.h"
#include "rtypes.h"
#include "opcode.h"
#include <stdio.h>
#include <assert.h>

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
	printf("%.7ld ", start);

    switch (chunk){
        case EMPTY_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7ld\n", "EMPTY", end - start);
            break;
        }case FALSE_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7ld\n", "FALSE", end - start);
            break;
        }case TRUE_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7ld\n", "TRUE", end - start);
            break;
        }case CINT_OPCODE:{
            int64_t value = (int64_t)advance(dumpper);
            size_t end = dumpper->ip;

            printf("%8.8s %.7ld", "CINT", end - start);
            printf(" | value: %ld\n", value);

            break;
        }case INT_OPCODE:{
            int64_t value = read_i64_const(dumpper);
            size_t end = dumpper->ip;

            printf("%8.8s %.7ld", "INT", end - start);
            printf(" | value: %ld\n", value);
            
            break;
        }case FLOAT_OPCODE:{
			double value = read_float_const(dumpper);
			size_t end = dumpper->ip;
			
			printf("%8.8s %.7ld", "FLOAT", end - start);
            printf(" | value: %.8f\n", value);
			
			break;
		}case STRING_OPCODE:{
            uint32_t hash = 0;
            char *value = read_str(dumpper, &hash);
            size_t end = dumpper->ip;

            printf("%8.8s %.7ld", "STRING", end - start);
            printf(" | hash: %u value: '%s'\n", hash, value);
            
            break;
        }case TEMPLATE_OPCODE:{
            int16_t len = read_i16(dumpper);
            size_t end = dumpper->ip;

            printf("%8.8s %.7ld", "TEMPLATE", end - start);
            printf(" | length: %d\n", len);

            break;
        }case ADD_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7ld\n", "ADD", end - start);
            break;
        }case SUB_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7ld\n", "SUB", end - start);
            break;
        }case MUL_OPCODE:{
            size_t end = dumpper->ip;
			printf("%8.8s %.7ld\n", "MUL", end - start);
            break;
        }case DIV_OPCODE:{
            size_t end = dumpper->ip;
			printf("%8.8s %.7ld\n", "DIV", end - start);
            break;
        }case MOD_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7ld\n", "MOD", end - start);
            break;
		}case LT_OPCODE:{
            size_t end = dumpper->ip;
			printf("%8.8s %.7ld\n", "LT", end - start);
            break;
        }case GT_OPCODE:{
            size_t end = dumpper->ip;
			printf("%8.8s %.7ld\n", "GT", end - start);
           	break;
        }case LE_OPCODE:{
            size_t end = dumpper->ip;
			printf("%8.8s %.7ld\n", "LE", end - start);
          	break;
        }case GE_OPCODE:{
            size_t end = dumpper->ip;
			printf("%8.8s %.7ld\n", "GE", end - start);
           	break;
        }case EQ_OPCODE:{
            size_t end = dumpper->ip;
			printf("%8.8s %.7ld\n", "EQ", end - start);
           	break;
        }case NE_OPCODE:{
            size_t end = dumpper->ip;
			printf("%8.8s %.7ld\n", "NE", end - start);
          	break;
        }case LSET_OPCODE:{
			uint8_t slot = advance(dumpper);
            size_t end = dumpper->ip;

			printf("%8.8s %.7ld", "LSET", end - start);
            printf(" | slot: %d\n", slot);
           	
            break;
        }case LGET_OPCODE:{
			uint8_t slot = advance(dumpper);
            size_t end = dumpper->ip;

			printf("%8.8s %.7ld", "LGET", end - start);
            printf(" | slot: %d\n", slot);
           	
            break;
        }case OSET_OPCODE:{
            uint8_t slot = advance(dumpper);
            size_t end = dumpper->ip;

			printf("%8.8s %.7ld", "OSET", end - start);
            printf(" | slot: %d\n", slot);
           	
            break;
        }case OGET_OPCODE:{
            uint8_t slot = advance(dumpper);
            size_t end = dumpper->ip;

			printf("%8.8s %.7ld", "OGET", end - start);
            printf(" | slot: %d\n", slot);
           	
            break;
        }case GDEF_OPCODE:{
            uint32_t hash = 0;
            char *value = read_str(dumpper, &hash);
            size_t end = dumpper->ip;

            printf("%8.8s %.7ld", "GDEF", end - start);
            printf(" | %u '%s'\n", hash, value);

            break;
        }case GSET_OPCODE:{
            uint32_t hash = 0;
            char *value = read_str(dumpper, &hash);
            size_t end = dumpper->ip;

            printf("%8.8s %.7ld", "GSET", end - start);
            printf(" | hash: %u value: '%s'\n", hash, value);
            
            break;
        }case GGET_OPCODE:{
            uint32_t hash = 0;
            char *value = read_str(dumpper, &hash);
            size_t end = dumpper->ip;

            printf("%8.8s %.7ld", "GGET", end - start);
            printf(" | hash: %u value: '%s'\n", hash, value);
            
            break;
        }case GASET_OPCODE:{
            char *value = read_str(dumpper, NULL);
            uint8_t access_type = advance(dumpper);
            size_t end = dumpper->ip;

            printf("%8.8s %.7ld", "GASET", end - start);
            printf(" | '%s' %s\n", value, access_type == 0 ? "private" : "public");
            
            break;
        }case NGET_OPCODE:{
            uint32_t hash = 0;
            char *value = read_str(dumpper, &hash);
            size_t end = dumpper->ip;

            printf("%8.8s %.7ld", "NGET", end - start);
            printf(" | hash: %u value: '%s'\n", hash, value);
            
            break;
        }case SGET_OPCODE:{
            int32_t symbol_index = read_i32(dumpper);
            size_t end = dumpper->ip;

            printf("%8.8s %.7ld", "SGET", end - start);
            printf(" | symbol: '%d'\n", symbol_index);
            
            break;
        }case ASET_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7ld\n", "ASET", end - start);
            break;
        }case PUT_OPCODE:{
			char *target = read_str(dumpper, NULL);
            size_t end = dumpper->ip;

			printf("%8.8s %.7ld", "PUT", end - start);
            printf(" | target: '%s'\n", target);
			
            break;
		}case OR_OPCODE:{
            size_t end = dumpper->ip;
			printf("%8.8s %.7ld\n", "OR", end - start);
           	break;
        }case AND_OPCODE:{
            size_t end = dumpper->ip;
			printf("%8.8s %.7ld\n", "AND", end - start);
            break;
        }case NNOT_OPCODE:{
            size_t end = dumpper->ip;
			printf("%8.8s %.7ld\n", "NNOT", end - start);
            break;
        }case NOT_OPCODE:{
            size_t end = dumpper->ip;
			printf("%8.8s %.7ld\n", "NOT", end - start);
          	break;
        }case POP_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7ld\n", "POP", end - start);
            break;
        }case JMP_OPCODE:{
			int16_t value = read_i16(dumpper);
			size_t to = dumpper->ip;
            size_t end = dumpper->ip;

			if(value == 0) to -= 3;
			else if(value > 0) to += value - 1;
			else to += value - 3;

			printf("%8.8s %.7ld", "JMP", end - start);
            printf(" | value: %d to: %ld\n", value, to);

            break;
        }case JIF_OPCODE:{
			int16_t value = read_i16(dumpper);
			size_t to = dumpper->ip;
            size_t end = dumpper->ip;

			if(value == 0) to -= 3;
			else if(value > 0) to += value - 1;
			else to += value - 3;

			printf("%8.8s %.7ld", "JIF", end - start);
            printf(" | value: %d to: %ld\n", value, to);

			break;
		}case JIT_OPCODE:{
            int16_t value = read_i16(dumpper);
			size_t to = dumpper->ip;
            size_t end = dumpper->ip;

			if(value == 0) to -= 3;
			else if(value > 0) to += value - 1;
			else to += value - 3;

			printf("%8.8s %.7ld", "JIT", end - start);
            printf(" | value: %d to: %ld\n", value, to);

			break;
		}case ARRAY_OPCODE:{
			uint8_t parameter = advance(dumpper);
            int32_t index = read_i32(dumpper);
            size_t end = dumpper->ip;

			printf("%8.8s %.7ld", "ARRAY", end - start);
            printf(" | parameter: %d index: %d\n", parameter, index);
			
            break;
		}case LIST_OPCODE:{
			int16_t len = read_i16(dumpper);
            size_t end = dumpper->ip;

			printf("%8.8s %.7ld", "LIST", end - start);
            printf(" | length: %d\n", len);
			
            break;
		}case DICT_OPCODE:{
            int16_t len = read_i16(dumpper);
            size_t end = dumpper->ip;

			printf("%8.8s %.7ld", "DICT", end - start);
            printf(" | length: %d\n", len);
			
            break;
        }case RECORD_OPCODE:{
			uint8_t len = advance(dumpper);
            
            for (uint8_t i = 0; i < len; i++)
                read_str(dumpper, NULL);

            size_t end = dumpper->ip;

			printf("%8.8s %.7ld", "RECORD", end - start);
            printf(" | length: %d\n", len);
		
            break;
		}case CALL_OPCODE:{
            uint8_t args_count = advance(dumpper);
            size_t end = dumpper->ip;

            printf("%8.8s %.7ld", "CALL", end - start);
            printf(" | arguments: %d\n", args_count);
            
            break;
        }case ACCESS_OPCODE:{
            char *symbol = read_str(dumpper, NULL);
            size_t end = dumpper->ip;

            printf("%8.8s %.7ld", "ACCESS", end - start);
            printf(" | value: %s\n", symbol);
            
            break;
        }case INDEX_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7ld\n", "INDEX", end - start);
            break;
        }case RET_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7ld\n", "RET", end - start);
            break;
        }case IS_OPCODE:{
			uint8_t type = advance(dumpper);
            size_t end = dumpper->ip;

			printf("%8.8s %.7ld", "IS", end - start);
            printf(" | type: %d\n", type);
			
            break;
		}case THROW_OPCODE:{
            size_t end = dumpper->ip;
            printf("%8.8s %.7ld\n", "THROW", end - start);
            break;
        }case LOAD_OPCODE:{
            char *path = read_str(dumpper, NULL);
            size_t end = dumpper->ip;
            
            printf("%8.8s %.7ld", "LOAD", end - start);
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

    printf("    Dumpping Function '%s':\n", function->name);

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
    printf("Dumpping Module '%s':\n", module->name);

    SubModule *submodule = module->submodule;
    Module *prev = dumpper->current_module;
    
    dumpper->current_module = module;

    DynArr *symbols = submodule->symbols;
    
    for (size_t i = 0; i < DYNARR_LEN(symbols); i++){
        SubModuleSymbol symbol = DYNARR_GET_AS(SubModuleSymbol, i, symbols);
        
        if(symbol.type == FUNCTION_MSYMTYPE){
            Fn *fn = symbol.value.fn;
            dump_function(fn, dumpper);
        }

        if(symbol.type == CLOSURE_MSYMTYPE){
            MetaClosure *meta_closure = symbol.value.meta_closure;
            Fn *fn = meta_closure->fn;
            dump_function(fn, dumpper);
        }
    }

    dumpper->current_module = prev;
}

Dumpper *dumpper_create(){
	Dumpper *dumpper = (Dumpper *)A_COMPILE_ALLOC(sizeof(Dumpper));
	memset(dumpper, 0, sizeof(Dumpper));
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
