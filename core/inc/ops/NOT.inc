TVal arg = regs.sp[-1];
if( tvIsLog( arg ) ) {
    regs.sp[-1] = tvLog( !tvGetLog( regs.sp[-1] ) );
}
else
if( tvIsInt( arg ) ) {
    regs.sp[-1] = tvInt( ~tvGetInt( regs.sp[-1] ) );
}
else {
    stateErrFmtA(
        state, ten_ERR_TYPE,
        "Wrong type (~ %t), expected (~ Log) or (~ Int)",
        arg
    );
}
