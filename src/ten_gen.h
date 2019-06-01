/**********************************************************************
This component implements the low level code generator for Ten bytecode,
which augments the higher level code generation implemented in the
compiler.  This component decides how to represent variables, upvalues,
constants, etc. and how scoping is implemented.
**********************************************************************/
#ifndef ten_gen_h
#define ten_gen_h
#include "ten_types.h"
#include "ten_opcodes.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    uint which;
    uint where;
    SymT name;
} GenLbl;

typedef enum {
    VAR_GLOBAL,
    VAR_UPVAL,
    VAR_LOCAL,
    VAR_CLOSED
} VarType;

typedef struct {
    uint    which;
    VarType type;
    SymT    name;
} GenVar;

typedef struct {
    uint which;
    TVal val;
} GenConst;

void
genInit( State* state );

Gen*
genMake( State* state, Gen* parent, SymT* func, bool global, bool debug );

void
genFree( State* state, Gen* gen );

Function*
genFinish( State* state, Gen* gen, bool constr );


void
genSetFile( State* state, Gen* gen, SymT file );

void
genSetFunc( State* state, Gen* gen, SymT func );

void
genSetLine( State* state, Gen* gen, uint linenum );


GenConst*
genAddConst( State* state, Gen* gen, TVal val );


GenVar*
genAddParam( State* state, Gen* gen, SymT name, bool vParam );

GenVar*
genAddVar( State* state, Gen* gen, SymT name );

GenVar*
genAddUpv( State* state, Gen* gen, SymT name );

GenVar*
genGetVar( State* state, Gen* gen, SymT name );


GenLbl*
genAddLbl( State* state, Gen* gen, SymT name );

GenLbl*
genGetLbl( State* state, Gen* gen, SymT name );

void
genMovLbl( State* state, Gen* gen, GenLbl* lbl, uint where );

void
genOpenScope( State* state, Gen* gen );

void
genCloseScope( State* state, Gen* gen );


void
genOpenLblScope( State* state, Gen* gen );

void
genCloseLblScope( State* state, Gen* gen );


void
genPutInstr( State* state, Gen* gen, instr in );

uint
genGetPlace( State* state, Gen* gen );

Upvalue**
genGlobalUpvals( State* state, Gen* gen );

#endif
