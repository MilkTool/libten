TVal arg = regs.sp[-1];
if( tvIsLog( arg ) ) {
    regs.sp[-1] = tvLog( !tvGetLog( arg ) );
}
else
if( tvIsInt( arg ) ) {
    regs.sp[-1] = tvInt( ~tvGetInt( arg ) );
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
            "Wrong type (~ %t), expected (~ Log) or (~ Int)",
            arg
        );
}
