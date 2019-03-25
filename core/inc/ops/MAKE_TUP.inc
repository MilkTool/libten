TVal* vals = regs.sp - opr;
if( opr )
    stateErrFmtA(
        state, ten_ERR_TUPLE,
        "Tuple exceeds max size"
    );

for( uint i = 0 ; i < opr ; i++ ) {
    if( tvIsTup( vals[i] ) )
        stateErrFmtA(
            state, ten_ERR_TUPLE,
            "Nested tuple"
        );
}

*(regs.sp++) = tvTup( opr );
