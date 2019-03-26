uint  cnt = 1;
TVal* vals = regs.sp - 1;
if( tvIsTup( vals[0] ) ) {
    cnt  += tvGetTup( vals[0] );
    vals = regs.sp - cnt;
    regs.sp--;
}
if( cnt != opr )
    stateErrFmtA(
        state, ten_ERR_ASSIGN,
        "Tuple assignment with mismatched sizes"
    );

TVal* vars = vals - opr;
for( uint i = 0 ; i < opr ; i++ ) {
    TVal var = vars[i];
    TVal val = vals[i];
    
    tenAssert( tvIsRef( var ) );
    RefT  ref = tvGetRef( var );
    refDef( ref, val );
}

regs.sp -= 2*cnt;
*(regs.sp++) = tvUdf();
