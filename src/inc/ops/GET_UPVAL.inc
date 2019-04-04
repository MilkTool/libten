VirFun* fun = &regs.cls->fun->u.vir;
tenAssert( fun->nUpvals > opr );

if( regs.cls->dat.upvals[opr] )
    *(regs.sp++) = regs.cls->dat.upvals[opr]->val;
else
    *(regs.sp++) = tvUdf();
