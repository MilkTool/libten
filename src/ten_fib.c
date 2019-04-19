#include "ten_fib.h"
#include "ten_sym.h"
#include "ten_ptr.h"
#include "ten_str.h"
#include "ten_rec.h"
#include "ten_dat.h"
#include "ten_upv.h"
#include "ten_gen.h"
#include "ten_env.h"
#include "ten_idx.h"
#include "ten_opcodes.h"
#include "ten_assert.h"
#include "ten_macros.h"
#include "ten_cls.h"
#include "ten_fun.h"
#include "ten_math.h"
#include <string.h>
#include <limits.h>

static void
ensureStack( State* state, Fiber* fib, uint n );

static void
contFirst( State* state, Fiber* fib, Tup* args );

static void
contNext( State* state, Fiber* fib, Tup* args );

static void
doCall( State* state, Fiber* fib );

static void
doLoop( State* state, Fiber* fib );

static void
pushAR( State* state, Fiber* fib, NatAR* nat );

static void
popAR( State* state, Fiber* fib );

static void
errUdfAsArg( State* state, Function* fun, uint arg );

static void
errTooFewArgs( State* state, Function* fun, uint argc );

static void
errTooManyArgs( State* state, Function* fun, uint argc );

void
fibInit( State* state ) {
    state->fibState = NULL;
}

#ifdef ten_TEST
typedef struct {
    Defer     defer;
    Scanner   scan;
    Gen*      gen;
    Function* fun;
    Closure*  cls;
    Fiber*    fib;
    TVal      val1;
    TVal      val2;
} FibTest;

static void
fibTestScan( State* state, Scanner* scan ) {
    FibTest* test = structFromScan( FibTest, scan );
    if( test->fun )
        stateMark( state, test->fun );
    if( test->cls )
        stateMark( state, test->cls );
    if( test->fib )
        stateMark( state, test->fib );
    tvMark( test->val1 );
    tvMark( test->val2 );
}

void
fibTestDefer( State* state, Defer* defer ) {
    FibTest* test = (FibTest*)defer;
    stateRemoveScanner( state, &test->scan );
    if( test->gen )
        genFree( state, test->gen );
}

