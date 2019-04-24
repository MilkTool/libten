TVal arg1 = regs.sp[-2];
TVal arg2 = regs.sp[-1];
regs.sp--;
if( tvIsInt( arg1 ) && tvIsInt( arg2 ) ) {
    IntT shift = tvGetInt( arg2 );
    if( shift < 0 )
        regs.sp[-1] = tvInt( (uint32_t)tvGetInt( arg1 ) << -shift );
    else
        regs.sp[-1] = tvInt( (uint32_t)tvGetInt( arg1 ) >> shift );
}
else {
    if( tvIsTup( arg1 ) || tvIsTup( arg2 ) )
        stateErrFmtA(
            state, ten_ERR_TYPE,
            "Passed tuple to operator."
        );
    else
        stateErrFmtA(
            state, ten_ERR_TYPE,
            "Wrong types (%t >> %t), expected (Int >> Int)",
            arg1, arg2
        );
}
