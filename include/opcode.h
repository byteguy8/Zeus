#ifndef OPCODE_H
#define OPCODE_H

typedef enum opcode{
    NULL_OPCODE,
    TRUE_OPCODE, FALSE_OPCODE,
    INT_OPCODE,
    
    // arithmeric
    ADD_OPCODE, SUB_OPCODE,
    MUL_OPCODE, DIV_OPCODE,

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

    PRT_OPCODE,
    POP_OPCODE,

    JMP_OPCODE,
	JIF_OPCODE
}OPCode;

#endif
