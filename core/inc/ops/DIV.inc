TVal arg1 = regs.sp[-2];
TVal arg2 = regs.sp[-1];
if( tvIsDec( arg1 ) && tvIsDec( arg2 ) ) {
    DecT denom = tvGetDec( arg2 );
    if( denom == 0.0 )
        stateErrFmtA(
            state, ten_ERR_ARITH,
            "Division by zero"
        );
    
    *(--regs.sp) = tvDec( tvGetDec( arg1 ) / denom );
}
else
if( tvIsInt( arg1 ) && tvIsInt( arg2 ) ) {
    IntT denom = tvGetInt( arg2 );
    if( denom == 0 )
        stateErrFmtA(
            state, ten_ERR_ARITH,
            "Division by zero"
        );
    
    *(--regs.sp) = tvInt( tvGetInt( arg1 ) / denom );
}
else {
    stateErrFmtA(
        state, ten_ERR_TYPE,
        "Wrong types (%t / %t), expected (Dec / Dec) or (Int / Int)",
        arg1, arg2
    );
}
