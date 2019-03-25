TVal arg1 = regs.sp[-2];
TVal arg2 = regs.sp[-1];
regs.sp--;
if( tvIsUdf( arg1 ) || tvIsUdf( arg2 ) )
    stateErrFmtA(
        state, ten_ERR_TYPE,
        "Comparison with Udf"
    );
regs.sp[-1] = tvLog( !tvEqual( arg1, arg2 ) );
