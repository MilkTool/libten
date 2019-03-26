#include "ten_stab.h"
#include "ten_ntab.h"
#include "ten_state.h"
#include "ten_assert.h"
#include "ten_macros.h"
#include <limits.h>

// This code can get a bit hairy.  Basically the way it works is we
// use an NTab instance to map names to slots within the `entries`
// buffer; and each of these makes up a list of entries for that
// name.  Each entry tells us where to find the variable with that
// name during a spcific range of pc (program counter) values.
// Each Entry will belong to two lists: its respective list within
// the `entries` buffer which is used to lookup the entry by name,
// and one of the `used`, `free`, or `stale` lists.  These lists
// are used to keep track of scoping.
//
// The `used` list contains entries that are currently in scope,
// meaning that a respective `stabCloseScope()` call hasn't been
// made for the `stabOpenScope()` that opened the entry's scope.
// Each entry is assigned a `scope` number, which increments for
// each scope created.  When a call to `stabCloseScope()` is made
// to close the current scope we use this number to figure out
// where the current scope ends, as all entries in the current
// scope will have the same scope number.  These are all removed
// from the `used` list and added to the `free` scope.
//
// The `free` scope contains entries that are not longer in scope,
// and whose `loc` (index into the value array) haven't yet been
// recycled.  Once an entry is not longer in scope the entry itself
// cannot be recycled because it'll be used for lookups, but its
// index can be recycled and put into a different entry.  So entries
// whose scopes have ended are put in the `free` list until they can
// be recycled; after which they're moved to the `stale` list.
//
// The `stale` list contains entries that are no longer in scope and
// have already been recycled.
//
// Note that even though we keep the name assigned to an entry as
// a SymT, we don't install a Scanner since these names will
// correspond exactly with those in the `ntab`, so scanning would
// be redundant.

#define putForScope( LIST, NODE )           \
    do {                                    \
        (NODE)->forScope.next = (LIST);     \
        (LIST) = (NODE);                    \
    } while( 0 )

#define popForScope( LIST )                 \
    do {                                    \
        (LIST) = (LIST)->forScope.next;     \
    } while( 0 )

#define putForName( LIST, NODE )            \
    do {                                    \
        (NODE)->forName.next = (LIST);      \
        (LIST) = (NODE);                    \
    } while( 0 )

#define popForName( LIST )                  \
    do {                                    \
        (LIST) = (LIST)->forName.next;      \
    } while( 0 )

typedef struct Entry {
    uint  loc;
    uint  scope;
    SymT  name;
    void* edat;
    struct {
        uint start;
        uint end;
    } range;
    
    struct {
        struct Entry* next;
    } forName;
    struct {
        struct Entry* next;
    } forScope;
} Entry;

struct STab {
    Finalizer finl;
    
    NTab* ntab;
    
    struct {
        uint    cap;
        Entry** buf;
    } entries;
    
    struct {
        Entry* used;
        Entry* free;
        Entry* stale;
    } scoping;
    
    // The current scope's scope number and start pc.
    struct {
        uint start;
        uint scope;
    } current;
    
    // Next scope number and next array index to allocate.
    uint nextScope;
    uint nextLoc;
    
    // Should slots be recycled?
    bool recycle;
    
    // Cleanup function for entry user data.
    FreeEntryCb free;
    
    // Table user data.
    void* udat;
};

static void
stabFinl( State* state, Finalizer* finl ) {
    STab* stab = structFromFinl( STab, finl );

    Entry* eIt;
    
    eIt = stab->scoping.used;
    while( eIt ) {
        Entry* e = eIt;
        eIt = eIt->forScope.next;
        
        if( e->edat && stab->free )
            stab->free( state, stab->udat, e->edat );
        
        stateFreeRaw( state, e, sizeof(Entry) );    
    }
    eIt = stab->scoping.free;
    while( eIt ) {
        Entry* e = eIt;
        eIt = eIt->forScope.next;
        
        if( e->edat && stab->free )
            stab->free( state, stab->udat, e->edat );
        
        stateFreeRaw( state, e, sizeof(Entry) );
    }
    eIt = stab->scoping.stale;
    while( eIt ) {
        Entry* e = eIt;
        eIt = eIt->forScope.next;
        
        if( e->edat && stab->free )
            stab->free( state, stab->udat, e->edat );
        
        stateFreeRaw( state, e, sizeof(Entry) );
    }

    stateFreeRaw( state, stab->entries.buf, sizeof(Entry*)*stab->entries.cap );
    stateFreeRaw( state, stab, sizeof(STab) );
}

