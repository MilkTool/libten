TVal* idx = regs.sp - 1;
tenAssert( tvIsObjType( *idx, OBJ_IDX ) );
Index* vIdx = tvGetObj( *idx );

uint  cnt = 1;
TVal* vars = idx - opr - 1;
TVal* vals = vars - 1;
if( tvIsTup( vals[0] ) ) {
    cnt  = tvGetTup( vals[0] );
    vals -= cnt;
}
if( cnt < opr )
    stateErrFmtA(
        state, ten_ERR_ASSIGN,
        "Too few signal arguments"
    );

TVal vVar = vars[opr];
tenAssert( tvIsRef( vVar ) );
RefT vRef = tvGetRef( vVar );

for( uint i = 0 ; i < opr ; i++ ) {
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

if( cnt != 1 )
    regs.sp -= cnt + opr + 3;
else
    regs.sp -= cnt + opr + 2;