void
fibTest( State* state ) {
    FibTest test = {
        .defer = { .cb = fibTestDefer },
        .scan  = { .cb = fibTestScan },
        .val1  = tvUdf(),
        .val2  = tvUdf()
    };
    stateInstallScanner( state, &test.scan );
    stateInstallDefer( state, &test.defer );
    
    {
        Gen* gen = genMake( state, NULL, NULL, false, true );
        test.gen = gen;
        
        GenVar*   v1 = genAddVar( state, gen, symGet( state, "v1", 2 ) );
        GenVar*   v2 = genAddVar( state, gen, symGet( state, "v2", 2 ) );
        GenVar*   v3 = genAddVar( state, gen, symGet( state, "v3", 2 ) );
        GenConst* c1 = genAddConst( state, gen, tvInt( 1 ) );
        GenConst* c2 = genAddConst( state, gen, tvInt( 2 ) );
        GenConst* c3 = genAddConst( state, gen, tvInt( 3 ) );
        
        genPutInstr( state, gen, inMake( OPC_GET_CONST, c1->which ) );
        // Stack: 1
        
        genPutInstr( state, gen, inMake( OPC_NOT, 0 ) );
        // Stack: -2
        
        genPutInstr( state, gen, inMake( OPC_GET_CONST, c2->which ) );
        // Stack 2 -2
        
        genPutInstr( state, gen, inMake( OPC_NEG, 0 ) );
        // Stack: -2 -2
        
        genPutInstr( state, gen, inMake( OPC_GET_CONST, c3->which ) );
        // Stack: 3 -2 -2
        
        genPutInstr( state, gen, inMake( OPC_FIX, 0 ) );
        // Stack: 3 -2 -2
        
        genPutInstr( state, gen, inMake( OPC_MUL, 0 ) );
        // Stack: -6 -2
        
        genPutInstr( state, gen, inMake( OPC_DIV, 0 ) );
        // Stack: 0
        
        genPutInstr( state, gen, inMake( OPC_GET_CONST, c2->which ) );
        // Stack: 2 0
        
        genPutInstr( state, gen, inMake( OPC_MOD, 0 ) );
        // Stack: 0

        genPutInstr( state, gen, inMake( OPC_GET_CONST, c1->which ) );
        // Stack: 1 0
        
        genPutInstr( state, gen, inMake( OPC_ADD, 0 ) );
        // Stack: 1

        genPutInstr( state, gen, inMake( OPC_GET_CONST, c2->which ) );
        // Stack: 2 1
        
        genPutInstr( state, gen, inMake( OPC_SUB, 0 ) );
        // Stack: -1
        
        genPutInstr( state, gen, inMake( OPC_GET_CONST, c3->which ) );
        // Stack: 3 -1
        
        genPutInstr( state, gen, inMake( OPC_LSL, 0 ) );
        // Stack: -8
        
        genPutInstr( state, gen, inMake( OPC_GET_CONST, c1->which ) );
        // Stack: 1 -8
        
        genPutInstr( state, gen, inMake( OPC_LSR, 0 ) );
        // Stack: 2147483644
        
        genPutInstr( state, gen, inMake( OPC_RETURN, 0 ) );
        
        Function* fun = genFinish( state, gen, false );
        test.fun = fun;
        test.gen = NULL;
        
        Closure* cls = clsNewVir( state, fun, NULL );
        test.cls = cls;
        
        Fiber* fib = fibNew( state, cls, NULL );
        test.fib = fib;
        
        Tup args = statePush( state, 0 );
        Tup rets = fibCont( state, fib, &args );
        tenAssert( tvGetInt( tupAt( rets, 0 ) ) == 2147483644 );
    }
    {
        Gen* gen = genMake( state, NULL, NULL, false, true );
        test.gen = gen;
        
        test.val1 = tvObj( idxNew( state ) );
        test.val2 = tvObj( idxNew( state ) );
        
        TVal k1 = tvSym( symGet( state, "k1", 2 ) );
        TVal k2 = tvSym( symGet( state, "k2", 2 ) );
        TVal k3 = tvSym( symGet( state, "k3", 2 ) );
        TVal k4 = tvSym( symGet( state, "k4", 2 ) );
        TVal v1 = tvInt( 1 );
        TVal v2 = tvInt( 2 );
        TVal v3 = tvInt( 3 );
        TVal v4 = tvInt( 4 );
        
        GenConst* ci1 = genAddConst( state, gen, test.val1 );
        GenConst* ci2 = genAddConst( state, gen, test.val2 );
        GenConst* ck1 = genAddConst( state, gen, k1 );
        GenConst* ck2 = genAddConst( state, gen, k2 );
        GenConst* ck3 = genAddConst( state, gen, k3 );
        GenConst* ck4 = genAddConst( state, gen, k4 );
        GenConst* cv1 = genAddConst( state, gen, v1 );
        GenConst* cv2 = genAddConst( state, gen, v2 );
        GenConst* cv3 = genAddConst( state, gen, v3 );
        GenConst* cv4 = genAddConst( state, gen, v4 );
        
        genPutInstr( state, gen, inMake( OPC_GET_CONST, ci1->which ) );
        genPutInstr( state, gen, inMake( OPC_GET_CONST, ck1->which ) );
        genPutInstr( state, gen, inMake( OPC_GET_CONST, cv1->which ) );
        genPutInstr( state, gen, inMake( OPC_GET_CONST, ck2->which ) );
        genPutInstr( state, gen, inMake( OPC_GET_CONST, cv2->which ) );
            genPutInstr( state, gen, inMake( OPC_GET_CONST, ci2->which ) );
            genPutInstr( state, gen, inMake( OPC_GET_CONST, ck3->which ) );
            genPutInstr( state, gen, inMake( OPC_GET_CONST, cv3->which ) );
            genPutInstr( state, gen, inMake( OPC_GET_CONST, ck4->which ) );
            genPutInstr( state, gen, inMake( OPC_GET_CONST, cv4->which ) );
            genPutInstr( state, gen, inMake( OPC_MAKE_REC, 2 ) );
        genPutInstr( state, gen, inMake( OPC_MAKE_VREC, 2 ) );
        
        genPutInstr( state, gen, inMake( OPC_RETURN, 0 ) );
        
        Function* fun = genFinish( state, gen, false );
        test.fun = fun;
        test.gen = NULL;
        
        Closure* cls = clsNewVir( state, fun, NULL );
        test.cls = cls;
        
        Fiber* fib = fibNew( state, cls, NULL );
        test.fib = fib;
        
        Tup args = statePush( state, 0 );
        Tup rets = fibCont( state, fib, &args );
        
        TVal ret = tupAt( rets, 0 );
        tenAssert( tvIsObj( ret ) && datGetTag( tvGetObj( ret ) ) == OBJ_REC );
        Record* rec = tvGetObj( ret );
        tenAssert( tvEqual( recGet( state, rec, k1 ), v1 ) );
        tenAssert( tvEqual( recGet( state, rec, k2 ), v2 ) );
        tenAssert( tvEqual( recGet( state, rec, k3 ), v3 ) );
        tenAssert( tvEqual( recGet( state, rec, k4 ), v4 ) );
    }
    {
        Gen* gen = genMake( state, NULL, NULL, false, true );
        test.gen = gen;
        
        test.val1 = tvObj( idxNew( state ) );
        
        GenConst* ci = genAddConst( state, gen, test.val1 );
        
        genPutInstr( state, gen, inMake( OPC_LOAD_INT, 1 ) );
        genPutInstr( state, gen, inMake( OPC_LOAD_INT, 2 ) );
        genPutInstr( state, gen, inMake( OPC_LOAD_INT, 3 ) );
            genPutInstr( state, gen, inMake( OPC_GET_CONST, ci->which ) );
            genPutInstr( state, gen, inMake( OPC_LOAD_INT, 0 ) );
            genPutInstr( state, gen, inMake( OPC_LOAD_INT, 4 ) );
            genPutInstr( state, gen, inMake( OPC_LOAD_INT, 1 ) );
            genPutInstr( state, gen, inMake( OPC_LOAD_INT, 5 ) );
            genPutInstr( state, gen, inMake( OPC_LOAD_INT, 2 ) );
            genPutInstr( state, gen, inMake( OPC_LOAD_INT, 6 ) );
            genPutInstr( state, gen, inMake( OPC_MAKE_REC, 3 ) );
        genPutInstr( state, gen, inMake( OPC_MAKE_VTUP, 3 ) );
        
        genPutInstr( state, gen, inMake( OPC_RETURN, 0 ) );
        
        Function* fun = genFinish( state, gen, false );
        test.fun = fun;
        test.gen = NULL;
        
        Closure* cls = clsNewVir( state, fun, NULL );
        test.cls = cls;
        
        Fiber* fib = fibNew( state, cls, NULL );
        test.fib = fib;
        
        Tup args = statePush( state, 0 );
        Tup rets = fibCont( state, fib, &args );
        tenAssert( rets.size == 6 );
        tenAssert( tvEqual( tupAt( rets, 0 ), tvInt( 1 ) ) );
        tenAssert( tvEqual( tupAt( rets, 1 ), tvInt( 2 ) ) );
        tenAssert( tvEqual( tupAt( rets, 2 ), tvInt( 3 ) ) );
        tenAssert( tvEqual( tupAt( rets, 3 ), tvInt( 4 ) ) );
        tenAssert( tvEqual( tupAt( rets, 4 ), tvInt( 5 ) ) );
        tenAssert( tvEqual( tupAt( rets, 5 ), tvInt( 6 ) ) );
    }
    {
        Gen* gen = genMake( state, NULL, NULL, false, true );
        test.gen = gen;
        
        GenVar* v1 = genAddVar( state, gen, symGet( state, "v1", 2 ) );
        tenAssert( v1->type == VAR_LOCAL );
        
        genPutInstr( state, gen, inMake( OPC_REF_LOCAL, v1->which ) );
        genPutInstr( state, gen, inMake( OPC_LOAD_INT, 123 ) );
        genPutInstr( state, gen, inMake( OPC_DEF_ONE, 0 ) );
        genPutInstr( state, gen, inMake( OPC_POP, 0 ) );
        genPutInstr( state, gen, inMake( OPC_GET_LOCAL, v1->which ) );
        
        genPutInstr( state, gen, inMake( OPC_RETURN, 0 ) );
        
        Function* fun = genFinish( state, gen, false );
        test.fun = fun;
        test.gen = NULL;
        
        Closure* cls = clsNewVir( state, fun, NULL );
        test.cls = cls;
        
        Fiber* fib = fibNew( state, cls, NULL );
        test.fib = fib;
        
        Tup args = statePush( state, 0 );
        Tup rets = fibCont( state, fib, &args );
        tenAssert( rets.size == 1 );
        tenAssert( tvEqual( tupAt( rets, 0 ), tvInt( 123 ) ) );
    }
    {
        Gen* gen = genMake( state, NULL, NULL, false, true );
        test.gen = gen;
        
        test.val1 = tvObj( idxNew( state ) );
        
        GenConst* ci = genAddConst( state, gen, test.val1 );
        GenVar* v1 = genAddVar( state, gen, symGet( state, "v1", 2 ) );
        tenAssert( v1->type == VAR_LOCAL );
        
        genPutInstr( state, gen, inMake( OPC_REF_LOCAL, v1->which ) );
        genPutInstr( state, gen, inMake( OPC_GET_CONST, ci->which ) );
            genPutInstr( state, gen, inMake( OPC_LOAD_INT, 1 ) );
            genPutInstr( state, gen, inMake( OPC_LOAD_INT, 2 ) );
            genPutInstr( state, gen, inMake( OPC_MAKE_TUP, 2 ) );
        genPutInstr( state, gen, inMake( OPC_DEF_VTUP, 0 ) );
        genPutInstr( state, gen, inMake( OPC_POP, 0 ) );
        genPutInstr( state, gen, inMake( OPC_GET_LOCAL, v1->which ) );
        
        genPutInstr( state, gen, inMake( OPC_RETURN, 0 ) );
        
        Function* fun = genFinish( state, gen, false );
        test.fun = fun;
        test.gen = NULL;
        
        Closure* cls = clsNewVir( state, fun, NULL );
        test.cls = cls;
        
        Fiber* fib = fibNew( state, cls, NULL );
        test.fib = fib;
        
        Tup args = statePush( state, 0 );
        Tup rets = fibCont( state, fib, &args );
        TVal ret = tupAt( rets, 0 );
        tenAssert( tvIsObj( ret ) && datGetTag( tvGetObj( ret ) ) == OBJ_REC );
        Record* rec = tvGetObj( ret );
        tenAssert( tvEqual( recGet( state, rec, tvInt( 0 ) ), tvInt( 1 ) ) );
        tenAssert( tvEqual( recGet( state, rec, tvInt( 1 ) ), tvInt( 2 ) ) );
    }
    {
        Gen* gen = genMake( state, NULL, NULL, false, true );
        test.gen = gen;
        
        test.val1 = tvObj( idxNew( state ) );
        
        GenConst* ci = genAddConst( state, gen, test.val1 );
        GenVar* v1 = genAddVar( state, gen, symGet( state, "v1", 2 ) );
        tenAssert( v1->type == VAR_LOCAL );
        
        genPutInstr( state, gen, inMake( OPC_REF_LOCAL, v1->which ) );
        genPutInstr( state, gen, inMake( OPC_GET_CONST, ci->which ) );
            genPutInstr( state, gen, inMake( OPC_GET_CONST, ci->which ) );
            genPutInstr( state, gen, inMake( OPC_LOAD_INT, 0 ) );
            genPutInstr( state, gen, inMake( OPC_LOAD_INT, 1 ) );
            genPutInstr( state, gen, inMake( OPC_LOAD_INT, 1 ) );
            genPutInstr( state, gen, inMake( OPC_LOAD_INT, 2 ) );
            genPutInstr( state, gen, inMake( OPC_MAKE_REC, 2 ) );
        genPutInstr( state, gen, inMake( OPC_DEF_VREC, 0 ) );
        genPutInstr( state, gen, inMake( OPC_POP, 0 ) );
        genPutInstr( state, gen, inMake( OPC_GET_LOCAL, v1->which ) );
        
        genPutInstr( state, gen, inMake( OPC_RETURN, 0 ) );
        
        Function* fun = genFinish( state, gen, false );
        test.fun = fun;
        test.gen = NULL;
        
        Closure* cls = clsNewVir( state, fun, NULL );
        test.cls = cls;
        
        Fiber* fib = fibNew( state, cls, NULL );
        
        Tup args = statePush( state, 0 );
        Tup rets = fibCont( state, fib, &args );
        TVal ret = tupAt( rets, 0 );
        tenAssert( tvIsObj( ret ) && datGetTag( tvGetObj( ret ) ) == OBJ_REC );
        Record* rec = tvGetObj( ret );
        tenAssert( tvEqual( recGet( state, rec, tvInt( 0 ) ), tvInt( 1 ) ) );
        tenAssert( tvEqual( recGet( state, rec, tvInt( 1 ) ), tvInt( 2 ) ) );
    }
    {
        Gen* gen = genMake( state, NULL, NULL, false, true );
        test.gen = gen;
        
        Gen* sub = genMake( state, gen, NULL, false, true );
        GenVar* p1 = genAddParam( state, sub, symGet( state, "p1", 2 ), false );
        genPutInstr( state, sub, inMake( OPC_GET_LOCAL, p1->which ) );
        genPutInstr( state, sub, inMake( OPC_RETURN, 0 ) );
        genFinish( state, sub, true );
        
        
        genPutInstr( state, gen, inMake( OPC_LOAD_INT, 123 ) );
        genPutInstr( state, gen, inMake( OPC_CALL, 0 ) );
        genPutInstr( state, gen, inMake( OPC_RETURN, 0 ) );
        
        Function* fun = genFinish( state, gen, false );
        test.fun = fun;
        test.gen = NULL;
        
        Closure* cls = clsNewVir( state, fun, NULL );
        test.cls = cls;
        
        Fiber* fib = fibNew( state, cls, NULL );
        test.fib = fib;
        
        Tup args = statePush( state, 0 );
        Tup rets = fibCont( state, fib, &args );
        tenAssert( rets.size == 1 );
        tenAssert( tvEqual( tupAt( rets, 0 ), tvInt( 123 ) ) );
    }
    
    stateCommitDefer( state, &test.defer );
}
#endif