STab*
stabMake( State* state, bool recycle, void* udat, FreeEntryCb free ) {
    Part stabP;
    STab* stab = stateAllocRaw( state, &stabP, sizeof(STab) );
    
    
    uint ecap = 7;
    Part entriesP;
    Entry** entries = stateAllocRaw( state, &entriesP, sizeof(Entry*)*ecap );
    for( uint i = 0 ; i < ecap ; i++ )
        entries[i] = NULL;
    
    stab->ntab          = ntabMake( state );
    stab->entries.cap   = ecap;
    stab->entries.buf   = entries;
    stab->scoping.used  = NULL;
    stab->scoping.free  = NULL;
    stab->scoping.stale = NULL;
    stab->current.start = 0;
    stab->current.scope = 0;
    stab->nextScope     = 1;
    stab->nextLoc       = 0;
    stab->recycle       = recycle;
    stab->free          = free;
    stab->udat          = udat;
    stab->finl.cb       = stabFinl;
    stateInstallFinalizer( state, &stab->finl );
    
    stateCommitRaw( state, &entriesP );
    stateCommitRaw( state, &stabP );
    
    return stab;
}



void
stabFree( State* state, STab* stab ) {
    ntabFree( state, stab->ntab );
    
    stateRemoveFinalizer( state, &stab->finl );
    stabFinl( state, &stab->finl );
}

uint
stabNumSlots( State* state, STab* stab ) {
    return stab->nextLoc;
}

uint
stabAdd( State* state, STab* stab, SymT sym, void* edat ) {
    // Allocate a slot for the entry if one doesn't exist.
    uint i = ntabAdd( state, stab->ntab, sym );
    if( i >= stab->entries.cap ) {
        Part entriesP = {
            .ptr = stab->entries.buf,
            .sz = sizeof(Entry*)*stab->entries.cap
        };
        uint ecap = stab->entries.cap * 2;
        Entry** entries = stateResizeRaw( state, &entriesP, sizeof(Entry*)*ecap );
        for( uint i = stab->entries.cap ; i < ecap ; i++ )
            entries[i] = NULL;
        
        stab->entries.cap = ecap;
        stab->entries.buf = entries;
        stateCommitRaw( state, &entriesP );
    }
    
    // Search existing entries for one with the current scope.
    Entry* eIt = stab->entries.buf[i];
    while( eIt ) {
        if( eIt->scope == stab->current.scope ) {
            if( eIt->edat && stab->free )
                stab->free( state, stab->udat, eIt->edat );
            eIt->edat = edat;
            return eIt->loc;
        }
        
        eIt = eIt->forName.next;
    }
    
    // No matching entries exist, so allocate a new one.
    Part entryP;
    Entry* entry = stateAllocRaw( state, &entryP, sizeof(Entry) );
    entry->scope = stab->current.scope;
    entry->name  = sym;
    entry->edat  = edat;
    entry->range.start = stab->current.start;
    entry->range.end   = UINT_MAX;
    putForScope( stab->scoping.used, entry );
    putForName( stab->entries.buf[i], entry );
    
    // If there are any entries in the `free` list then recycle
    // the next one's `loc`, and move it to the `stale` list.
    if( stab->scoping.free ) {
        Entry* free = stab->scoping.free;
        
        entry->loc = free->loc;
        popForScope( stab->scoping.free );
        putForScope( stab->scoping.stale, free );
    }
    // Otherwise allocate a new locator.
    else {
        entry->loc = stab->nextLoc++;
    }
    stateCommitRaw( state, &entryP );
    
    return entry->loc;
}

