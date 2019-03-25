VirFun* fun = &regs.cls->fun->u.vir;
tenAssert( opr < fun->nLabels );

TVal arg = regs.sp[-1];
instr* dst = fun->labels[opr];
if( !tvIsUdf( arg ) ) {
    regs.ip = dst;
}
else {
    regs.sp--;
}
