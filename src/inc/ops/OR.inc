TVal arg1 = regs.sp[-2];
TVal arg2 = regs.sp[-1];
regs.sp--;
if( tvIsInt( arg1 ) && tvIsInt( arg2 ) ) {
    regs.sp[-1] = tvInt( tvGetInt( arg1 ) | tvGetInt( arg2 ) );
}
else
if( tvIsLog( arg1 ) && tvIsLog( arg2 ) ) {
    regs.sp[-1] = tvLog( tvGetLog( arg1 ) || tvGetLog( arg2 ) );
}
else {
    if( tvIsTup( arg1 ) || tvIsTup( arg2 ) )
        stateErrFmtA(
            state, ten_ERR_TYPE,
            "Passed tuple to operator."
        );
    else
        stateErrFmtA(
            state, ten_ERR_TYPE,
            "Wrong types (%t | %t), expected (Log | Log) or (Int | Int)",
            arg1, arg2
        );
}
