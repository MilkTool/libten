VirFun* fun = &regs.cls->fun->u.vir;
ushort  opr = inGetOpr( in );
tenAssert( fun->nConsts > opr );

*(regs.sp++) = fun->consts[opr];
