#ifndef OPCODE_H
#define OPCODE_H

typedef enum opcode{
    NULL_OPCODE,
    TRUE_OPCODE, FALSE_OPCODE,
    INT_OPCODE,
    
    ADD_OPCODE, SUB_OPCODE,
    MUL_OPCODE, DIV_OPCODE,

    LT_OPCODE, // less
    GT_OPCODE, // greater
    LE_OPCODE, // less equals
    GE_OPCODE, // greater equals
    EQ_OPCODE, // equals
    NE_OPCODE, // not equals

    LSET_OPCODE, LGET_OPCODE,

    PRT_OPCODE,
    POP_OPCODE
}OPCode;

#endif