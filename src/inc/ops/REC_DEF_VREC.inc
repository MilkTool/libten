TVal src = regs.sp[-1];
if( !tvIsObj( src ) || datGetTag( tvGetObj( src ) ) != OBJ_REC )
    stateErrFmtA(
        state, ten_ERR_ASSIGN,
        "Record assignment from non-Rec value"
    );

TVal dst = regs.sp[-2LL*opr - 4];
if( !tvIsObj( dst ) || datGetTag( tvGetObj( dst ) ) != OBJ_REC )
    stateErrFmtA(
        state, ten_ERR_ASSIGN,
        "Record assignment to non-Rec value"
    );

tenAssert( tvIsObj( regs.sp[-2] ) && datGetTag( tvGetObj( regs.sp[-2] ) ) == OBJ_IDX );
Index* vIdx = tvGetObj( regs.sp[-2] );
TVal   vKey = regs.sp[-3];

TVal (*pairs)[2] = (TVal (*)[2])(regs.sp - 3 - 2*opr);

Record* dRec = tvGetObj( dst );
Record* sRec = tvGetObj( src );

Index* srcIdx  = tpGetPtr( sRec->idx );
TVal*  srcVals = sRec->vals;
uint   srcCap  = sRec->cap;
bool   srcMrk[srcCap]; memset( srcMrk, 0, sizeof(bool)*srcCap );

for( uint i = 0 ; i < opr ; i++ ) {
    TVal dKey = pairs[i][0];
    TVal sKey = pairs[i][1];
    
    uint loc = idxGetByKey( state, srcIdx, sKey );
    TVal val;
    if( loc >= srcCap ) {
        val = tvUdf();
    }
    else {
        val = srcVals[loc];
        srcMrk[loc] = true;
    }
    
    recDef( state, dRec, dKey, val );
}


Record* vRec = recNew( state, vIdx );
recDef(  state, dRec, vKey, tvObj( vRec ) );

IdxIter* iter = idxIterMake( state, srcIdx );
TVal key;
uint loc;
while( idxIterNext( state, iter, &key, &loc ) ) {
    TVal val = loc >= srcCap ? tvUdf() : srcVals[loc];
    
    if( !tvIsUdf( val ) && !srcMrk[loc] )
        recDef( state, vRec, key, val );
}

regs.sp -= 3 + 2*opr;
regs.sp[-1] = tvUdf();
