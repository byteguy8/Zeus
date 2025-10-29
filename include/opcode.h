#ifndef OPCODE_H
#define OPCODE_H

typedef enum opcode{
    // PRIMITIVES
    OP_EMPTY,    // push NULL value equivalent
    OP_FALSE,    // push FALSE value
    OP_TRUE,     // push TRUE value
    OP_CINT,     // push integer of 1 byte
    OP_INT,      // push integer of 8 bytes
    OP_FLOAT,    // push float
    OP_STRING,   // push string
    OP_STTE,     // start constructing template
    OP_ETTE,     // end constructing template and push it
    OP_ARRAY,
	OP_LIST,
    OP_DICT,
	OP_RECORD,

    OP_WTTE,
    OP_IARRAY,
    OP_ILIST,
    OP_IDICT,
    OP_IRECORD,

    OP_CONCAT,
    OP_MULSTR,

    // ARITHMETIC
    OP_ADD,    // add two integers or floats and push the result
    OP_SUB,    // subtract two integers or floats and push the result
    OP_MUL,    // muliply two integers or floats and push the result
    OP_DIV,    // divide two integers or floats and push the result
	OP_MOD,    // calculate module from two integers and push the result

    // BITWISE
    OP_BNOT,    // not bitwise
    OP_LSH,     // left shift
    OP_RSH,     // right shift
    OP_BAND,    // and bitwise
    OP_BXOR,    // xor bitwise
    OP_BOR,     // or bitwise

    // COMPARISON
    OP_LT,    // less
    OP_GT,    // greater
    OP_LE,    // less equals
    OP_GE,    // greater equals
    OP_EQ,    // equals
    OP_NE,    // not equals

    // LOGICAL
    OP_OR,
    OP_AND,
    OP_NOT,
    OP_NNOT,

    OP_LSET,    // set local symbol
    OP_LGET,    // get local symbol
    OP_OSET,    // set out value symbol (for closures)
    OP_OGET,    // get out value symbol (for closures)
    OP_GDEF,    // define a global symbol
    OP_GASET,   // set global symbol access
    OP_GSET,    // get global symbol
    OP_GGET,    // set global symbol
    OP_NGET,    // get native symbol
    OP_SGET,    // get symbol from list of symbols
	OP_ASET,    // set a value inside array
    OP_RSET,    // set a value inside record

    OP_POP,

    OP_JMP,
	OP_JIF,
	OP_JIT,

    OP_CALL,
    OP_ACCESS,
    OP_INDEX,
    OP_RET,
	OP_IS,
    OP_TRYO,
    OP_TRYC,
    OP_THROW,
    OP_HLT,
}OPCode;

#endif
