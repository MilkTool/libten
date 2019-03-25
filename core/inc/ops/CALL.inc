if( inGetOpc( *regs.ip ) == OPC_RETURN ) {
    regs.ip++;
    doCall( state, fib, true );
}
else {
    pushAR( state, fib, NULL );
    doCall( state, fib, false );
}
