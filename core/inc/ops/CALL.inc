bool tail;
if( inGetOpc( *regs.ip ) == OPC_RETURN ) {
    tail = true;
}
else {
    pushAR( state, fib, NULL );
    tail = false;
}

// Figure out how many arguments were passed,
// and where they start.
TVal* args = &regs.sp[-1];
uint  argc = 1;
TVal* argv = args - 1;
if( tvIsTup( *args ) ) {
    argc = tvGetTup( *args );
    argv -= argc;
    tenAssert( argc < args - fib->tmpStack.tmps );
    
    // Pop the tuple header, it's no longer needed.
    regs.sp--;
}

// First value in argv is the closure being called.
tenAssert( tvIsObj( argv[0] ) );
tenAssert( datGetTag( tvGetObj( argv[0] ) ) == OBJ_CLS );

Closure* cls = tvGetObj( argv[0] );
uint parc = cls->fun->nParams;

// Check the arguments for `udf`.
for( uint i = 1 ; i <= argc ; i++ )
    if( tvIsUdf( argv[i] ) )
        errUdfAsArg( state, cls->fun, i );

// If too few arguments were passed then it's an error.
if( argc < parc )
    errTooFewArgs( state, cls->fun, argc );

// If there are too many arguments and the function
// doesn't accept variatic arguments then it's an
// error.  If the function does accept vargs then
// the extra arguments need to be copied to a record.
if( argc > parc ) {
    if( cls->fun->vargIdx == NULL )
        errTooManyArgs( state, cls->fun, argc );
    
    // Put the varg record on the stack to keep it
    // from getting garbage collected.
    ensureStack( state, fib, 1 );
    Record* rec = recNew( state, cls->fun->vargIdx );
    *(fib->rPtr->sp++) = tvObj( rec );
    
    uint  diff  = argc - parc;
    TVal* extra = &argv[1 + parc];
    for( uint i = 0 ; i < diff ; i++ ) {
        TVal key = tvInt( i );
        recDef( state, rec, key, extra[i] );
    }
    
    // Record is set as the last argument, after the
    // place of the last non-variatic parameter.
    extra[0] = tvObj( rec );
    argc = parc + 1;
    
    // Adjust the stack pointer to again point to the
    // slot just after the arguments.
    regs.sp = extra + 1;
}

// For a tail call we copy the arguments and the closure
// itself down the stack to replace the previous call's
// frame.
if( tail ) {
    for( uint i = 0 ; i <= argc ; i++ )
        regs.lcl[i] = argv[i];
    argv = regs.lcl;
}
else {
    regs.lcl = argv;
}

regs.cls = cls;
if( cls->fun->type == FUN_VIR ) {
    VirFun* fun = & cls->fun->u.vir;
    ensureStack( state, fib, fun->nLocals + fun->nTemps );
    
    regs.sp += fun->nLocals;
    regs.ip = cls->fun->u.vir.code;
}
else {

    // This is used below to make sure the function call left
    // a return value on the stack.
    #ifndef rigK_NDEBUG
        uint oTop = regs.sp - fib->tmpStack.tmps;
    #endif

    regs.ip = NULL;
    
    // Initialize an argument tuple for the callback.
    Tup aTup = {
        .base   = &fib->tmpStack.tmps,
        .offset = regs.lcl - fib->tmpStack.tmps + 1,
        .size   = argc
    };
    
    // If a Data object is attached to the closure
    // then we need to initialize a tuple for its
    // members as well, otherwise pass NULL.
    if( cls->dat.dat != NULL ) {
        Data* dat = cls->dat.dat;
        Tup mTup = {
            .base   = &dat->mems,
            .offset = 0,
            .size   = dat->info->nMems
        };
        cls->fun->u.nat.cb( (ten_State*)state, (ten_Tup*)&aTup, (ten_Tup*)&mTup, dat->data );
    }
    else {
        cls->fun->u.nat.cb( (ten_State*)state, (ten_Tup*)&aTup, NULL, NULL );
    }
    // Make sure the call left a return value.
    tenAssert( oTop < regs.sp - fib->tmpStack.tmps );

    #include "RETURN.inc"
}
