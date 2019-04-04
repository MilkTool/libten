VirFun* fun = &regs.cls->fun->u.vir;
tenAssert( opr < fun->nLabels );
regs.ip = fun->labels[opr];
