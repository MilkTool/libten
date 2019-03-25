TVal* val = envGetGlobalByLoc( state, opr );
tenAssert( val );
*(regs.sp++) = *val;
