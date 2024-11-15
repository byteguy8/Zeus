#include "dumpper.h"
#include "memory.h"
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
    return *(uint8_t *)dynarr_get(dumpper->ip++, chunks);
}

static int32_t read_i32(Dumpper *dumpper){
	uint8_t bytes[4];

	for(size_t i = 0; i < 4; i++)
		bytes[i] = advance(dumpper);

	return compose_i32(bytes);
}

static int64_t read_i64_const(Dumpper *dumpper){
    DynArr *constants = dumpper->constants;
    int32_t index = read_i32(dumpper);
    return *(int64_t *)dynarr_get(index, constants);
}

static void execute(uint8_t chunk, Dumpper *dumpper){
	printf("%.7ld ", dumpper->ip - 1);

    switch (chunk){
        case NULL_OPCODE:{
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
			printf("LGET_OPCODE %d\n", slot);
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

			if(jmp_value == 0) break;

			size_t ip = dumpper->ip;
			if(jmp_value > 0) ip += jmp_value - 1;
			else ip += jmp_value - 5; 

			printf("JMP_OPCODE value: %d, to: %ld\n", jmp_value, ip);

            break;
        }
		case JIF_OPCODE:{
			int32_t jmp_value = read_i32(dumpper);

			if(jmp_value == 0) break;

			size_t ip = dumpper->ip;
			if(jmp_value > 0) ip += jmp_value - 1;
			else ip += jmp_value - 5;

			printf("JIF_OPCODE value: %d, to: %ld\n", jmp_value, ip);

			break;
		}
		case JIT_OPCODE:{
			int32_t jmp_value = read_i32(dumpper);

			if(jmp_value == 0) break;

			size_t ip = dumpper->ip;
			if(jmp_value > 0) ip += jmp_value - 1;
			else ip += jmp_value - 5;

			printf("JIT_OPCODE value: %d, to: %ld\n", jmp_value, ip);

			break;
		}
        default:{
            assert("Illegal opcode\n");
        }
    }
}

Dumpper *dumpper_create(){
	Dumpper *dumpper = (Dumpper *)memory_alloc(sizeof(Dumpper));
	memset(dumpper, 0, sizeof(Dumpper));
	return dumpper;
}

void dumpper_dump(DynArr *constants, DynArr *chunks, Dumpper *dumpper){
	dumpper->ip = 0;
	dumpper->constants = constants;
	dumpper->chunks = chunks;

	while(!is_at_end(dumpper))
		execute(advance(dumpper), dumpper);
}