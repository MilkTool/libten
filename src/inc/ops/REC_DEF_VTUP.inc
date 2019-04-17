uint  cnt = 1;
TVal* vals = regs.sp - 1;
if( tvIsTup( vals[0] ) ) {
    cnt  = tvGetTup( vals[0] );
    vals = --regs.sp - cnt;
}
if( cnt < opr )
    stateErrFmtA(
        state, ten_ERR_ASSIGN,
        "Tuple assignment with mismatched sizes"
    );

tenAssert( tvIsObj( vals[-1] ) && datGetTag( tvGetObj( vals[-1] ) ) == OBJ_IDX );
Index* vIdx = tvGetObj( vals[-1] );

TVal vKey = vals[-2];

TVal* keys = vals - 2 - opr;

TVal dst = keys[-1];
if( !tvIsObj( dst ) || datGetTag( tvGetObj( dst ) ) != OBJ_REC )
    stateErrFmtA(
        state, ten_ERR_ASSIGN,
        "Record assignment to non-Rec value"
    );

Record* dRec = tvGetObj( dst );
for( uint i = 0 ; i < opr ; i++ )
    recDef( state, dRec, keys[i], vals[i] );

Record* vRec = recNew( state, vIdx );
recDef( state, dRec, vKey, tvObj( vRec ) );

for( uint i = opr ; i < cnt ; i++ ) {
    TVal key = tvInt( i - opr );
    TVal val = vals[i];
    
    recDef( state, vRec, key, val );
}

regs.sp -= cnt + opr + 1;
regs.sp[-1] = tvUdf();
