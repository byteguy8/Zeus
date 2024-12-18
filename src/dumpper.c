#include "dumpper.h"
#include "memory.h"
#include "types.h"
#include "opcode.h"
#include <stdio.h>
#include <assert.h>

static int32_t compose_i32(uint8_t *bytes){
    return ((int32_t)bytes[3] << 24) | ((int32_t)bytes[2] << 16) | ((int32_t)bytes[1] << 8) | ((int32_t)bytes[0]);
}

static int is_at_end(Dumpper *dumpper){
    DynArr *chunks = dumpper->chunks;
    return dumpper->ip >= chunks->used;
}

static uint8_t advance(Dumpper *dumpper){
    DynArr *chunks = dumpper->chunks;
    return DYNARR_GET_AS(uint8_t, dumpper->ip++, chunks);
}

static int32_t read_i32(Dumpper *dumpper){
	uint8_t bytes[4];

	for(size_t i = 0; i < 4; i++)
		bytes[i] = advance(dumpper);

	return compose_i32(bytes);
}

static int64_t read_i64_const(Dumpper *dumpper){
    Module *module = dumpper->current_module;
    DynArr *constants = module->constants;
    int32_t index = read_i32(dumpper);
    return DYNARR_GET_AS(int64_t, index, constants);
}

static char *read_str(Dumpper *dumpper, uint32_t *out_hash){
    Module *module = dumpper->current_module;
    uint32_t hash = (uint32_t)read_i32(dumpper);
    if(out_hash) *out_hash = hash;
    return lzhtable_hash_get(hash, module->strings);
}

static void execute(uint8_t chunk, Dumpper *dumpper){
	printf("%.7ld ", dumpper->ip - 1);

    switch (chunk){
        case EMPTY_OPCODE:{
            printf("NULL_OPCODE\n");
            break;
        }
        case FALSE_OPCODE:{
            printf("FALSE_OPCODE\n");
            break;
        }
        case TRUE_OPCODE:{
            printf("TRUE_OPCODE\n");
            break;
        }
        case INT_OPCODE:{
            int64_t i64 = read_i64_const(dumpper);
            printf("INT_OPCODE value: %ld\n", i64);
            break;
        }
        case STRING_OPCODE:{
            uint32_t hash = 0;
            char *str = read_str(dumpper, &hash);
            printf("STRING_OPCODE hash: %u value: '%s'\n", hash, str);
            break;
        }
        case ADD_OPCODE:{
            printf("ADD_OPCODE\n");
            break;
        }
        case SUB_OPCODE:{
            printf("SUB_OPCODE\n");
            break;
        }
        case MUL_OPCODE:{
			printf("MUL_OPCODE\n");
            break;
        }
        case DIV_OPCODE:{
			printf("DIV_OPCODE\n");
            break;
        }
        case MOD_OPCODE:{
            printf("MOD_OPCODE\n");
            break;
		}
        case LT_OPCODE:{
			printf("LT_OPCODE\n");
            break;
        }
        case GT_OPCODE:{
			printf("GT_OPCODE\n");
           	break;
        }
        case LE_OPCODE:{
			printf("LE_OPCODE\n");
          	break;
        }
        case GE_OPCODE:{
			printf("GE_OPCODE\n");
           	break;
        }
        case EQ_OPCODE:{
			printf("EQ_OPCODE\n");
           	break;
        }
        case NE_OPCODE:{
			printf("NE_OPCODE\n");
          	break;
        }
        case LSET_OPCODE:{
			uint8_t slot = advance(dumpper);
			printf("LSET_OPCODE %d\n", slot);
           	break;
        }
        case LGET_OPCODE:{
			uint8_t slot = advance(dumpper);
			printf("LGET_OPCODE slot: %d\n", slot);
           	break;
        }
        case GSET_OPCODE:{
            uint32_t hash = 0;
            char *str = read_str(dumpper, &hash);
            printf("GSET_OPCODE hash: '%u' value: '%s'\n", hash, str);
            break;
        }
        case GGET_OPCODE:{
            uint32_t hash = 0;
            char *str = read_str(dumpper, &hash);
            printf("GGET_OPCODE hash: '%u' value: '%s'\n", hash, str);
            break;
        }
        case NGET_OPCODE:{
            uint32_t hash = 0;
            char *value = read_str(dumpper, &hash);
            printf("NGET_OPCODE hash: %u value: '%s'\n", hash, value);
            break;
        }
        case SGET_OPCODE:{
            uint32_t hash = 0;
            char *value = read_str(dumpper, &hash);
            printf("SGET_OPCODE hash: %u value: '%s'\n", hash, value);
            break;
        }
		case PUT_OPCODE:{
			char *target = read_str(dumpper, NULL);
			printf("PUT_OPCODE target: %s\n", target);
			break;
		}
        case OR_OPCODE:{
			printf("OR_OPCODE\n");
           	break;
        }
        case AND_OPCODE:{
			printf("AND_OPCODE\n");
            break;
        }
        case NNOT_OPCODE:{
			printf("NNOT_OPCODE\n");
            break;
        }
        case NOT_OPCODE:{
			printf("NOT_OPCODE\n");
          	break;
        }
        case PRT_OPCODE:{
			printf("PRT_OPCODE\n");
            break;
        }
        case POP_OPCODE:{
            printf("POP\n");
            break;
        }
        case JMP_OPCODE:{
			int32_t jmp_value = read_i32(dumpper);
			size_t ip = dumpper->ip;

			if(jmp_value == 0) ip -= 5;
			else if(jmp_value > 0) ip += jmp_value - 1;
			else ip += jmp_value - 5; 

			printf("JMP_OPCODE value: %d, to: %ld\n", jmp_value, ip);

            break;
        }
		case JIF_OPCODE:{
			int32_t jmp_value = read_i32(dumpper);
			size_t ip = dumpper->ip;

			if(jmp_value == 0) ip -= 5;
			else if(jmp_value > 0) ip += jmp_value - 1;
			else ip += jmp_value - 5;

			printf("JIF_OPCODE value: %d, to: %ld\n", jmp_value, ip);

			break;
		}
		case JIT_OPCODE:{
			int32_t jmp_value = read_i32(dumpper);
			size_t ip = dumpper->ip;

			if(jmp_value == 0) ip -= 5;
			else if(jmp_value > 0) ip += jmp_value - 1;
			else ip += jmp_value - 5;

			printf("JIT_OPCODE value: %d, to: %ld\n", jmp_value, ip);

			break;
		}
		case LIST_OPCODE:{
			int32_t len = read_i32(dumpper);
			printf("LIST_OPCODE len: %d\n", len);
			break;
		}
        case DICT_OPCODE:{
            int32_t len = read_i32(dumpper);
			printf("DICT_OPCODE len: %d\n", len);
			break;
        }
		case RECORD_OPCODE:{
			uint8_t len = advance(dumpper);
			printf("RECORD_OPCODE len: %d\n", len);
			break;
		}
        case CALL_OPCODE:{
            uint8_t args_count = advance(dumpper);
            printf("CALL_OPCODE args: %d\n", args_count);
            break;
        }
        case ACCESS_OPCODE:{
            char *symbol = read_str(dumpper, NULL);
            printf("ACCESS_OPCODE symbol: %s\n", symbol);
            break;
        }
        case RET_OPCODE:{
            printf("RET_OPCODE\n");
            break;
        }
		case IS_OPCODE:{
			uint8_t type = advance(dumpper);
			printf("IS_OPCODE value: %d\n", type);
			break;
		}
        case THROW_OPCODE:{
            printf("THROW_OPCODE\n");
            break;
        }
        default:{
            assert("Illegal opcode\n");
        }
    }
}

