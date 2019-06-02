/***********************************************************************
This implements STab, which is your typical symbol table used by the
compiler for tracking identifiers, labels, etc.  The table allows its
user to bring its own representation for the 'thing' being represented;
which the table stores as a `void*` and releases with the destructor
registered at table creation time; if this argument isn't passed as NULL.
***********************************************************************/

#ifndef ten_stab_h
#define ten_stab_h
#include "ten_types.h"
#include <stdbool.h>

typedef void (*FreeEntryCb)( State* state, void* udat, void* edat );
typedef void (*ProcEntryCb)( State* state, void* udat, void* edat );

STab*
stabMake( State* state, bool recycle, void* udat, FreeEntryCb free );

void
stabFree( State* state, STab* stab );

uint
stabNumSlots( State* state, STab* stab );

uint
stabAdd( State* state, STab* stab, SymT sym, void* edat );

uint
stabGetLoc( State* state, STab* stab, SymT sym, uint pc );

void*
stabGetDat( State* state, STab* stab, SymT sym, uint pc );

SymT
stabRev( State* state, STab* stab, uint loc, uint pc );

void
stabOpenScope( State* state, STab* stab, uint pc );

void
stabCloseScope( State* state, STab* stab, uint pc );

void
stabForEach( State* state, STab* stab, ProcEntryCb proc );

#endif
