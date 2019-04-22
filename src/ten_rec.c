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
    rec->cap  = cap;
    rec->vals = vals;
    
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
    uint   cap  = rec->cap;
    TVal*  vals = rec->vals;
    
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
    if( i >= cap || tvIsUdf( vals[i] ) )
        idxAddByLoc( state, idx, i );
    
    
    // Adjust size of value array if too small.
    if( i >= cap ) {
        Part valsP = {.ptr = vals, .sz = sizeof(TVal)*cap };
        
        uint ncap = idx->nextLoc;
        if( ncap == UINT_MAX )
            stateErrFmtA( state, ten_ERR_RECORD, "Record exceeds max size" );
        TVal* nvals = stateResizeRaw( state, &valsP, sizeof(TVal)*ncap );
        for( uint j = cap ; j < ncap ; j++ )
            nvals[j] = tvUdf();
        
        stateCommitRaw( state, &valsP );
        cap  = rec->cap  = ncap;
        vals = rec->vals = nvals;
    }
    
    vals[i] = val;
}

void
recSet( State* state, Record* rec, TVal key, TVal val ) {
    Index* idx  = tpGetPtr( rec->idx );
    uint   cap  = rec->cap;
    TVal*  vals = rec->vals;
    
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
    uint   cap  = rec->cap;
    TVal*  vals = rec->vals;
    
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
    uint   cap  = rec->cap;
    TVal*  vals = rec->vals;
    
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
    uint   cap  = rec->cap;
    TVal*  vals = rec->vals;
    
    stateMark( state, idx );
    for( uint i = 0 ; i < cap ; i++ )
        tvMark( vals[i] );
}

void
recDestruct( State* state, Record* rec ) {
    Index* idx  = tpGetPtr( rec->idx );
    uint   cap  = rec->cap;
    TVal*  vals = rec->vals;
    
    if( !datIsDead( idx ) )
        for( uint i = 0 ; i < cap ; i++ )
            if( !tvIsUdf( vals[i] ) )
                idxRemByLoc( state, idx, i );
    
    stateFreeRaw( state, vals, sizeof(TVal)*cap );
}
