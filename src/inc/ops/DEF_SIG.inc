uint  cnt = 1;
TVal* vars = regs.sp - opr;
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
if( cnt > opr )
    stateErrFmtA(
        state, ten_ERR_ASSIGN,
        "Too many signal arguments"
    );

for( uint i = 0 ; i < opr ; i++ ) {
    TVal var = vars[i];
    TVal val = vals[i];
    
    tenAssert( tvIsRef( var ) );
    RefT  ref = tvGetRef( var );
    refDef( ref, val );
}

if( cnt != 1 )
    regs.sp -= cnt + 1 + opr;
else
    regs.sp -= cnt + opr;
