#include "ten_rec.h"
#include "ten_idx.h"
#include "ten_sym.h"
#include "ten_ptr.h"
#include "ten_state.h"
#include "ten_assert.h"
#include <limits.h>

void
recInit( State* state ) {
    state->recState = NULL;
}

#ifdef ten_TEST
#include "ten_str.h"
#include "ten_fmt.h"
#include "ten_macros.h"

typedef struct {
    Defer     defer;
    Scanner   scan;
    Index*    idx;
    Record*   rec;
    TVal      key;
    TVal      val;
} RecTest;

void
recTestScan( State* state, Scanner* scan ) {
    RecTest* test = structFromScan( RecTest, scan );
    stateMark( state, test->idx );
    if( test->rec )
        stateMark( state, test->rec );
    tvMark( test->key );
    tvMark( test->val );
}

void
recTestDefer( State* state, Defer* defer ) {
    RecTest* test = (RecTest*)defer;
    stateRemoveScanner( state, &test->scan );
}

void
recTest( State* state ) {
    RecTest test = {
        .defer = { .cb = recTestDefer },
        .scan  = { .cb = recTestScan },
        .idx   = idxNew( state ),
        .key   = tvUdf(),
        .val   = tvUdf()
    };
    stateInstallScanner( state, &test.scan );
    stateInstallDefer( state, &test.defer );
    
    for( uint i = 0 ; i < 100 ; i++ ) {
        test.rec = recNew( state, test.idx );
        for( uint j = 0 ; j < 100 ; j++ ) {
            test.key = tvInt( j );
            
            fmtA( state, false, "%u", j );
            test.val = tvObj( strNew( state, fmtBuf( state ), fmtLen( state ) ) );
            recDef( state, test.rec, test.key, test.val );
            tenAssert( tvEqual( recGet( state, test.rec, test.key ), test.val ) );
            test.val = tvSym( symGet( state, fmtBuf( state ), fmtLen( state ) ) );
            recSet( state, test.rec, test.key, test.val );
            tenAssert( tvEqual( recGet( state, test.rec, test.key ), test.val ) );
        }
        recSep( state, test.rec );
        tenAssert( tpGetPtr( test.rec->idx ) == test.idx );
        test.key = tvNil();
        test.val = tvNil();
        recDef( state, test.rec, test.key, test.val );
        tenAssert( tvEqual( recGet( state, test.rec, test.key ), test.val ) );
        tenAssert( tpGetPtr( test.rec->idx ) != test.idx );
    }
    
    
    stateCommitDefer( state, &test.defer );
}
#endif

Record*
recNew( State* state, Index* idx ) {
    Part recP;
    Record* rec = stateAllocObj( state, &recP, sizeof(Record), OBJ_REC );
    
    tenAssert( idx->nextLoc < USHRT_MAX );
    ushort cap = idx->nextLoc;
    
    Part valsP;
    TVal* vals = stateAllocRaw( state, &valsP, sizeof(TVal)*cap );
    for( uint i = 0 ; i < cap ; i++ )
        vals[i] = tvUdf();
    
    rec->idx  = tpMake( 0, idx );
    rec->vals = tpMake( cap, vals );
    
    stateCommitObj( state, &recP );
    stateCommitRaw( state, &valsP );
    
    return rec;
}

void
recSep( State* state, Record* rec ) {
    Index* idx = tpGetPtr( rec->idx );
    rec->idx = tpMake( 1, idx );
}

Index*
recIndex( State* state, Record* rec ) {
    return tpGetPtr( rec->idx );
}