static void
onError( State* state, Defer* defer );


Fiber*
fibNew( State* state, Closure* cls, SymT* tag ) {
    Part fibP;
    Fiber* fib = stateAllocObj( state, &fibP, sizeof(Fiber), OBJ_FIB );
    
    uint arCap = 7;
    Part arP;
    VirAR* ar = stateAllocRaw( state, &arP, sizeof(VirAR)*arCap );
    
    uint tmpCap = 16;
    Part tmpsP;
    TVal* tmps = stateAllocRaw( state, &tmpsP, sizeof(TVal)*tmpCap );
    
    fib->state         = ten_FIB_STOPPED;
    fib->nats          = NULL;
    fib->arStack.ars   = ar;
    fib->arStack.cap   = arCap;
    fib->arStack.top   = 0;
    fib->tmpStack.tmps = tmps;
    fib->tmpStack.cap  = tmpCap;
    fib->rPtr          = &fib->rBuf;
    fib->entry         = cls;
    fib->parent        = NULL;
    fib->errNum        = ten_ERR_NONE;
    fib->errVal        = tvUdf();
    fib->errStr        = NULL;
    fib->trace         = NULL;
    fib->errDefer.cb   = onError;
    fib->yieldJmp      = NULL;
    
    if( tag ) {
        fib->tag    = *tag;
        fib->tagged = true;
    }
    else {
        fib->tagged = false;
    }
    
    memset( &fib->rBuf, 0, sizeof(Regs) );
    fib->rPtr->sp = fib->tmpStack.tmps;
    
    stateCommitRaw( state, &arP );
    stateCommitRaw( state, &tmpsP );
    stateCommitObj( state, &fibP );
    
    return fib;
}

