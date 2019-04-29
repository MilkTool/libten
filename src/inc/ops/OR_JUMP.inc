VirFun* fun = &regs.cls->fun->u.vir;
tenAssert( opr < fun->nLabels );

TVal arg = regs.sp[-1];
if( tvIsUdf( arg ) )
    stateErrFmtA(
        state, ten_ERR_TYPE,
        "Conditional expression passed Udf"
    );
if( tvIsTup( arg ) )
    stateErrFmtA(
        state, ten_ERR_TYPE,
        "Conditional expression passed Tup"
    );

instr* dst = fun->labels[opr];
if( ( tvIsLog( arg ) && !tvGetLog( arg ) ) || tvIsNil( arg ) ) {
    regs.sp--;
}
else {
    regs.ip = dst;
}
