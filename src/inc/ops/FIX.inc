TVal arg = regs.sp[-1];
if( tvIsUdf( arg ) )
    regs.sp[-1] = tvNil();