Tup
fibPush( State* state, Fiber* fib, uint n ) {
    ensureStack( state, fib, n + 1 );
    
    Tup tup = {
        .base   = &fib->tmpStack.tmps,
        .offset = fib->rPtr->sp - fib->tmpStack.tmps,
        .size   = n
    };
    for( uint i = 0 ; i < n ; i++ )
        *(fib->rPtr->sp++) = tvUdf();
    if( n != 1 )
        *(fib->rPtr->sp++) = tvTup( n );
    
    return tup;
}

Tup
fibTop( State* state, Fiber* fib ) {
    tenAssert( fib->rPtr->sp > fib->tmpStack.tmps );
    
    uint loc  = fib->rPtr->sp - fib->tmpStack.tmps - 1;
    uint size = 1;
    if( tvIsTup( fib->tmpStack.tmps[loc] ) ) {
        size = tvGetTup( fib->tmpStack.tmps[loc] );
        
        tenAssert( loc >= size );
        loc -= size;
    }
    
    return (Tup){
        .base   = &fib->tmpStack.tmps,
        .offset = loc,
        .size   = size
    };
}

void
fibPop( State* state, Fiber* fib ) {
    tenAssert( fib->rPtr->sp > fib->tmpStack.tmps );
    
    if( tvIsTup( fib->rPtr->sp[-1] ) ) {
        uint size = tvGetTup( fib->rPtr->sp[-1] );
        
        tenAssert( fib->rPtr->sp - size >= fib->tmpStack.tmps );
        fib->rPtr->sp -= size;
    }
    tenAssert( fib->rPtr->sp > fib->tmpStack.tmps );
    fib->rPtr->sp--;
}



Tup
fibCont( State* state, Fiber* fib, Tup* args ) {
    if( fib->state == ten_FIB_RUNNING )
        stateErrFmtA( state, ten_ERR_FIBER, "Continued running fiber" );
    if( fib->state == ten_FIB_WAITING )
        stateErrFmtA( state, ten_ERR_FIBER, "Continued waiting fiber" );
    if( fib->state == ten_FIB_FINISHED )
        stateErrFmtA( state, ten_ERR_FIBER, "Continued finished fiber" );
    if( fib->state == ten_FIB_FAILED )
        stateErrFmtA( state, ten_ERR_FIBER, "Continued failed fiber" );
    
    
    // Put the parent fiber (the current one at this point)
    // into a waiting state.  Remove its errDefer to prevent
    // errors from the continuation from propegating.
    Fiber* parent = state->fiber;
    if( parent ) {
        parent->state = ten_FIB_WAITING;
        stateCancelDefer( state, &parent->errDefer );
    }
    
    // Set the fiber that's being continued to the running
    // state and install its errDefer to catch errors.
    fib->state   = ten_FIB_RUNNING;
    fib->parent  = parent;
    state->fiber = fib;
    stateInstallDefer( state, &fib->errDefer );
    
    // Install our own error handler to localize non-critical
    // errors to the fiber.
    jmp_buf  errJmp;
    jmp_buf* oldJmp = stateSwapErrJmp( state, &errJmp );
    int eSig = setjmp( errJmp );
    if( eSig ) {
        
        // When an error actually occurs replace the original
        // handler, so any further errors go to the right place.
        stateSwapErrJmp( state, oldJmp );
        
        // Restore the parent fiber to the running state.
        state->fiber = parent;
        if( parent ) {
            parent->state = ten_FIB_RUNNING;
            stateInstallDefer( state, &parent->errDefer );
            fib->parent = NULL;
        }
        
        // Critical errors are re-thrown, these will be caught
        // by each parent fiber, allowing them to cleanup, but
        // will ultimately propegate back to the user.
        if( fib->errNum == ten_ERR_FATAL )
            stateErrProp( state );
        
        return (Tup){ .base = &fib->tmpStack.tmps, .offset = 0, .size = 0 };
    }
    
    
    // This is where we jump to yield from the fiber, it'll
    // ultimately be the last code run in this function if
    // the continuation doesn't fail, since we jump back here
    // at the end.
    jmp_buf yieldJmp;
    fib->yieldJmp = &yieldJmp;
    int ySig = setjmp( yieldJmp );
    if( ySig ) {
        
        // Restore old error jump.
        stateSwapErrJmp( state, oldJmp );
        
        // Restore the calling fiber.
        state->fiber = parent;
        if( parent ) {
            parent->state = ten_FIB_RUNNING;
            stateInstallDefer( state, &parent->errDefer );
            fib->parent = NULL;
        }
        
        // Cancel the fiber's error handling defer.
        stateCancelDefer( state, &fib->errDefer );
        
        // The top tuple on the stack contains the yielded
        // values, so that's what we return.
        return fibTop( state, fib );
    }
    
    
    // We have two cases for continuation, the first continuation
    // is treated differently from subsequent ones as it needs to
    // initialize it with a call to the entry closure.  Subsequent
    // continuations also expect the top of the stack to contain
    // the last yielded value, which isn't the case for the first
    // continuation.  We use the entry closure's pointer itself
    // to tell which to do, after the first continutation it'll be
    // set to NULL.
    if( fib->entry )
        contFirst( state, fib, args );
    else
        contNext( state, fib, args );
    
    // We'll only reach this point once the fiber has finished,
    // its entry closure returned.  But this is treated just
    // like another yield with the addition of setting the fiber's
    // state to FINISHED.  So we just jump back to the yield
    // handler.
    fib->state = ten_FIB_FINISHED;
    longjmp( *fib->yieldJmp, 1 );
}

void
fibYield( State* state, Tup* vals ) {
    Fiber* fib = state->fiber;
    tenAssert( fib );
    
    
    TVal* dstv = fib->rPtr->lcl;
    while( fib->rPtr->ip == 0 && fib->arStack.top > 0 ) {
        dstv = fib->rPtr->lcl;
        popAR( state, fib );
    }
    
    if( fib->rPtr->ip == 0 && fib->arStack.top == 0 ) {
        fib->state = ten_FIB_FINISHED;
    }
    else {
        fib->state = ten_FIB_STOPPED;
    }
    
    // Copy yielded values to expected location.
    uint valc = vals->size;
    ensureStack( state, fib, valc + 1 );
    
    TVal* valv = *vals->base + vals->offset;
    for( uint i = 0 ; i < valc ; i++ )
        dstv[i] = valv[i];
    fib->rPtr->sp = dstv + valc;
    
    if( valc != 1 )
        *(fib->rPtr->sp++) = tvTup( valc );
    

    // Save register set to buffer.
    fib->rBuf  = *fib->rPtr;
    fib->rPtr  = &fib->rBuf;
    
    longjmp( *fib->yieldJmp, 1 );
}

