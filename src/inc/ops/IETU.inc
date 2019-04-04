TVal arg1 = regs.sp[-2];
TVal arg2 = regs.sp[-1];
regs.sp--;
regs.sp[-1] = tvLog( tvEqual( arg1, arg2 ) );
