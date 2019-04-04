#include "ten_idx.h"
#include "ten_state.h"
#include "ten_assert.h"
#include "ten_tables.h"
#include "ten_macros.h"
#include "ten_sym.h"
#include "ten_ptr.h"
#include <string.h>
#include <limits.h>

static uint
stepTarget( uint cap );

static void
growMap( State* state, Index* idx, bool clean );

static void
growRefs( State* state, Index* idx );

static uint
find( TVal* keys, uint cap, uint* steps, TVal key );


void
idxInit( State* state ) {
    state->idxState = NULL;
}

#ifdef ten_TEST

#include "ten_str.h"

typedef struct {
    Defer     defer;
    Scanner   scan;
    Index*    idx;
    TVal      key;
} IdxTest;

static void
idxTestScan( State* state, Scanner* scan ) {
    IdxTest* test = structFromScan( IdxTest, scan );
    tvMark( test->key );
    stateMark( state, test->idx );
}

void
idxTestDefer( State* state, Defer* defer ) {
    IdxTest* test = (IdxTest*)defer;
    stateRemoveScanner( state, &test->scan );
}

void
idxTest( State* state ) {
    IdxTest test = {
        .defer = { .cb = idxTestDefer },
        .scan  = { .cb = idxTestScan },
        .idx   = idxNew( state ),
        .key   = tvUdf()
    };
    stateInstallScanner( state, &test.scan );
    stateInstallDefer( state, &test.defer );
    
    uint locs[1000];
    for( uint i = 0 ; i < 1000 ; i++ ) {
        test.key = tvInt( i );
        locs[i] = idxAddByKey( state, test.idx, test.key );
    }
    
    for( uint i = 0 ; i < 1000 ; i++ ) {
        idxAddByLoc( state, test.idx, locs[i] );
    }
    
    for( uint i = 0 ; i < 1000 ; i++ ) {
        test.key = tvInt( i );
        idxRemByKey( state, test.idx, test.key );
    }
    
    for( uint i = 0 ; i < 1000 ; i++ ) {
        idxRemByLoc( state, test.idx, locs[i] );
    }
    
    test.key = tvObj( strNew( state, "TestStringKey", 13 ) );
    idxAddByKey( state, test.idx, test.key );
    idxRemByKey( state, test.idx, test.key );
    
    stateCommitDefer( state, &test.defer );
}

#endif

Index*
idxNew( State* state ) {
    Part idxP;
    Index* idx = stateAllocObj( state, &idxP, sizeof(Index), OBJ_IDX );
    memset( idx, 0, sizeof(*idx) );
    
    uint mcap = fastGrowthMapCapTable[0];
    
    Part keysP;
    TVal* keys = stateAllocRaw( state, &keysP, sizeof(TVal)*mcap );
    for( uint i = 0 ; i < mcap ; i++ )
        keys[i] = tvUdf();
    
    Part locsP;
    uint* locs = stateAllocRaw( state, &locsP, sizeof(uint)*mcap );
    for( uint i = 0 ; i < mcap ; i++ )
        locs[i] = UINT_MAX;
    
    
    uint rcap = IDX_INIT_REFS_CAP;
    
    Part refsP;
    uint* refs = stateAllocRaw( state, &refsP, sizeof(uint)*rcap );
    
    idx->stepTarget = stepTarget( mcap );
    idx->stepLimit  = idx->stepTarget;
    idx->nextLoc    = 0;
    idx->map.row    = 0;
    idx->map.cap    = mcap;
    idx->map.keys   = keys;
    idx->map.locs   = locs;
    idx->refs.cap   = rcap;
    idx->refs.buf   = refs;
    
    stateCommitObj( state, &idxP );
    stateCommitRaw( state, &keysP );
    stateCommitRaw( state, &locsP );
    stateCommitRaw( state, &refsP );
    
    return idx;
}

