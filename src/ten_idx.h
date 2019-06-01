/**********************************************************************
This component implements Ten's Index data type, which serves as a
shared lookup table for record instances.  This relationship between
records and indices is a pretty important part of both this library,
and the Ten language itself, and is described in detail in
`../docs/articles/Records.md`.
**********************************************************************/

#ifndef ten_idx_h
#define ten_idx_h
#include "ten_types.h"
#include <stddef.h>
#include <stdbool.h>

// The Index type just maps Ten values (used as keys) to
// locators used as offsets in the value array of records.
// But it's implemented in a bit of an unusual way in an
// attempt to improve locality of reference when on lookup.
//
// We use an open addressed hashmap for lookups, so there's
// no array of linked lists as in the more popular design;
// there's just one long array of key-value pairs.  But
// there's a twist, we add a stepLimit and a stepTarget.  The
// stepLimit is the largest gap between a key's position
// in the Index and its ideal position; so it's also the
// largest number of array slots (after the first) that'll
// be checked when searching for an existing key.  This
// limit can increase when a new key is added, but will be
// reset to stepTarget when the Index changes size, thus
// the stepLimit will always tend toward the stepTarget.
//
// The stepTarget ultimately determines the space vs time
// overhead tradeoff balance for the Index.  In general
// we'll increase the stepTarget for larger Index sizes
// and decrease it for smaller sizes; this ensures that
// the more common smaller Indexes are fast, but the
// larger ones aren't prohibitively expensive in terms
// of memory.

struct Index {
    
    // The next locator to be allocated, increases away from 0
    // for each new key added to the Index, never decreases,
    // but existing allocations can be recycled.
    uint nextLoc;
    
    // The current stepTarget and stepLimit as described above.
    uint stepTarget;
    uint stepLimit;
    
    // This represents the map from keys to locators.  We use
    // a seperate array for each field associated with a slot
    // to maximize the density of the keys and improve cache
    // usage.  In addition to the key and locator arrays, we
    // also have one for ref counts, since this is how we
    // keep track of which slots are still in use and which
    // can be recycled.
    struct {
        size_t row;
        size_t cap;
        TVal*  keys;
        uint*  locs;
    } map;
    
    // This is a map of ref counts, for each key `buf[loc]`
    // keeps track of its current ref count, where `loc`
    // is the locator allocated for the key.  The `row`
    // indicates the capacity of `buf` as a row of the
    // recCapTable table.
    struct {
        size_t row;
        uint*  buf;
    } refs;
};

#define idxSize( STATE, IDX ) (sizeof(Index))
#define idxTrav( STATE, IDX ) (idxTraverse( (STATE), (IDX) ))
#define idxDest( STATE, IDX ) (idxDestruct( (STATE), (IDX) ))


void
idxInit( State* state );

Index*
idxNew( State* state );

Index*
idxSub( State* state, Index* idx, uint top );

uint
idxAddByKey( State* state, Index* idx, TVal key );

uint
idxGetByKey( State* state, Index* idx, TVal key );

void
idxRemByKey( State* state, Index* idx, TVal key );

void
idxAddByLoc( State* state, Index* idx, uint loc );

void
idxRemByLoc( State* state, Index* idx, uint loc );

void
idxTraverse( State* state, Index* idx );

void
idxDestruct( State* state, Index* idx );


typedef struct IdxIter IdxIter;

IdxIter*
idxIterMake( State* state, Index* idx );

void
idxIterFree( State* state, IdxIter* iter );

bool
idxIterNext( State* state, IdxIter* iter, TVal* key, uint* loc );

#endif