static void dump_function(Fn *function, Dumpper *dumpper){
    DynArr *chunks = function->chunks;
    
    dumpper->ip = 0;
    dumpper->chunks = chunks;

    printf("Dumpping '%s':\n", function->name);

    while(!is_at_end(dumpper))
		execute(advance(dumpper), dumpper);
}

static void dump_module(Module *module, Dumpper *dumpper){
    printf("Dumpping Module '%s'\n", module->name);

    Module *prev = dumpper->current_module;
    dumpper->current_module = module;

    LZHTableNode *node = module->symbols->head;
    
    while (node){
        LZHTableNode *next = node->next_table_node;
        ModuleSymbol *symbol = (ModuleSymbol *)node->value;
        
        if(symbol->type == FUNCTION_MSYMTYPE){
            Fn *fn = symbol->value.fn;
            dump_function(fn, dumpper);
        }

        if(symbol->type == MODULE_MSYMTYPE){
            Module *m = symbol->value.module;
            dump_module(m, dumpper);
        }

        node = next;
    }

    dumpper->current_module = prev;
}

Dumpper *dumpper_create(){
	Dumpper *dumpper = (Dumpper *)A_COMPILE_ALLOC(sizeof(Dumpper));
	memset(dumpper, 0, sizeof(Dumpper));
	return dumpper;
}

void dumpper_dump(Module *module, Dumpper *dumpper){
	memset(dumpper, 0, sizeof(Dumpper));

    dumpper->ip = 0;
    dumpper->module = module;
    dumpper->current_module = module;

    dump_module(module, dumpper);
}
