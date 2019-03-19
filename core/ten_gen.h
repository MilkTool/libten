// Here we implement the code generator.  We keep this separate from
// the compiler itself despite the performance overhead to allow for
// a bit more flexibility between the two components, and to keep each
// at a manageable size.  For example we can pretty easily extend the
// code generator to produce a serialized function as output instead
// of a VM dependent function object; without touching the compiler
// code.
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

#ifdef ten_TEST
    void
    genTest( State* state );
#endif

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

#endif
