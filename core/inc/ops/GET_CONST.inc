VirFun* fun = &regs.cls->fun->u.vir;
tenAssert( fun->nConsts > opr );

*(regs.sp++) = fun->consts[opr];
