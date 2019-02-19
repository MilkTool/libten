#include "ten_rec.h"
#include "ten_state.h"

void
recInit( State* state ) {
    state->recState = NULL;
}

void
recNew( State* state, Index* idx ) {
    Part recP;
    Record* rec = stateAllocObj( state, &recP, sizeof(Record), OBJ_REC );
    
    rigAssert( idx->nextLoc < USHORT_MAX );
    ushort cap = idx->nextLoc;
    
    Part valsP;
    TVal* vals = stateAllocRaw( state, &valsP, sizeof(TVal)*cap );
    for( uint i = 0 ; i < cap ; i++ )
        vals[i] = valUdf();
    
    rec->idx  = tpMake( 0, idx );
    rec->vals = tpMake( cap, vals );
    
    stateCommitObj( state, &recP );
    stateCommitRaw( state, &valsP );
    
    return rec;
}

void
recSep( State* state, Record* rec ) {
    Index* idx = tpGetPtr( rec->idx );
    rec->idx = mkIndex( 1, idx );
}

void
recDef( State* state, Record* rec, TVal key, TVal val ) {
    Index* idx  = tpGetPtr( rec->idx );
    uint   cap  = tpGetTag( rec->vals );
    TVal*  vals = tpGetPtr( rec->vals );
    
    // If the Record is marked to be separated from
    // the Index then copy a subset of the Index as
    // the Record's new Index.
    if( tpGetTag( rec->idx ) ) {
        idx = idxSub( state, idx, cap );
        rec->idx = tpMake( 0, idx );
        for( uint i = 0 ; i < cap ; i++ )
            idxAddByLoc( state, idx, i );
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
    
    
    // Adjust size of value array if too small.
    if( i >= cap ) {
        Part valsP = {.ptr = vals, .sz = sizeof(TVal)*cap };
        
        int   ncap  = idx->nextLoc;
        TVal* nvals = stateResizeRaw( state, &valsP, ncap );
        for( uint j = cap ; j = ncap ; j++ )
            nvals[j] = tvUdf();
        
        rec->vals = tpMake( ncap, nvals );
        cap  = ncap;
        vals = nvals;
    }
    
    if( tvIsUdf( vals[i] ) )
        idxAddByLoc( i );
    
    vals[i] = val;
}

void
recSet( State* state, Record* rec, TVal key, TVal val ) {
    Index* idx  = tpGetPtr( rec->idx );
    uint   cap  = tpGetTag( rec->vals );
    TVal*  vals = tpGetPtr( rec->vals );
    
    uint i = idxGetByKey( state, idx, key );
    if( i >= cap || tvIsUdf( vals[i] ) )
        stateErrFmt( state, ten_ERR_REC, "Set of undefined record field" );
    if( tvIsUdf( val ) )
        stateErrFmt( state, ten_ERR_REC, "Field set to `udf`" );
    
    rec->vals[i] = val;
}

TVal
recGet( State* state, Record* rec, TVal key ) {
    Index* idx  = tpGetPtr( rec->idx );
    uint   cap  = tpGetTag( rec->vals );
    TVal*  vals = tpGetPtr( rec->vals );
    
    uint i = idxGetByKey( state, idx, key );
    if( i >= rec->cap || tvIsUdf( vals[i] ) )
        return tvUdf()
    else
        return vals[i];
}


void
recTraverse( State* state, Record* rec ) {
    Index* idx  = tpGetPtr( rec->idx );
    uint   cap  = tpGetTag( rec->vals );
    Value* vals = tpGetPtr( rec->vals );
    
    stateMark( state, idx );
    for( uint i = 0 ; i < cap ; i++ )
        tvMark( vals[i] );
}

void
recDestruct( State* state, Record* rec ) {
    Index* idx  = tpGetPtr( rec->idx );
    uint   cap  = tpGetTag( rec->vals );
    Value* vals = tpGetPtr( rec->vals );
    
    // If the Index hasn't been destructed yet then
    // unref all the record's keys.
    if( idx->map.cap > 0 )
    for( uint i = 0 ; i < cap ; i++ )
        idxRemByLoc( state, idx, i );
    
    stateFreeRaw( state, vals, sizeof(TVal)*cap );
}
