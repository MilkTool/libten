TVal rec = regs.sp[-2];
TVal key = regs.sp[-1];

tenAssert( tvIsObjType( rec, OBJ_REC ) );
regs.sp--;
regs.sp[-1] = recGet( state, tvGetObj( rec ), key );
