#ifndef OPCODE_H
#define OPCODE_H

typedef enum opcode{
    TRUE_OPCODE,
    FALSE_OPCODE,
    INT_OPCODE,
    
    ADD_OPCODE,
    SUB_OPCODE,
    MUL_OPCODE,
    DIV_OPCODE,

    PRT_OPCODE,
    POP_OPCODE
}OPCode;

#endif