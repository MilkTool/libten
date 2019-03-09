#ifndef ten_opcodes_h
#define ten_opcodes_h

#define OP( NAME, EFFECT ) OPC_ ## NAME,
typedef enum {
    #include "inc/ops.inc"
    OPC_LAST
} OpCode;
#undef OP

#endif
