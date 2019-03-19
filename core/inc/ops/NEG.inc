TVal arg = regs.sp[-1];
if( tvIsDec( arg ) ) {
    regs.sp[-1] = tvDec( -tvGetDec( regs.sp[-1] ) );
}
else
if( tvIsInt( arg ) ) {
    regs.sp[-1] = tvInt( -tvGetInt( regs.sp[-1] ) );
}
else {
    stateErrFmtA(
        state, ten_ERR_TYPE,
        "Wrong type (- %t), expected (- Int) or (- Dec)",
        arg
    );
}
