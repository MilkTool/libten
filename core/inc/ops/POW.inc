TVal arg1 = regs.sp[-2];
TVal arg2 = regs.sp[-1];
regs.sp--;
if( tvIsDec( arg1 ) && tvIsDec( arg2 ) ) {
    DecT a = tvGetDec( arg1 );
    DecT b = tvGetDec( arg2 );
    if( a == 0.0 && b == 0.0 )
        stateErrFmtA(
            state, ten_ERR_ARITH,
            "Zero with zero exponent"
        );
    if( a == 0.0 && b < 0.0 )
        stateErrFmtA(
            state, ten_ERR_ARITH,
            "Zero with negative exponent"
        );
    if( a < 0.0 && b != trunc( b ) )
        stateErrFmtA(
            state, ten_ERR_ARITH,
            "Exponent of negative number must be whole number"
        );
    
    double r = pow( a, b );
    if( isnan( r ) )
        stateErrFmtA(
            state, ten_ERR_ARITH,
            "Power function returned NaN"
        );
    
    regs.sp[-1] = tvDec( r );
}
else
if( tvIsInt( arg1 ) && tvIsInt( arg2 ) ) {
    IntT a = tvGetInt( arg1 );
    IntT b = tvGetInt( arg2 );
    if( a == 0 && b == 0 )
        stateErrFmtA(
            state, ten_ERR_ARITH,
            "Zero with zero exponent"
        );
    if( a == 0 && b < 0 )
        stateErrFmtA(
            state, ten_ERR_ARITH,
            "Zero with negative exponent"
        );
    if( a == 1 ) {
        regs.sp[-1] = tvInt( 1 );
        break;
    }
    if( b < 0 ) {
        regs.sp[-1] = tvInt( 0 );
        break;
    }
    
    IntT r = 1;
    for( IntT i = 0 ; i < b ; i++ )
        r *= a;
    
    regs.sp[-1] = tvInt( r );
}
else {
    stateErrFmtA(
        state, ten_ERR_TYPE,
        "Wrong types (%t ^ %t), expected (Dec ^ Dec) or (Int ^ Int)",
        arg1, arg2
    );
}
