TVal arg = regs.sp[-1];
if( tvIsUdf( arg ) )
    regs.sp[-1] = tvNil();
else {
    if( tvIsTup( arg ) )
        stateErrFmtA(
            state, ten_ERR_TYPE,
            "Passed tuple to operator."
        );
}
