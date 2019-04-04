#ifdef ten_DEBUG
    Function* fun = regs.cls->fun;
    VirFun*   vir = &fun->u.vir;
    tenAssert( opr < vir->nLocals + fun->nParams + 1 );
#endif

*(regs.sp++) = regs.lcl[opr];