static Entry*
getEntry( State* state, STab* stab, SymT sym, uint pc ) {
    uint i = ntabGet( state, stab->ntab, sym );
    if( i == UINT_MAX )
        return NULL;
    
    Entry* eIt = stab->entries.buf[i];
    while( eIt ) {
        if( eIt->range.start <= pc && pc < eIt->range.end )
            return eIt;
        
        eIt = eIt->forName.next;
    }
    
    return NULL;
}

uint
stabGetLoc( State* state, STab* stab, SymT sym, uint pc ) {
    Entry* e = getEntry( state, stab, sym, pc );
    if( e == NULL )
        return UINT_MAX;
    return e->loc;
}

void*
stabGetDat( State* state, STab* stab, SymT sym, uint pc ) {
    Entry* e = getEntry( state, stab, sym, pc );
    if( e == NULL )
        return NULL;
    return e->edat;
}

SymT
stabRev( State* state, STab* stab, uint loc, uint pc ) {
    // This will only be used for debugging, it'll perform
    // a lookup by `loc` rather than name.  Since the only
    // use will be for debugging it doesn't have to be
    // especially efficient, and it isn't worth allocating
    // an additional loc-to-entry map; so we just perform
    // a linear search over the scoping lists.
    Entry* eIt;
    
    eIt = stab->scoping.used;
    while( eIt ) {
        if( eIt->loc == loc && eIt->range.start <= pc && pc < eIt->range.end )
            return eIt->name;
        eIt = eIt->forScope.next;  
    }
    eIt = stab->scoping.free;
    while( eIt ) {
        if( eIt->loc == loc && eIt->range.start <= pc && pc < eIt->range.end )
            return eIt->name;
        eIt = eIt->forScope.next;
    }
    eIt = stab->scoping.stale;
    while( eIt ) {
        if( eIt->loc == loc && eIt->range.start <= pc && pc < eIt->range.end )
            return eIt->name;
        eIt = eIt->forScope.next;
    }
    
    tenAssertNeverReached();
    return 0;
}

void
stabOpenScope( State* state, STab* stab, uint pc ) {
    
    // Make a dummy entry to save the `current.scope` and
    // and `current.start` to be restored after the new
    // scope is closed.
    Part entryP;
    Entry* entry = stateAllocRaw( state, &entryP, sizeof(Entry) );
    entry->scope = stab->current.scope;
    entry->loc   = UINT_MAX;
    entry->edat  = NULL;
    entry->range.start = stab->current.start;
    entry->range.end   = stab->current.start;
    putForScope( stab->scoping.used, entry );
    stateCommitRaw( state, &entryP );
    
    stab->current.scope++;
    stab->current.start = pc;
}

void
stabCloseScope( State* state, STab* stab, uint pc ) {
    tenAssert( stab->scoping.used );
    
    // Move all the `used` entries with the current scope
    // number to the `free` list so their `loc`s can be
    // recycled, if recycling is enabled otherwise move
    // them to the `stale` list.
    while( stab->scoping.used->scope == stab->current.scope ) {
        Entry* entry = stab->scoping.used;
        entry->range.end = pc;
        popForScope( stab->scoping.used );
        
        if( stab->recycle )
            putForScope( stab->scoping.free, entry );
        else
            putForScope( stab->scoping.stale, entry );
    }
    
    // Restore the current scope info from the dummy entry
    // on the `used` list.
    Entry* dummy = stab->scoping.used;
    popForScope( stab->scoping.used );
    tenAssert( dummy != NULL );
    tenAssert( dummy->loc == UINT_MAX );
    stab->current.scope = dummy->scope;
    stab->current.start = dummy->range.start;
    stateFreeRaw( state, dummy, sizeof(Entry) );
}


void
stabForEach( State* state, STab* stab, ProcEntryCb proc ) {
    Entry* eIt;
    
    eIt = stab->scoping.used;
    while( eIt ) {
        if( eIt->loc != UINT_MAX )
            proc( state, stab->udat, eIt->edat );
        eIt = eIt->forScope.next;  
    }
    eIt = stab->scoping.free;
    while( eIt ) {
        proc( state, stab->udat, eIt->edat );
        eIt = eIt->forScope.next;
    }
    eIt = stab->scoping.stale;
    while( eIt ) {
        proc( state, stab->udat, eIt->edat );
        eIt = eIt->forScope.next;
    }
}
