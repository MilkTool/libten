
ten_Tup
tf_reentry( ten_PARAMS ) {
    struct {
        ten_Tup args;
        ten_Tup rets;
        long    num;
    } ctx;
    
    ten_Var clsArg = { .tup = args, .loc = 0 };
    ten_expect( ten, "cls", ten_clsType( ten ), &clsArg );
    
    ten_Var numRet = { .tup = &ctx.rets, .loc = 0 };
    
    switch( ten_seek( ten, &ctx, sizeof(ctx) ) ) {
        case 0: goto cp0;
        case 1: goto cp1;
    }
    
    ten_checkpoint( ten, 0, &ctx.rets );
        ctx.args = ten_pushA( ten, "" );
        ctx.rets = ten_call( ten, &clsArg, &ctx.args );
    cp0:
    
    ten_expect( ten, "callback return", ten_intType( ten ), &numRet );
    ctx.num = ten_getInt( ten, &numRet );
    if( ctx.num != 123 )
        ten_panic( ten, ten_int( ten, 1 ) );
    
    ten_checkpoint( ten, 1, &ctx.rets );
        ctx.num = 321;
        ten_yield( ten, &ctx.args );
    cp1:
    
    if( ctx.num != 321 )
        ten_panic( ten, ten_int( ten, 2 ) );
    
    return ten_pushA( ten, "" );
}

void
fiber_reentry1( ten_State* ten ) {
    ten_Tup vars = ten_pushA( ten, "UUUUU" );
    ten_Var reentFunVar = { .tup = &vars, .loc = 0 };
    ten_Var reentClsVar = { .tup = &vars, .loc = 1 };
    ten_Var yieldClsVar = { .tup = &vars, .loc = 2 };
    ten_Var mainClsVar  = { .tup = &vars, .loc = 3 };
    ten_Var mainFibVar  = { .tup = &vars, .loc = 4 };
    
    ten_FunParams reentp = {
        .name   = "reentry",
        .params = (char const*[]){ "cls", NULL },
        .cb     = tf_reentry
    };
    ten_newFun( ten, &reentp, &reentFunVar );
    ten_newCls( ten, &reentFunVar, NULL, &reentClsVar );
    
    ten_Source* ysrc = ten_stringSource(
        ten,
        "do yield() for 123",
        "yielder.ten"
    );
    ten_compileExpr( ten, NULL, ysrc, ten_SCOPE_LOCAL, ten_COM_CLS, &yieldClsVar );
    
    ten_Source* msrc = ten_stringSource(
        ten,
        "reentry( yielder )",
        "fiber.inc"
    );
    char const* upvals[] = { "reentry", "yielder", NULL };
    ten_compileScript( ten, NULL, msrc, ten_SCOPE_LOCAL, ten_COM_CLS, &mainClsVar );
    ten_setUpvalue( ten, &mainClsVar, 0, &reentClsVar );
    ten_setUpvalue( ten, &mainClsVar, 1, &yieldClsVar );
    
    ten_newFib( ten, &mainClsVar, NULL, &mainFibVar );
    
    ten_Tup args = ten_pushA( ten, "" );
    ten_cont( ten, &mainFibVar, &args );
    ten_propError( ten, &mainFibVar );
    ten_cont( ten, &mainFibVar, &args );
    ten_propError( ten, &mainFibVar );
    ten_cont( ten, &mainFibVar, &args );
    ten_propError( ten, &mainFibVar );
}

ten_Tup
tf_nreentry( ten_PARAMS ) {
    ten_Tup vals = ten_pushA( ten, "" );
    ten_yield( ten, &vals );
    
    return ten_pushA( ten, "I", 123 );
}

void
fiber_reentry2( ten_State* ten ) {
    ten_Tup vars = ten_pushA( ten, "UUU" );
    ten_Var nreentFunVar = { .tup = &vars, .loc = 0 };
    ten_Var nreentClsVar = { .tup = &vars, .loc = 1 };
    ten_Var nreentFibVar = { .tup = &vars, .loc = 2 };
    
    ten_FunParams nreentp = {
        .name   = "nreentry",
        .params = (char const*[]){ "cls", NULL },
        .cb     = tf_nreentry
    };
    ten_newFun( ten, &nreentp, &nreentFunVar );
    ten_newCls( ten, &nreentFunVar, NULL, &nreentClsVar );
    ten_newFib( ten, &nreentClsVar, NULL, &nreentFibVar );
    
    ten_Tup args  = ten_pushA( ten, "" );
    ten_Tup rets1 = ten_cont( ten, &nreentFibVar, &args );
    ten_propError( ten, &nreentFibVar );
    if( ten_size( ten, &rets1 ) != 0 )
        ten_panic( ten, ten_int( ten, 3 ) );
    
    ten_Tup rets2 = ten_cont( ten, &nreentFibVar, &args );
    ten_propError( ten, &nreentFibVar );
    if( ten_size( ten, &rets2 ) != 0 )
        ten_panic( ten, ten_int( ten, 4 ) );
}

void
test_fiber( ten_State* ten ) {
    fiber_reentry1( ten );
    fiber_reentry2( ten );
}
