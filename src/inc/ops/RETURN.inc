// Copy return values downt to the start of the call frame.
uint  retc = 1;
TVal* retv = regs.sp - 1;
if( tvIsTup( *retv ) ) {
    retc += tvGetTup( *retv );
    retv -= retc - 1;
}
TVal* dstv = regs.lcl;
for( uint i = 0 ; i < retc ; i++ )
    dstv[i] = retv[i];
regs.sp = dstv + retc;


if( fib->arStack.top == 0 || fib->arStack.ars[fib->arStack.top-1].nats )
    goto end;
else
    popAR( state, fib );
