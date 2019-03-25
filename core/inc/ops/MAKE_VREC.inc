TVal* sv = regs.sp - 1;
tenAssert( tvIsObj( *sv ) );
tenAssert( datGetTag( tvGetObj( *sv ) ) == OBJ_REC );
Record* src = tvGetObj( *sv );

TVal* iv = regs.sp - opr*2 - 2;
tenAssert( tvIsObj( *iv ) );
tenAssert( datGetTag( tvGetObj( *iv ) ) == OBJ_IDX );
Index*  idx = tvGetObj( *iv );
Record* rec = recNew( state, idx );
*iv = tvObj( rec );

TVal (*pairs)[2] = (TVal (*)[2])(iv + 1);

Index* srcIdx  = tpGetPtr( rec->idx );
TVal*  srcVals = tpGetPtr( rec->vals );
uint   srcCap  = tpGetTag( rec->vals );
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
