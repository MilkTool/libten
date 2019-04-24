TVal arg1 = regs.sp[-2];
TVal arg2 = regs.sp[-1];
if( tvIsTup( arg1 ) || tvIsTup( arg2 ) )
    stateErrFmtA(
        state, ten_ERR_TYPE,
        "Passed tuple to operator."
    );

regs.sp--;
regs.sp[-1] = tvLog( tvEqual( arg1, arg2 ) );