Index*
idxSub( State* state, Index* idx, uint top ) {
    if( top == 0 )
        top = idx->nextLoc;
    
    Part subP;
    Index* sub = stateAllocObj( state, &subP, sizeof(Index), OBJ_IDX );
    
    uint mrow = 0;
    while( fastGrowthMapCapTable[mrow] < 3*top && mrow < fastGrowthMapCapTableSize )
        mrow++;
    uint mcap;
    if( mrow >= fastGrowthMapCapTableSize )
        mcap = 3*top;
    else
        mcap = fastGrowthMapCapTable[mrow];
    
    Part keysP;
    TVal* keys = stateAllocRaw( state, &keysP, sizeof(TVal)*mcap );
    for( uint i = 0 ; i < mcap ; i++ )
        keys[i] = tvUdf();
    
    Part locsP;
    uint* locs = stateAllocRaw( state, &locsP, sizeof(uint)*mcap );
    for( uint i = 0 ; i < mcap ; i++ )
        locs[i] = UINT_MAX;
    
    uint rcap = top;
    
    Part refsP;
    uint* refs = stateAllocRaw( state, &refsP, sizeof(uint)*rcap );
    for( uint i = 0 ; i < top ; i++ )
        refs[i] = 0;
    
    sub->stepTarget = stepTarget( mcap );
    sub->stepLimit  = sub->stepTarget;
    sub->nextLoc    = top;
    sub->map.row    = mrow;
    sub->map.cap    = mcap;
    sub->map.keys   = keys;
    sub->map.locs   = locs;
    sub->refs.cap   = rcap;
    sub->refs.buf   = refs;
    
    for( uint i = 0 ; i < idx->map.cap ; i++ ) {
        if( tvIsUdf( idx->map.keys[i] ) )
            continue;
        if( idx->map.locs[i] >= top )
            continue;
        
        uint s = 0;
        uint j = find( keys, mcap, &s, idx->map.keys[i] );
        tenAssert( s < mcap );
        
        keys[j] = idx->map.keys[i];
        locs[j] = idx->map.locs[i];
        
        if( s > sub->stepLimit )
            sub->stepLimit = s;
    }
    
    // Commit these because growMap() will reallocate them.
    stateCommitRaw( state, &keysP );
    stateCommitRaw( state, &locsP );
    stateCommitRaw( state, &refsP );
    
    // Grow the Index until the stepTarget is reached
    // to ensure we begin with the target step count.
    while( sub->stepLimit > sub->stepTarget )
        growMap( state, sub, false );
    
    stateCommitObj( state, &subP );
    return sub;
}

uint
idxAddByKey( State* state, Index* idx, TVal key ) {
    
    // Find a slot for the key, this puts the number
    // of steps from the key's ideal location in `s`.
    uint s = 0;
    uint i = find( idx->map.keys, idx->map.cap, &s, key );
    tenAssert( i < idx->map.cap );
    
    // If an entry for the key doesn't exist then add one.
    if( tvIsUdf( idx->map.keys[i] ) ) {
        idx->map.keys[i] = key;
        
        // A locator of UINT_MAX indicates that the slot
        // has never been assigned a locator, so allocate
        // a new one.
        if( idx->map.locs[i] == UINT_MAX ) {
            tenAssert( idx->nextLoc < UINT_MAX );
            
            uint loc = idx->nextLoc++;
            idx->map.locs[i] = loc;
            
            if( loc >= idx->refs.cap )
                growRefs( state, idx );
            idx->refs.buf[loc] = 0;
        }
        
        // Increment ref count.
        idx->refs.buf[idx->map.locs[i]]++;
        tenAssert( idx->refs.buf[idx->map.locs[i]] < UINT_MAX );
    }
    
    // Save the locator since the Index may be grown below,
    // if it's grown then keys will be reordered in the map
    // and `idx->map.locs[i]` will refer to a different slot.
    uint loc = idx->map.locs[i];
    
    if( s > idx->stepLimit )
        idx->stepLimit = s;
    
    // If `find()` took more than the prefered number of steps
    // to find the slot then grow the Index.  This may still
    // result in stepLimit being greater than stepTarget; but
    // it'll work toward the target with each new definition
    // until it's reached.
    if( idx->stepLimit > idx->stepTarget || idx->nextLoc >= idx->map.cap )
        growMap( state, idx, true );
    
    return loc;
}