Tup
fibCall_( State* state, Closure* cls, Tup* args, char const* file, uint line ) {
    Fiber* fib = state->fiber;
    tenAssert( fib );
    
    Tup cit = fibPush( state, fib, 1 );
    tupAt( cit, 0 ) = tvObj( cls );
    
    Tup dup = fibPush( state, fib, args->size );
    for( uint i = 0 ; i < args->size ; i++ )
        tupAt( dup, i ) = tupAt( *args, i );
    
    NatAR nat = { .file = file, .line = line };
    pushAR( state, fib, &nat );
    doCall( state, fib );
    popAR( state, fib );
    
    return fibTop( state, fib );
}

void
fibClearError( State* state, Fiber* fib ) {
    if( fib->errNum == ten_ERR_NONE )
        return;
    
    fib->errNum = ten_ERR_NONE;
    fib->errStr = NULL;
    fib->errVal = tvUdf();
    stateFreeTrace( state, fib->trace );
}

void
fibPropError( State* state, Fiber* fib ) {
    if( fib->errNum == ten_ERR_NONE )
        return;
    
    state->errNum = fib->errNum;
    state->errStr = fib->errStr;
    state->errVal = fib->errVal;
    state->trace  = fib->trace;
    fib->trace = NULL;
    
    stateErrProp( state );
}


void
fibTraverse( State* state, Fiber* fib ) {
    NatAR* nIt = fib->nats;
    while( nIt ) {
        stateMark( state, nIt->ar.cls );
        nIt = nIt->prev;
    }
    
    for( uint i = 0 ; i < fib->arStack.top ; i++ ) {
        stateMark( state, fib->arStack.ars[i].ar.cls );
        
        NatAR* nIt = fib->arStack.ars[i].nats;
        while( nIt ) {
            stateMark( state, nIt->ar.cls );
            nIt = nIt->prev;
        }
    }
    
    for( TVal* v = fib->tmpStack.tmps ; v < fib->rPtr->sp ; v++ )
        tvMark( *v );
    
    if( fib->entry )
        stateMark( state, fib->entry );
    if( fib->parent )
        stateMark( state, fib->parent );    
    
    tvMark( fib->errVal );
    if( state->gcFull && fib->tagged )
        symMark( state, fib->tag );
}

void
fibDestruct( State* state, Fiber* fib ) {
    stateFreeRaw( state, fib->arStack.ars, fib->arStack.cap*sizeof(VirAR) );
    stateFreeRaw( state, fib->tmpStack.tmps, fib->tmpStack.cap*sizeof(TVal) );
    fib->arStack.cap   = 0;
    fib->arStack.ars   = NULL;
    fib->tmpStack.cap  = 0;
    fib->tmpStack.tmps = NULL;
    
    if( fib->trace )
        stateFreeTrace( state, fib->trace );
}

static void
contFirst( State* state, Fiber* fib, Tup* args ) {
    tenAssert( fib->entry != NULL );
    
    // On the first continuation we need to push
    // the entry closure and arguments onto the
    // stack before invoking a call to kickstart
    // the fiber.
    Tup cls = fibPush( state, fib, 1 );
    tupAt( cls, 0 ) = tvObj( fib->entry );
    fib->entry = NULL;
    
    Tup args2 = fibPush( state, fib, args->size );
    for( uint i = 0 ; i < args->size ; i++ )
        tupAt( args2, i ) = tupAt( *args, i );
    
    // That's all for initialization, the call routine
    // will take care of the rest.
    doCall( state, fib  );
}

static void
contNext( State* state, Fiber* fib, Tup* args ) {
    tenAssert( fib->entry == NULL );
    
    // The previous continuation will have left its
    // return/yield values on the stack, so pop those.
    fibPop( state, fib );
    
    // And the fiber will expect continuation arguments,
    // to replace the yield returns, so we push those.
    Tup args2 = fibPush( state, fib, args->size );
    for( uint i = 0 ; i < args->size ; i++ )
        tupAt( args2, i ) = tupAt( *args, i );
    
    // From here we directly enter the interpret loop.
    doLoop( state, fib );
}

static void
doCall( State* state, Fiber* fib ) {
    tenAssert( fib->state == ten_FIB_RUNNING );
    tenAssert( fib->rPtr->sp > fib->tmpStack.tmps + 1 );
    
    Regs* regs = fib->rPtr;
    
    // Figure out how many arguments were passed,
    // and where they start.
    TVal* args = &regs->sp[-1];
    uint  argc = 1;
    TVal* argv = args - 1;
    if( tvIsTup( *args ) ) {
        argc = tvGetTup( *args );
        argv -= argc;
        tenAssert( argc < args - fib->tmpStack.tmps );
        
        // Pop the tuple header, it's no longer needed.
        regs->sp--;
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
        regs->sp = extra + 1;
    }
    // Otherwise the parameter count must be matched by the arguments.
    else {
        if( argc > parc )
            errTooManyArgs( state, cls->fun, argc );
    }
    
    regs->lcl = argv;
    regs->cls = cls;
    if( cls->fun->type == FUN_VIR ) {
        VirFun* fun = & cls->fun->u.vir;
        ensureStack( state, fib, fun->nLocals + fun->nTemps );
        
        TVal* locals = regs->lcl + argc + 1;
        for( uint i = 0 ; i < fun->nLocals ; i++ )
            locals[i] = tvUdf();
        
        regs->sp += fun->nLocals;
        regs->ip = cls->fun->u.vir.code;
        doLoop( state, fib );
        
        uint  retc = 1;
        TVal* retv = regs->sp - 1;
        if( tvIsTup( *retv ) ) {
            retc += tvGetTup( *retv );
            retv -= retc - 1;
        }
        TVal* dstv = regs->lcl;
        for( uint i = 0 ; i < retc ; i++ )
            dstv[i] = retv[i];
        regs->sp = dstv + retc;
    }
    else {
        
        regs->ip = NULL;
        
        // Initialize an argument tuple for the callback.
        Tup aTup = {
            .base   = &fib->tmpStack.tmps,
            .offset = regs->lcl - fib->tmpStack.tmps + 1,
            .size   = argc
        };
        
        // If a Data object is attached to the closure
        // then we need to initialize a tuple for its
        // members as well, otherwise pass NULL.
        ten_Tup t;
        if( cls->dat.dat != NULL ) {
            Data* dat = cls->dat.dat;
            Tup mTup = {
                .base   = &dat->mems,
                .offset = 0,
                .size   = dat->info->nMems
            };
            t = cls->fun->u.nat.cb( (ten_State*)state, (ten_Tup*)&aTup, (ten_Tup*)&mTup, dat->data );
        }
        else {
            t = cls->fun->u.nat.cb( (ten_State*)state, (ten_Tup*)&aTup, NULL, NULL );
        }
        
        Tup*  rets = (Tup*)&t;
        uint  retc = rets->size;
        ensureStack( state, fib, retc + 1 );
        
        TVal* retv = *rets->base + rets->offset;
        TVal* dstv = regs->lcl;
        for( uint i = 0 ; i < retc ; i++ )
            dstv[i] = retv[i];
        regs->sp = dstv + retc;
        
        if( retc != 1 )
            *(regs->sp++) = tvTup( retc );
    }
}

