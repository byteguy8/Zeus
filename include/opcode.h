#ifndef OPCODE_H
#define OPCODE_H

typedef enum opcode{
    EMPTY_OPCODE,
    TRUE_OPCODE, FALSE_OPCODE,
    INT_OPCODE, STRING_OPCODE,
    
    // arithmeric
    ADD_OPCODE, SUB_OPCODE,
    MUL_OPCODE, DIV_OPCODE,
	MOD_OPCODE,

    // comparison
    LT_OPCODE, // less
    GT_OPCODE, // greater
    LE_OPCODE, // less equals
    GE_OPCODE, // greater equals
    EQ_OPCODE, // equals
    NE_OPCODE, // not equals

    // logical
    OR_OPCODE, AND_OPCODE,
    NOT_OPCODE, NNOT_OPCODE,

    LSET_OPCODE, LGET_OPCODE,
    GSET_OPCODE, GGET_OPCODE,
    SGET_OPCODE,

    PRT_OPCODE,
    POP_OPCODE,

    JMP_OPCODE,
	JIF_OPCODE,
	JIT_OPCODE,

	LIST_OPCODE,
    DICT_OPCODE,
    CALL_OPCODE,
    RET_OPCODE,
    ACCESS_OPCODE,
}OPCode;

#endif
