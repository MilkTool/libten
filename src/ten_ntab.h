// Here we implement NTab (Name Table).  This type is used throughout
// the code base to map names (as symbols) to array indices.  This
// is basically the same as the Index type, but specialized for the
// purpose of mapping names.  Since it'll mostly be used for
// compilation and debugging, we can optimize them more for size than
// performance; and since they won't contain any GC objects or pointers,
// we only need to scan for symbols.

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
