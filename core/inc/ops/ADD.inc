TVal arg1 = regs.sp[-2];
TVal arg2 = regs.sp[-1];
if( tvIsDec( arg1 ) && tvIsDec( arg2 ) ) {
    *(--regs.sp) = tvDec( tvGetDec( arg1 ) + tvGetDec( arg2 ) );
}
else
if( tvIsInt( arg1 ) && tvIsInt( arg2 ) ) {
    *(--regs.sp) = tvInt( tvGetInt( arg1 ) + tvGetInt( arg2 ) );
}
else {
    stateErrFmtA(
        state, ten_ERR_TYPE,
        "Wrong types (%t + %t), expected (Dec + Dec) or (Int + Int)",
        arg1, arg2
    );
}
