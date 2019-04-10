TVal* sv = regs.sp - 1;
if( !tvIsObjType( *sv, OBJ_REC ) )
    stateErrFmtA(
        state, ten_ERR_TYPE,
        "Record expansion attempted on non-Rec type %t",
        *sv
    );
Record* src = tvGetObj( *sv );

TVal (*pairs)[2] = (TVal (*)[])(regs.sp - 1 - opr*2);

TVal* iv = (TVal*)pairs - 1;
tenAssert( tvIsObj( *iv ) );
tenAssert( datGetTag( tvGetObj( *iv ) ) == OBJ_IDX );
Index*  idx = tvGetObj( *iv );
Record* rec = recNew( state, idx );
*iv = tvObj( rec );

Index* srcIdx  = tpGetPtr( src->idx );
TVal*  srcVals = src->vals;
uint   srcCap  = src->cap;
bool   srcMrk[srcCap]; memset( srcMrk, 0, sizeof(bool)*srcCap );

for( uint i = 0 ; i < opr ; i++ ) {
    recDef( state, rec, pairs[i][0], pairs[i][1] );
    uint loc = idxGetByKey( state, srcIdx, pairs[i][0] );
    if( loc < srcCap )
        srcMrk[i] = true;
}

IdxIter* iter = idxIterMake( state, srcIdx );
TVal key;
uint loc;
while( idxIterNext( state, iter, &key, &loc ) ) {
    TVal val = loc >= srcCap ? tvUdf() : srcVals[loc];
    
    if( !tvIsUdf( val ) && !srcMrk[loc] )
        recDef( state, rec, key, val );
}

regs.sp -= opr*2 + 1;
