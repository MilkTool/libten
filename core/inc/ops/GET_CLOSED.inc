#ifdef ten_DEBUG
    VirFun* fun = &regs.cls->fun->u.vir;
    tenAssert( opr < fun->nLocals );
    tenAssert( tvIsObj( regs.lcl[opr] ) );
    tenAssert( datGetTag( tvGetObj( regs.lcl[opr] ) ) == OBJ_UPV );
#endif

*(regs.sp++) = ((Upvalue*)tvGetObj( regs.lcl[opr] ))->val;
