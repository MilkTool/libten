uint  cnt = 1;
TVal* vals = regs.sp - 1;
if( tvIsTup( vals[0] ) ) {
    cnt  = tvGetTup( vals[0] );
    vals = --regs.sp - cnt;
}
if( cnt != opr )
    stateErrFmtA(
        state, ten_ERR_ASSIGN,
        "Tuple assignment with mismatched sizes"
    );

TVal* vars = vals - opr;
for( uint i = 0 ; i < cnt ; i++ ) {
    TVal var = vars[i];
    TVal val = vals[i];
    
    tenAssert( tvIsRef( var ) );

    RefT  ref = tvGetRef( var );
    refSet( ref, val );
}

regs.sp -= 2*cnt;
*(regs.sp++) = tvUdf();