void
recDef( State* state, Record* rec, TVal key, TVal val ) {
    Index* idx  = tpGetPtr( rec->idx );
    uint   cap  = tpGetTag( rec->vals );
    TVal*  vals = tpGetPtr( rec->vals );
    
    if( tvIsUdf( key ) )
        stateErrFmtA( state, ten_ERR_RECORD, "Use of `udf` as record key" );
    
    // If the Record is marked to be separated from
    // the Index then copy a subset of the Index as
    // the Record's new Index.
    if( tpGetTag( rec->idx ) ) {
        Index* sdx = idxSub( state, idx, cap );
        rec->idx = tpMake( 0, sdx );
        for( uint i = 0 ; i < cap ; i++ )
            if( !tvIsUdf( vals[i] ) ) {
                idxRemByLoc( state, idx, i );
                idxAddByLoc( state, sdx, i );
            }
        idx = sdx;
    }
    
    uint i = idxGetByKey( state, idx, key );
    
    // If defining to `udf` and the key exists in the
    // Index, and the Record references the key; then
    // unref/remove the key from the Index.
    if( tvIsUdf( val ) ) {
        if( i < cap && !tvIsUdf( vals[i] ) ) {
            idxRemByLoc( state, idx, i );
            vals[i] = tvUdf();
        }
        return;
    }
    
    if( i == UINT_MAX )
        i = idxAddByKey( state, idx, key );
    else
    if( i < cap && tvIsUdf( vals[i] ) )
        idxAddByLoc( state, idx, i );
    
    
    // Adjust size of value array if too small.
    if( i >= cap ) {
        Part valsP = {.ptr = vals, .sz = sizeof(TVal)*cap };
        
        int   ncap  = idx->nextLoc;
        TVal* nvals = stateResizeRaw( state, &valsP, sizeof(TVal)*ncap );
        for( uint j = cap ; j < ncap ; j++ )
            nvals[j] = tvUdf();
        
        stateCommitRaw( state, &valsP );
        rec->vals = tpMake( ncap, nvals );
        cap  = ncap;
        vals = nvals;
    }
    
    vals[i] = val;
}

void
recSet( State* state, Record* rec, TVal key, TVal val ) {
    Index* idx  = tpGetPtr( rec->idx );
    uint   cap  = tpGetTag( rec->vals );
    TVal*  vals = tpGetPtr( rec->vals );
    
    if( tvIsUdf( key ) )
        stateErrFmtA( state, ten_ERR_RECORD, "Use of `udf` as record key" );
    if( tvIsUdf( val ) )
        stateErrFmtA( state, ten_ERR_RECORD, "Field set to `udf`" );
    
    uint i = idxGetByKey( state, idx, key );
    if( i >= cap || tvIsUdf( vals[i] ) )
        stateErrFmtA( state, ten_ERR_RECORD, "Set of undefined record field" );
    
    vals[i] = val;
}

TVal
recGet( State* state, Record* rec, TVal key ) {
    Index* idx  = tpGetPtr( rec->idx );
    uint   cap  = tpGetTag( rec->vals );
    TVal*  vals = tpGetPtr( rec->vals );
    
    if( tvIsUdf( key ) )
        stateErrFmtA( state, ten_ERR_RECORD, "Use of `udf` as record key" );
    
    uint i = idxGetByKey( state, idx, key );
    if( i >= cap || tvIsUdf( vals[i] ) )
        return tvUdf();
    else
        return vals[i];
}

typedef struct {
    Defer    base;
    IdxIter* it;
} FreeIterDefer;

static void
freeIterDefer( State* state, Defer* d ) {
    FreeIterDefer* defer = (FreeIterDefer*)d;
    idxIterFree( state, defer->it );
}
void
recForEach( State* state, Record* rec, void* dat, RecEntryCb cb ) {
    Index* idx  = tpGetPtr( rec->idx );
    uint   cap  = tpGetTag( rec->vals );
    TVal*  vals = tpGetPtr( rec->vals );
    
    IdxIter* it = idxIterMake( state, idx );
    
    FreeIterDefer defer = { .base = { .cb = freeIterDefer }, .it = it };
    stateInstallDefer( state, (Defer*)&defer );
    
    TVal key;
    uint loc;
    uint cont = idxIterNext( state, it, &key, &loc );
    while( cont ) {
        while( cont && loc >= cap && tvIsUdf( vals[loc] ) )
            cont = idxIterNext( state, it, &key, &loc );
        
        if( cont ) {
            cb( state, dat, key, vals[loc] );
            cont = idxIterNext( state, it, &key, &loc );
        }
    }
    
    stateCommitDefer( state, (Defer*)&defer );
}


void
recTraverse( State* state, Record* rec ) {
    Index* idx  = tpGetPtr( rec->idx );
    uint   cap  = tpGetTag( rec->vals );
    TVal*  vals = tpGetPtr( rec->vals );
    
    stateMark( state, idx );
    for( uint i = 0 ; i < cap ; i++ )
        tvMark( vals[i] );
}

void
recDestruct( State* state, Record* rec ) {
    Index* idx  = tpGetPtr( rec->idx );
    uint   cap  = tpGetTag( rec->vals );
    TVal*  vals = tpGetPtr( rec->vals );
    
    if( !datIsDead( idx ) )
        for( uint i = 0 ; i < cap ; i++ )
            if( !tvIsUdf( vals[i] ) )
                idxRemByLoc( state, idx, i );
    
    stateFreeRaw( state, vals, sizeof(TVal)*cap );
}
