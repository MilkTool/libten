// Figure out how many arguments were passed,
// and where they start.
TVal* args = &regs.sp[-1];
uint  argc = 1;
TVal* argv = args - 1;
if( tvIsTup( *args ) ) {
    argc = tvGetTup( *args );
    argv -= argc;
    tenAssert( argc < args - fib->stack.buf );
    
    // Pop the tuple header, it's no longer needed.
    regs.sp--;
}

// First value in argv is the closure being called.
if( !tvIsObjType( argv[0], OBJ_CLS ) )
    stateErrFmtA(
        state, ten_ERR_TYPE,
        "Attempt to call non-Cls type %t",
        argv[0]
    );

Closure* cls = tvGetObj( argv[0] );
uint parc = cls->fun->nParams;

// Check the arguments for `udf`.
for( uint i = 1 ; i <= argc ; i++ )
    if( tvIsUdf( argv[i] ) )
        errUdfAsArg( state, cls->fun, i );

// If too few arguments were passed then it's an error.
if( argc < parc )
    errTooFewArgs( state, cls->fun, argc );

// If the function expects a variadic argument record.
if( cls->fun->vargIdx ) {
    
    // Put the varg record in a temporary to keep
    // if from being collected.
    Record* rec = recNew( state, cls->fun->vargIdx );
    stateTmp( state, tvObj( rec ) );
    
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
// Otherwise the parameter count must be matched by the arguments.
else {
    if( argc > parc )
        errTooManyArgs( state, cls->fun, argc );
}

// For a tail call we copy the arguments and the closure
// itself down the stack to replace the previous call's
// frame.  We also don't save the activation record.
if( inGetOpc( *regs.ip ) == OPC_RETURN && fib->virs.top > 0 ) {
    for( uint i = 0 ; i <= argc ; i++ )
        regs.lcl[i] = argv[i];
    argv = regs.lcl;
}
else {
    fib->push( state, fib, NULL );
    regs.lcl = argv;
}

regs.cls = cls;
if( cls->fun->type == FUN_VIR ) {
    VirFun* fun = & cls->fun->u.vir;
    ensureStack( state, fib, fun->nLocals + fun->nTemps );
    
    TVal* locals = regs.lcl + argc + 1;
    for( uint i = 0 ; i < fun->nLocals ; i++ )
        locals[i] = tvUdf();
    
    regs.sp += fun->nLocals;
    regs.ip = cls->fun->u.vir.code;
}
else {
        
    regs.ip         = NULL;
    regs.context    = NULL;
    regs.checkpoint = -1;
    
    ten_Call call = { .ten = (ten_State*)state };
    *(Tup*)&call.args = (Tup) {
        .base   = &fib->stack.buf,
        .offset = regs.lcl - fib->stack.buf + 1,
        .size   = argc
    };
    
    if( cls->dat.dat ) {
        Data* dat = cls->dat.dat;
        *(Tup*)&call.mems = (Tup) {
            .base   = &dat->mems,
            .offset = 0,
            .size   = dat->info->nMems
        };
        call.data = dat->data;
    }
    else {
        *(Tup*)&call.mems = (Tup) {
            .base   = NULL,
            .offset = 0,
            .size   = 0
        };
    }
    ten_Tup t = cls->fun->u.nat.cb( &call );
    
    Tup*  rets = (Tup*)&t;
    uint  retc = rets->size;
    ensureStack( state, fib, retc + 1 );
    
    TVal* retv = *rets->base + rets->offset;
    TVal* dstv = regs.lcl;
    for( uint i = 0 ; i < retc ; i++ )
        dstv[i] = retv[i];
    regs.sp = dstv + retc;
    if( retc != 1 )
        *(regs.sp++) = tvTup( retc );
    
    VirAR* top = &fib->virs.buf[fib->virs.top-1];
    if( fib->virs.top == 0 || top->nats )
        EXIT;
    else
        fib->pop( state, fib );
}
