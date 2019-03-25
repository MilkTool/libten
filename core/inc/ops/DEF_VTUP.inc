uint  cnt = 1;
TVal* vals = regs.sp - 1;
if( tvIsTup( vals[0] ) ) {
    cnt  += tvGetTup( vals[0] );
    vals -= cnt;
    regs.sp--;
}
if( cnt < opr )
    stateErrFmtA(
        state, ten_ERR_ASSIGN,
        "Tuple assignment with mismatched sizes"
    );

tenAssert( tvIsObj( vals[-1] ) && datGetTag( tvGetObj( vals[-1] ) ) == OBJ_IDX );
Index* vIdx = tvGetObj( vals[-1] );

TVal vVar = vals[-2];
tenAssert( tvIsRef( vVar ) );
RefT vRef = tvGetRef( vVar );

TVal* vars = vals - 2 - opr;
for( uint i = 0 ; i < cnt ; i++ ) {
    TVal var = vars[i];
    TVal val = vals[i];
    
    tenAssert( tvIsRef( var ) );
    refDef( tvGetRef( var ), val );
}

Record* vRec = recNew( state, vIdx );
refDef( vRef, tvObj( vRec ) );

for( uint i = opr ; i < cnt ; i++ ) {
    TVal key = tvInt( i - opr );
    TVal val = vals[i];
    
    recDef( state, vRec, key, val );
}

regs.sp -= cnt + opr;
regs.sp[-1] = tvUdf();
