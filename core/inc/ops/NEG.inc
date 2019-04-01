TVal arg = regs.sp[-1];
if( tvIsDec( arg ) ) {
    regs.sp[-1] = tvDec( -tvGetDec( arg ) );
}
else
if( tvIsInt( arg ) ) {
    regs.sp[-1] = tvInt( -tvGetInt( arg ) );
}
else {
    stateErrFmtA(
        state, ten_ERR_TYPE,
        "Wrong type (- %t), expected (- Int) or (- Dec)",
        arg
    );
}
