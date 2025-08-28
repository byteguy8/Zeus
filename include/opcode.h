#ifndef OPCODE_H
#define OPCODE_H

typedef enum opcode{
    // PRIMITIVES
    EMPTY_OPCODE,    // push NULL value equivalent
    FALSE_OPCODE,    // push FALSE value
    TRUE_OPCODE,     // push TRUE value
    CINT_OPCODE,     // push integer of 1 byte
    INT_OPCODE,      // push integer of 8 bytes
    FLOAT_OPCODE,    // push float
    STRING_OPCODE,   // push string
    TEMPLATE_OPCODE, // construct template and push it
    ARRAY_OPCODE,
	LIST_OPCODE,
    DICT_OPCODE,
	RECORD_OPCODE,

    IARRAY_OPCODE,
    ILIST_OPCODE,
    IDICT_OPCODE,

    CONCAT_OPCODE,
    MULSTR_OPCODE,

    // ARITHMETIC
    ADD_OPCODE, // add two integers or floats and push the result
    SUB_OPCODE, // subtract two integers or floats and push the result
    MUL_OPCODE, // muliply two integers or floats and push the result
    DIV_OPCODE, // divide two integers or floats and push the result
	MOD_OPCODE, // calculate module from two integers and push the result

    // BITWISE
    BNOT_OPCODE, // not bitwise
    LSH_OPCODE,  // left shift
    RSH_OPCODE,  // right shift
    BAND_OPCODE, // and bitwise
    BXOR_OPCODE, // xor bitwise
    BOR_OPCODE,  // or bitwise

    // COMPARISON
    LT_OPCODE, // less
    GT_OPCODE, // greater
    LE_OPCODE, // less equals
    GE_OPCODE, // greater equals
    EQ_OPCODE, // equals
    NE_OPCODE, // not equals

    // LOGICAL
    OR_OPCODE,
    AND_OPCODE,
    NOT_OPCODE,
    NNOT_OPCODE,

    LSET_OPCODE,  // set local symbol
    LGET_OPCODE,  // get local symbol
    OSET_OPCODE,  // set out value symbol (for closures)
    OGET_OPCODE,  // get out value symbol (for closures)
    GDEF_OPCODE,  // define a global symbol
    GASET_OPCODE, // set global symbol access
    GSET_OPCODE,  // get global symbol
    GGET_OPCODE,  // set global symbol
    NGET_OPCODE,  // get native symbol
    SGET_OPCODE,  // get symbol from list of symbols
	ASET_OPCODE,  // set a value inside array
    PUT_OPCODE,   // set a value inside record

    POP_OPCODE,

    JMP_OPCODE,
	JIF_OPCODE,
	JIT_OPCODE,

    CALL_OPCODE,
    ACCESS_OPCODE,
    INDEX_OPCODE,
    RET_OPCODE,
	IS_OPCODE,
    THROW_OPCODE,
    LOAD_OPCODE,
}OPCode;

#endif