static void
doLoop( State* state, Fiber* fib ) {
    // Copy the current set of registers to a local struct
    // for faster access.  These will be copied back to the
    // original struct before returning.
    Regs* rPtr = fib->rPtr;
    Regs  regs = *fib->rPtr;
    fib->rPtr = &regs;
    
    #ifdef ten_NO_COMPUTED_GOTOS
        #define LOOP                        \
            loop: {                         \
                instr in = (*regs.ip++);    \
                switch( inGetOpc( in ) ) {  \
        
        #define CASE( N )                   \
            case OPC_ ## N: {
        
        #define BREAK                       \
            }                               \
            goto loop;
        
        #define EXIT                        \
            do {                            \
                goto end;                   \
            } while( 0 )
        #define NEXT                        \
            do {                            \
                goto loop;                  \
            } while( 0 )
        #define END                         \
            } end:
    #else
        #define OP( N, SE ) &&do_ ## N,
        static void* ops[] = {
            #include "inc/ops.inc"
        };
        #undef OP
        
        #define LOOP                        \
            {                               \
                instr in = (*regs.ip++);    \
                goto *ops[ inGetOpc( in ) ];
        
        #define CASE( N )                   \
            do_ ## N: {
        
        #define BREAK                       \
            }                               \
            in = (*regs.ip++);              \
            goto *ops[ inGetOpc( in ) ];
        
        #define EXIT                        \
            do {                            \
                goto end;                   \
            } while( 0 )
        #define NEXT                        \
            do {                            \
                in = (*regs.ip++);          \
                goto *ops[ inGetOpc( in ) ];\
            } while( 0 )
        #define END                         \
            } end:
    #endif
    
    LOOP
        CASE(DEF_ONE)
            #include "inc/ops/DEF_ONE.inc"
        BREAK;
        CASE(DEF_TUP)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/DEF_TUP.inc"
        BREAK;
        CASE(DEF_VTUP)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/DEF_VTUP.inc"
        BREAK;
        CASE(DEF_REC)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/DEF_REC.inc"
        BREAK;
        CASE(DEF_VREC)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/DEF_VREC.inc"
        BREAK;
        CASE(DEF_SIG)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/DEF_SIG.inc"
        BREAK;
        CASE(DEF_VSIG)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/DEF_VSIG.inc"
        BREAK;
        CASE(SET_ONE)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/SET_ONE.inc"
        BREAK;
        CASE(SET_TUP)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/SET_TUP.inc"
        BREAK;
        CASE(SET_VTUP)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/SET_VTUP.inc"
        BREAK;
        CASE(SET_REC)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/SET_REC.inc"
        BREAK;
        CASE(SET_VREC)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/SET_VREC.inc"
        BREAK;
        CASE(REC_DEF_ONE)
            #include "inc/ops/REC_DEF_ONE.inc"
        BREAK;
        CASE(REC_DEF_TUP)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/REC_DEF_TUP.inc"
        BREAK;
        CASE(REC_DEF_VTUP)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/REC_DEF_VTUP.inc"
        BREAK;
        CASE(REC_DEF_REC)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/REC_DEF_REC.inc"
        BREAK;
        CASE(REC_DEF_VREC)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/REC_DEF_VREC.inc"
        BREAK;
        CASE(REC_SET_ONE)
            #include "inc/ops/REC_SET_ONE.inc"
        BREAK;
        CASE(REC_SET_TUP)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/REC_SET_TUP.inc"
        BREAK;
        CASE(REC_SET_VTUP)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/REC_SET_VTUP.inc"
        BREAK;
        CASE(REC_SET_REC)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/REC_SET_REC.inc"
        BREAK;
        CASE(REC_SET_VREC)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/REC_SET_VREC.inc"
        BREAK;
        CASE(GET_CONST)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/GET_CONST.inc"
        BREAK;
        CASE(GET_CONST0)
            ushort const opr = 0;
            #include "inc/ops/GET_CONST.inc"
        BREAK;
        CASE(GET_CONST1)
            ushort const opr = 1;
            #include "inc/ops/GET_CONST.inc"
        BREAK;
        CASE(GET_CONST2)
            ushort const opr = 2;
            #include "inc/ops/GET_CONST.inc"
        BREAK;
        CASE(GET_CONST3)
            ushort const opr = 3;
            #include "inc/ops/GET_CONST.inc"
        BREAK;
        CASE(GET_CONST4)
            ushort const opr = 4;
            #include "inc/ops/GET_CONST.inc"
        BREAK;
        CASE(GET_CONST5)
            ushort const opr = 5;
            #include "inc/ops/GET_CONST.inc"
        BREAK;
        CASE(GET_CONST6)
            ushort const opr = 6;
            #include "inc/ops/GET_CONST.inc"
        BREAK;
        CASE(GET_CONST7)
            ushort const opr = 7;
            #include "inc/ops/GET_CONST.inc"
        BREAK;
        CASE(GET_UPVAL)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/GET_UPVAL.inc"
        BREAK;
        CASE(GET_UPVAL0)
            ushort const opr = 0;
            #include "inc/ops/GET_UPVAL.inc"
        BREAK;
        CASE(GET_UPVAL1)
            ushort const opr = 1;
            #include "inc/ops/GET_UPVAL.inc"
        BREAK;
        CASE(GET_UPVAL2)
            ushort const opr = 2;
            #include "inc/ops/GET_UPVAL.inc"
        BREAK;
        CASE(GET_UPVAL3)
            ushort const opr = 3;
            #include "inc/ops/GET_UPVAL.inc"
        BREAK;
        CASE(GET_UPVAL4)
            ushort const opr = 4;
            #include "inc/ops/GET_UPVAL.inc"
        BREAK;
        CASE(GET_UPVAL5)
            ushort const opr = 5;
            #include "inc/ops/GET_UPVAL.inc"
        BREAK;
        CASE(GET_UPVAL6)
            ushort const opr = 6;
            #include "inc/ops/GET_UPVAL.inc"
        BREAK;
        CASE(GET_UPVAL7)
            ushort const opr = 7;
            #include "inc/ops/GET_UPVAL.inc"
        BREAK;
        CASE(GET_LOCAL)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/GET_LOCAL.inc"
        BREAK;
        CASE(GET_LOCAL0)
            ushort const opr = 0;
            #include "inc/ops/GET_LOCAL.inc"
        BREAK;
        CASE(GET_LOCAL1)
            ushort const opr = 1;
            #include "inc/ops/GET_LOCAL.inc"
        BREAK;
        CASE(GET_LOCAL2)
            ushort const opr = 2;
            #include "inc/ops/GET_LOCAL.inc"
        BREAK;
        CASE(GET_LOCAL3)
            ushort const opr = 3;
            #include "inc/ops/GET_LOCAL.inc"
        BREAK;
        CASE(GET_LOCAL4)
            ushort const opr = 4;
            #include "inc/ops/GET_LOCAL.inc"
        BREAK;
        CASE(GET_LOCAL5)
            ushort const opr = 5;
            #include "inc/ops/GET_LOCAL.inc"
        BREAK;
        CASE(GET_LOCAL6)
            ushort const opr = 6;
            #include "inc/ops/GET_LOCAL.inc"
        BREAK;
        CASE(GET_LOCAL7)
            ushort const opr = 7;
            #include "inc/ops/GET_LOCAL.inc"
        BREAK;
        CASE(GET_CLOSED)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/GET_CLOSED.inc"
        BREAK;
        CASE(GET_CLOSED0)
            ushort const opr = 0;
            #include "inc/ops/GET_CLOSED.inc"
        BREAK;
        CASE(GET_CLOSED1)
            ushort const opr = 1;
            #include "inc/ops/GET_CLOSED.inc"
        BREAK;
        CASE(GET_CLOSED2)
            ushort const opr = 2;
            #include "inc/ops/GET_CLOSED.inc"
        BREAK;
        CASE(GET_CLOSED3)
            ushort const opr = 3;
            #include "inc/ops/GET_CLOSED.inc"
        BREAK;
        CASE(GET_CLOSED4)
            ushort const opr = 4;
            #include "inc/ops/GET_CLOSED.inc"
        BREAK;
        CASE(GET_CLOSED5)
            ushort const opr = 5;
            #include "inc/ops/GET_CLOSED.inc"
        BREAK;
        CASE(GET_CLOSED6)
            ushort const opr = 6;
            #include "inc/ops/GET_CLOSED.inc"
        BREAK;
        CASE(GET_CLOSED7)
            ushort const opr = 7;
            #include "inc/ops/GET_CLOSED.inc"
        BREAK;
        CASE(GET_GLOBAL)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/GET_GLOBAL.inc"
        BREAK;
        CASE(GET_FIELD)
            #include "inc/ops/GET_FIELD.inc"
        BREAK;
        CASE(REF_UPVAL)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/REF_UPVAL.inc"
        BREAK;
        CASE(REF_LOCAL)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/REF_LOCAL.inc"
        BREAK;
        CASE(REF_CLOSED)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/REF_CLOSED.inc"
        BREAK;
        CASE(REF_GLOBAL)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/REF_GLOBAL.inc"
        BREAK;
        CASE(LOAD_NIL)
            #include "inc/ops/LOAD_NIL.inc"
        BREAK;
        CASE(LOAD_UDF)
            #include "inc/ops/LOAD_UDF.inc"
        BREAK;
        CASE(LOAD_LOG)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/LOAD_LOG.inc"
        BREAK;
        CASE(LOAD_INT)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/LOAD_INT.inc"
        BREAK;
        CASE(MAKE_TUP)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/MAKE_TUP.inc"
        BREAK;
        CASE(MAKE_VTUP)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/MAKE_VTUP.inc"
        BREAK;
        CASE(MAKE_CLS)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/MAKE_CLS.inc"
        BREAK;
        CASE(MAKE_REC)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/MAKE_REC.inc"
        BREAK;
        CASE(MAKE_VREC)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/MAKE_VREC.inc"
        BREAK;
        CASE(POP)
            #include "inc/ops/POP.inc"
        BREAK;
        CASE(NEG)
            #include "inc/ops/NEG.inc"
        BREAK;
        CASE(NOT)
            #include "inc/ops/NOT.inc"
        BREAK;
        CASE(FIX)
            #include "inc/ops/FIX.inc"
        BREAK;
        CASE(POW)
            #include "inc/ops/POW.inc"
        BREAK;
        CASE(MUL)
            #include "inc/ops/MUL.inc"
        BREAK;
        CASE(DIV)
            #include "inc/ops/DIV.inc"
        BREAK;
        CASE(MOD)
            #include "inc/ops/MOD.inc"
        BREAK;
        CASE(ADD)
            #include "inc/ops/ADD.inc"
        BREAK;
        CASE(SUB)
            #include "inc/ops/SUB.inc"
        BREAK;
        CASE(LSL)
            #include "inc/ops/LSL.inc"
        BREAK;
        CASE(LSR)
            #include "inc/ops/LSR.inc"
        BREAK;
        CASE(AND)
            #include "inc/ops/AND.inc"
        BREAK;
        CASE(XOR)
            #include "inc/ops/XOR.inc"
        BREAK;
        CASE(OR)
            #include "inc/ops/OR.inc"
        BREAK;
        CASE(IMT)
            // Is More Than.
            #include "inc/ops/IMT.inc"
        BREAK;
        CASE(ILT)
            // Is Less Than.
            #include "inc/ops/ILT.inc"
        BREAK;
        CASE(IME)
            // Is More or Equal
            #include "inc/ops/IME.inc"
        BREAK;
        CASE(ILE)
            // Is Less or Equal
            #include "inc/ops/ILE.inc"
        BREAK;
        CASE(IET)
            // Is Equal To
            #include "inc/ops/IET.inc"
        BREAK;
        CASE(NET)
            // Not Equal To
            #include "inc/ops/NET.inc"
        BREAK;
        CASE(IETU)
            // Is Equal To Udf
            #include "inc/ops/IETU.inc"
        BREAK;
        CASE(AND_JUMP)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/AND_JUMP.inc"
        BREAK;
        CASE(OR_JUMP)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/OR_JUMP.inc"
        BREAK;
        CASE(UDF_JUMP)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/UDF_JUMP.inc"
        BREAK;
        CASE(ALT_JUMP)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/ALT_JUMP.inc"
        BREAK;
        CASE(JUMP)
            ushort const opr = inGetOpr( in );
            #include "inc/ops/JUMP.inc"
        BREAK;
        CASE(CALL)
            #include "inc/ops/CALL.inc"
        BREAK;
        CASE(RETURN)
            #include "inc/ops/RETURN.inc"
        BREAK;
    END
    
    // Restore old register set.
    *rPtr = regs;
    fib->rPtr = rPtr;
}

