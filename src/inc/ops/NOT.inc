TVal arg = regs.sp[-1];
if( tvIsLog( arg ) ) {
    regs.sp[-1] = tvLog( !tvGetLog( arg ) );
}
else
if( tvIsInt( arg ) ) {
    regs.sp[-1] = tvInt( ~tvGetInt( arg ) );
}
else {
    stateErrFmtA(
        state, ten_ERR_TYPE,
        "Wrong type (~ %t), expected (~ Log) or (~ Int)",
        arg
    );
}