uint
idxGetByKey( State* state, Index* idx, TVal key ) {
    // Find the key's map slot, this'll only try stepLimit
    // steps beyond the key's ideal location.
    uint i = find( idx->map.keys, idx->map.cap, &idx->stepLimit, key );
    if( i == UINT_MAX )
        return UINT_MAX;
    
    return idx->map.locs[i];
}

void
idxRemByKey( State* state, Index* idx, TVal key ) {
    // Find the key's map slot, this'll only try stepLimit
    // steps beyond the key's ideal location.
    uint i = find( idx->map.keys, idx->map.cap, &idx->stepLimit, key );
    if( i == UINT_MAX )
        return;
    
    tenAssert( idx->refs.buf[idx->map.locs[i]] > 0 );
    idx->refs.buf[idx->map.locs[i]]--;
}

void
idxAddByLoc( State* state, Index* idx, uint loc ) {
    tenAssert( loc < idx->nextLoc );
    tenAssert( idx->refs.buf[loc] < UINT_MAX );
    
    idx->refs.buf[loc]++;
}

void
idxRemByLoc( State* state, Index* idx, uint loc ) {
    tenAssert( loc < idx->nextLoc );
    tenAssert( idx->refs.buf[loc] > 0 );
    
    idx->refs.buf[loc]--;
}

void
idxTraverse( State* state, Index* idx ) {
    for( uint i = 0 ; i < idx->map.cap ; i++ )
        tvMark( idx->map.keys[i] );
}

void
idxDestruct( State* state, Index* idx ) {
    stateFreeRaw( state, idx->map.keys, sizeof(TVal)*idx->map.cap );
    stateFreeRaw( state, idx->map.locs, sizeof(uint)*idx->map.cap );
    stateFreeRaw( state, idx->refs.buf, sizeof(uint)*idx->refs.cap );
    
    // Cap of 0 tells records that the Index has been destructed,
    // otherwise the record may try to unref its keys when we're
    // destructing everything while finalizing the State.
    idx->map.cap  = 0;
    idx->refs.cap = 0;
}

static uint
stepTarget( uint cap ) {
    // This computes the truncated log2( cap )
    // as the target step count.  Using a log
    // of the cap ensures that the step size
    // will grow, but not unreasonably fast, so
    // we still have some lookup efficiency.
    uint log = 0;
    uint msk = UINT_MAX;
    while( msk & cap ) {
        msk <<= 1;
        log++;
    }
    
    return log;
}

static void
growMap( State* state, Index* idx, bool clean ) {
    uint mcap;
    if( idx->map.row + 1 < fastGrowthMapCapTableSize )
        mcap = fastGrowthMapCapTable[++idx->map.row];
    else
        mcap = idx->map.cap * 2;
    
    uint steps = 0;
    
    Part keysP;
    TVal* keys = stateAllocRaw( state, &keysP, sizeof(TVal)*mcap );
    for( uint i = 0 ; i < mcap ; i++ )
        keys[i] = tvUdf();
    
    Part locsP;
    uint* locs = stateAllocRaw( state, &locsP, sizeof(uint)*mcap );
    for( uint i = 0 ; i < mcap ; i++ )
        locs[i] = UINT_MAX;
    
    // Migrate the old keys to positions in the new arrays.
    for( uint i = 0 ; i < idx->map.cap ; i++ ) {
        // Unallocated slots of course are not migrated.
        if( tvIsUdf( idx->map.keys[i] ) )
            continue;
        
        // Also don't migrate keys with a ref count of zero
        // yet, we'll do that in the next pass.  We need to
        // migrate these to recycle their allocated locators,
        // but since the keys themselves may be replaced with
        // `udf` it's better to give the valid keys the best
        // slots.
        if( idx->refs.buf[idx->map.locs[i]] == 0 )
            continue;
        
        // Figure out where to put the key in the new allocations.
        uint s = 0;
        uint j = find( keys, mcap, &s, idx->map.keys[i] );
        if( s > steps )
            steps = s;
        
        keys[j] = idx->map.keys[i];
        locs[j] = idx->map.locs[i];
    }
    
    // Migrate the old locators whose keys are no longer being
    // used.  If `clean` is true then we'll also skip the keys
    // and just migrate the locators, leaving the keys as `udf`.
    for( uint i = 0 ; i < idx->map.cap ; i++ ) {
        if( tvIsUdf( idx->map.keys[i] ) )
            continue;
        if( idx->refs.buf[idx->map.locs[i]] != 0 )
            continue;
        
        uint s = 0;
        uint j = find( keys, mcap, &s, idx->map.keys[i] );
        
        if( !clean )
            keys[j] = idx->map.keys[i];
        locs[j] = idx->map.locs[i];
    }
    
    // Finalize the memory.
    stateFreeRaw( state, idx->map.keys, sizeof(TVal)*idx->map.cap );
    stateFreeRaw( state, idx->map.locs, sizeof(uint)*idx->map.cap );
    stateCommitRaw( state, &keysP );
    stateCommitRaw( state, &locsP );
    
    idx->map.cap    = mcap;
    idx->map.keys   = keys;
    idx->map.locs   = locs;
    idx->stepLimit  = steps;
    idx->stepTarget = stepTarget( mcap );
}

