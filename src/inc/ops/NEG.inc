TVal arg = regs.sp[-1];
if( tvIsDec( arg ) ) {
    regs.sp[-1] = tvDec( -tvGetDec( arg ) );
}
else
if( tvIsInt( arg ) ) {
    regs.sp[-1] = tvInt( -tvGetInt( arg ) );
}
else {
    if( tvIsTup( arg ) )
        stateErrFmtA(
            state, ten_ERR_TYPE,
            "Passed tuple to operator."
        );
    else
        stateErrFmtA(
            state, ten_ERR_TYPE,
            "Wrong type (- %t), expected (- Int) or (- Dec)",
            arg
        );
}
