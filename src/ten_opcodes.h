/***********************************************************************
This just wraps the opcode definitions given in `./inc/ops.inc` with
an appropriate enum to be used elsewhere in the runtime.
***********************************************************************/

#ifndef ten_opcodes_h
#define ten_opcodes_h

#define OP( NAME, EFFECT ) OPC_ ## NAME,
typedef enum {
    #include "inc/ops.inc"
    OPC_LAST
} OpCode;
#undef OP

#endif
