TVal arg1 = regs.sp[-2];
TVal arg2 = regs.sp[-1];
regs.sp--;
if( tvIsInt( arg1 ) && tvIsInt( arg2 ) ) {
    regs.sp[-1] = tvInt( tvGetInt( arg1 ) | tvGetInt( arg2 ) );
}
else {
    stateErrFmtA(
        state, ten_ERR_TYPE,
        "Wrong types (%t | %t), expected (Int | Int)",
        arg1, arg2
    );
}
