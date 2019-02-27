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
    uint          which;
    uint volatile where;
    SymT          name;
} GenLbl;

typedef enum {
    VAR_GLOBAL,
    VAR_UPVAL,
    VAR_LOCAL,
    VAR_CLOSED
} VarType;

typedef struct {
    uint             which;
    VarType volatile type;
    SymT             name;
} GenVar;

typedef struct {
    uint which;
    TVal val;
} GenConst;

void
genInit( State* state );

Gen*
genMake( State* state, Gen* parent, bool global, bool debug );

Function*
genFinish( State* state, Gen* gen, bool constr );


void
genSetFile( State* state, Gen* gen, SymT file );

void
genSetFunc( State* state, Gen* gen, SymT func );

void
genSetLine( State* state, Gen* gen, uint linenum );


GenConst const*
genAddConst( State* state, Gen* gen, TVal val );


GenVar const*
genAddParam( State* state, Gen* gen, SymT name, bool vParam );

GenVar const*
genAddVar( State* state, Gen* gen, SymT name );

GenVar const*
genGetVar( State* state, Gen* gen, SymT name );


GenLbl const*
genAddLbl( State* state, Gen* gen, SymT name );

GenLbl const*
genGetLbl( State* state, Gen* gen, SymT name );

void
genMovLbl( State* state, Gen* gen, GenLbl const* lbl, uint where );

void
genOpenScope( State* state, Gen* gen );

void
genCloseScope( State* state, Gen* gen );


void
genPutInstr( State* state, Gen* gen, instr in );

uint
genGetPlace( State* state, Gen* gen );

#endif