static void
pushAR( State* state, Fiber* fib, NatAR* nat ) {
    AR* ar;
    if( nat ) {
        ar = &nat->ar;
        
        NatAR** nats;
        if( fib->arStack.top > 0 )
            nats = &fib->arStack.ars[fib->arStack.top-1].nats;
        else
            nats = &fib->nats;
        
        nat->prev = *nats;
        *nats = nat;
    }
    else {
        if( fib->arStack.top >= fib->arStack.cap ) {
            Part arP = {
                .ptr = fib->arStack.ars,
                .sz  = sizeof(VirAR)*fib->arStack.cap
            };
            uint ncap = fib->arStack.cap*2;
            fib->arStack.ars = stateResizeRaw( state, &arP, sizeof(VirAR)*ncap );
            fib->arStack.cap = ncap;
            stateCommitRaw( state, &arP );
        }
        
        VirAR* vir = &fib->arStack.ars[fib->arStack.top++];
        vir->nats = NULL;
        ar = &vir->ar;
    }
    
    ar->cls   = fib->rPtr->cls;
    ar->rAddr = fib->rPtr->ip;
    ar->oLcls = fib->rPtr->lcl - fib->tmpStack.tmps;
}

static void
popAR( State* state, Fiber* fib ) {
    AR* ar = NULL;
    if( fib->arStack.top > 0 ) {
        NatAR** nats = &fib->arStack.ars[fib->arStack.top-1].nats;
        if( *nats ) {
            NatAR* nat = *nats;
            *nats = nat->prev;
            ar = &nat->ar;
        }
        else {
            ar = &fib->arStack.ars[--fib->arStack.top].ar;
        }
    }
    else
    if( fib->nats ) {
        NatAR* nat = fib->nats;
        fib->nats = nat->prev;
        
        ar = &nat->ar;
    }
    else {
        tenAssertNeverReached();
    }
    
    fib->rPtr->cls = ar->cls;
    fib->rPtr->ip  = ar->rAddr;
    fib->rPtr->lcl = fib->tmpStack.tmps + ar->oLcls;
}