static void
growRefs( State* state, Index* idx ) {
    size_t cap = idx->refs.cap * 2;
    
    Part  bufP = { .ptr = idx->refs.buf, .sz = sizeof(uint)*idx->refs.cap };
    uint* buf  = stateResizeRaw( state, &bufP, sizeof(uint)*cap );
    stateCommitRaw( state, &bufP );
    
    idx->refs.cap = cap;
    idx->refs.buf = buf;
}

static uint
find( TVal* keys, uint cap, uint* steps, TVal key ) {
    uint hash = tvHash( key );
    uint lim  = *steps > 0 ? *steps : cap;
    
    register uint s = 0;
    register uint i = hash % cap;
    while( s++ < lim ) {
        if( tvIsUdf( keys[i] ) || tvEqual( keys[i], key ) ) {
            if( *steps == 0 )
                *steps = s;
            return i;
        }
        i = (i + 1) % cap;
    }
    
    return UINT_MAX;
}

struct IdxIter {
    Finalizer finl;
    Scanner   scan;
    Index*    idx;
    uint      slot;
};

void
idxIterScan( State* state, Scanner* scan ) {
    IdxIter* iter = structFromScan( IdxIter, scan );
    stateMark( state, iter->idx );
}

void
idxIterFinl( State* state, Finalizer* finl ) {
    IdxIter* iter = structFromFinl( IdxIter, finl );
    stateRemoveScanner( state, &iter->scan );
    stateFreeRaw( state, iter, sizeof(IdxIter) );
}

IdxIter*
idxIterMake( State* state, Index* idx ) {
    Part iterP;
    IdxIter* iter = stateAllocRaw( state, &iterP, sizeof(IdxIter) );
    iter->idx     = idx;
    iter->slot    = 0;
    iter->scan.cb = idxIterScan;
    iter->finl.cb = idxIterFinl;
    stateInstallScanner( state, &iter->scan );
    stateInstallFinalizer( state, &iter->finl );
    stateCommitRaw( state, &iterP );
    return iter;
}

void
idxIterFree( State* state, IdxIter* iter ) {
    stateRemoveScanner( state, &iter->scan );
    stateRemoveFinalizer( state, &iter->finl );
    stateFreeRaw( state, iter, sizeof(IdxIter) );
}

bool
idxIterNext( State* state, IdxIter* iter, TVal* key, uint* loc ) {
    Index* idx = iter->idx;
    while( iter->slot < idx->map.cap && tvIsUdf( idx->map.keys[iter->slot] ) )
        iter->slot++;
    if( iter->slot == idx->map.cap )
        return false;
    
    *key = idx->map.keys[iter->slot];
    *loc = idx->map.locs[iter->slot];
    iter->slot++;
    return true;
}
