// This implements your typical symbol table.  It's used by the compiler
// to map variables to specific slots within the array of locals, or
// to map labels to slots within their own array.  This usese the NTab
// implementation internally but adds scoping and slot recycling to
// the mix.  For example variables defined within a `do-for` block will
// only be in scope within that block, anywhere else and the same
// variable slot defined within this scope can be used for a different
// variable.
#ifndef ten_stab_h
#define ten_stab_h
#include "ten_types.h"


STab*
stabMake( State* state );

void
stabFree( State* state, STab* stab );

uint
stabAdd( State* state, STab* stab, SymT sym );

uint
stabGet( State* state, STab* stab, SymT sym, uint pc );

SymT
stabRev( State* state, STab* stab, uint loc, uint pc );

void
stabOpenScope( State* state, STab* stab, uint pc );

void
stabCloseScope( State* state, STab* stab, uint pc );

#endif
