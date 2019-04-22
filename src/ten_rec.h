// This component implements the  Record object type, which is
// basically just an array of values linked to an Index.  New
// fields defined in a Record will be added to its Index, fields
// redefined to `udf` are removed; at least this is how the Record
// sees it, in reality the Index keeps a ref count for each key
// and increments when a key is added or decrements when removed.
#ifndef ten_rec_h
#define ten_rec_h
#include "ten_types.h"

struct Record {
    
    // A pointer to the Record's associated index.  This
    // is paired with a 'sep' tag, indicating if the Record
    // has been 'separated' from the Index; so can't add
    // new fields.  We do this separation lazily to avoid
    // the copy overhead if possible, so the first `def`
    // after the Record has been separated will result in
    // its Index being replaced with a copy of the original.
    TPtr idx;
    
    TVal* vals;
    uint  cap;
};

#define recSize( STATE, REC ) (sizeof(Record))
#define recTrav( STATE, REC ) (recTraverse( STATE, REC ))
#define recDest( STATE, REC ) (recDestruct( STATE, REC ))


void
recInit( State* state );

Record*
recNew( State* state, Index* idx );

void
recSep( State* state, Record* rec );

Index*
recIndex( State* state, Record* rec );

void
recDef( State* state, Record* rec, TVal key, TVal val );

void
recSet( State* state, Record* rec, TVal key, TVal val );

TVal
recGet( State* state, Record* rec, TVal key );

typedef void (*RecEntryCb)( State* state, void* udat, TVal key, TVal val );

void
recForEach( State* state, Record* rec, void* udat, RecEntryCb cb );

void
recTraverse( State* state, Record* rec );

void
recDestruct( State* state, Record* rec );

#endif
