if( !tvIsObj( regs.sp[-1] ) || datGetTag( tvGetObj( regs.sp[-1] ) ) != OBJ_REC )
    stateErrFmtA(
        state, ten_ERR_ASSIGN,
        "Record assignment from non-Rec value"
    );

tenAssert( tvIsObj( regs.sp[-2] ) && datGetTag( tvGetObj( regs.sp[-2] ) ) == OBJ_IDX );

Record* rec  = tvGetObj( regs.sp[-1] );
Index*  vIdx = tvGetObj( regs.sp[-2] );
TVal    vVar = regs.sp[-3];

tenAssert( tvIsRef( vVar ) );
RefT vRef = tvGetRef( vVar );

Index* recIdx  = tpGetPtr( rec->idx );
TVal*  recVals = rec->vals;
uint   recCap  = rec->cap;
bool   recMrk[recCap]; memset( recMrk, 0, sizeof(bool)*recCap );

TVal (*pairs)[2] = (TVal (*)[2])(regs.sp - 3 - 2*opr);
for( uint i = 0 ; i < opr ; i++ ) {
    TVal var = pairs[i][0];
    TVal key = pairs[i][1];
    
    uint loc = idxGetByKey( state, recIdx, key );
    TVal val;
    if( loc >= recCap ) {
        val = tvUdf();
    }
    else {
        val = recVals[loc];
        recMrk[loc] = true;
    }
    
    tenAssert( tvIsRef( var ) );
    RefT  ref = tvGetRef( var );
    refDef( ref, val );
}

Record* vRec = recNew( state, vIdx );
refDef( vRef, tvObj( vRec ) );

IdxIter* iter = idxIterMake( state, recIdx );
TVal key;
uint loc;
while( idxIterNext( state, iter, &key, &loc ) ) {
    TVal val = loc >= recCap ? tvUdf() : recVals[loc];
    
    if( !tvIsUdf( val ) && !recMrk[loc] )
        recDef( state, vRec, key, val );
}

regs.sp -= 2 + 2*opr;
regs.sp[-1] = tvUdf();
