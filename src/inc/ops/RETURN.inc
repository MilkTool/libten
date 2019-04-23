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


VirAR* top = &fib->virs.buf[fib->virs.top-1];
if( fib->virs.top == 0 || top->nats )
    EXIT;
else
    fib->pop( state, fib );
