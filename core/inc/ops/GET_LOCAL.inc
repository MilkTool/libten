#ifdef ten_DEBUG
    VirFun* fun = &regs.cls->fun->u.vir;
    tenAssert( opr < fun->nLocals );
#endif

*(regs.sp++) = regs.lcl[opr];
