/***********************************************************************
This implements the NTab data structure, used elsewhere throughout the
runtime.  This is basically another form of hashmap, but unlike the Index
type NTab isn't considered a Ten object.  Since these will mostly be used
during compilation, and generally won't live very long; it's implemented
as a separate chaining hashmap rather than an open addressing hashmap as
is the index implementation; to optimize lookup time instead of size.
***********************************************************************/

#ifndef ten_ntab_h
#define ten_ntab_h
#include "ten_types.h"

NTab*
ntabMake( State* state );

void
ntabFree( State* state, NTab* ntab );

uint
ntabAdd( State* state, NTab* ntab, SymT name );

uint
ntabGet( State* state, NTab* ntab, SymT name );

#endif