static void
ensureStack( State* state, Fiber* fib, uint n ) {
    uint top = fib->rPtr->sp - fib->tmpStack.tmps;
    if( top + n < fib->tmpStack.cap )
        return;
    
    // The address of the stack may change, so save
    // the stack based pointers as offsets to be
    // restored after the resize.
    uint oSp  = fib->rPtr->sp - fib->tmpStack.tmps;
    uint oLcl = fib->rPtr->lcl - fib->tmpStack.tmps;
    
    uint cap = ( top + n ) * 2;
    Part tmpsP = {
        .ptr = fib->tmpStack.tmps,
        .sz  = sizeof(TVal)*fib->tmpStack.cap
    };
    fib->tmpStack.tmps = stateResizeRaw( state, &tmpsP, sizeof(TVal)*cap );
    fib->tmpStack.cap  = cap;
    stateCommitRaw( state, &tmpsP );
    fib->rPtr->sp  = fib->tmpStack.tmps + oSp;
    fib->rPtr->lcl = fib->tmpStack.tmps + oLcl;
}

static void
genTrace( State* state, Fiber* fib ) {
    char const* tag = NULL;
    if( fib->tagged )
        tag = symBuf( state, fib->tag );
    
    // Generate stack trace.
    if( !state->config.ndebug ) {
        if( fib->rPtr->ip ) {
            VirFun* vir  = &fib->rPtr->cls->fun->u.vir;
            ullong place = fib->rPtr->ip - vir->code;
            
            tenAssert( vir->dbg );
            
            uint      nLines = vir->dbg->nLines;
            LineInfo* lines  = vir->dbg->lines;
            for( uint i = 0 ; i < nLines ; i++ ) {
                if( lines[i].start <= place && place < lines[i].end ) {
                    uint        line  = lines[i].line;
                    char const* file  = symBuf( state, vir->dbg->file );
                    statePushTrace( state, tag, file, line );
                    break;
                }
            }
        }
        
        for( long i = (long)fib->arStack.top - 1 ; i >= 0 ; i-- ) {
            NatAR* nIt = fib->arStack.ars[i].nats;
            while( nIt ) {
                statePushTrace( state, tag, nIt->file, nIt->line );
                nIt = nIt->prev;
            }
            if( !fib->arStack.ars[i].ar.rAddr )
                continue;
            
            VirFun* vir   = &fib->arStack.ars[i].ar.cls->fun->u.vir;
            ullong  place = fib->arStack.ars[i].ar.rAddr - vir->code;
            tenAssert( vir->dbg );
            
            uint      nLines = vir->dbg->nLines;
            LineInfo* lines  = vir->dbg->lines;
            for( uint i = 0 ; i < nLines ; i++ ) {
                if( lines[i].start <= place && place < lines[i].end ) {
                    uint        line = lines[i].line;
                    char const* file = symBuf( state, vir->dbg->file );
                    statePushTrace( state, tag, file, line );
                    break;
                }
            }
        }
        
        NatAR* nIt = fib->nats;
        while( nIt ) {
            statePushTrace( state, tag, nIt->file, nIt->line );
            nIt = nIt->prev;
        }
    }
    
    if( fib->parent )
        genTrace( state, fib->parent );
}


static void
onError( State* state, Defer* defer ) {
    if( state->errNum == ten_ERR_FATAL )
        return;
    
    Fiber* fib = (void*)defer - (uintptr_t)&((Fiber*)NULL)->errDefer;
    genTrace( state, fib );
    
    // Set the fiber's error values from the state.
    fib->errNum = state->errNum;
    fib->errVal = state->errVal;
    fib->errStr = state->errStr;
    fib->trace  = stateClaimTrace( state );
    stateClearError( state );
    
    // Set fiber to a failed state.
    fib->state = ten_FIB_FAILED;

    state->fiber->rBuf  = *state->fiber->rPtr;
    state->fiber->rPtr  = &state->fiber->rBuf;
}

static void
errUdfAsArg( State* state, Function* fun, uint arg ) {
    char const* func = "<anon>";
    if( fun->type == FUN_VIR && fun->u.vir.dbg )
        func = symBuf( state, fun->u.vir.dbg->func );
    else
    if( fun->type == FUN_NAT )
        func = symBuf( state, fun->u.nat.name );
    
    stateErrFmtA(
        state, ten_ERR_CALL,
        "Passed `udf` for argument %u to '%s'",
        arg, func
    );
}

static void
errTooFewArgs( State* state, Function* fun, uint argc ) {
    char const* func = "<anon>";
    if( fun->type == FUN_VIR && fun->u.vir.dbg )
        func = symBuf( state, fun->u.vir.dbg->func );
    else
    if( fun->type == FUN_NAT )
        func = symBuf( state, fun->u.nat.name );
    
    stateErrFmtA(
        state, ten_ERR_CALL,
        "Too few arguments to `%s`",
        func
    );
}

static void
errTooManyArgs( State* state, Function* fun, uint argc ) {
    char const* func = "<anon>";
    if( fun->type == FUN_VIR && fun->u.vir.dbg )
        func = symBuf( state, fun->u.vir.dbg->func );
    else
    if( fun->type == FUN_NAT )
        func = symBuf( state, fun->u.nat.name );
    
    stateErrFmtA(
        state, ten_ERR_CALL,
        "Too many arguments to `%s`",
        func
    );
}
