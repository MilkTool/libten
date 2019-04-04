TVal arg = regs.sp[-1];
regs.sp--;

if( tvIsTup( arg ) )
    regs.sp -= tvGetTup( arg );

tenAssert( regs.sp >= fib->tmpStack.tmps );
