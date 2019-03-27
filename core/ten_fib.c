#include "ten_fib.h"
#include "ten_sym.h"
#include "ten_ptr.h"
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
doCall( State* state, Fiber* fib, bool tail );

static void
doTailCall( State* state, Fiber* fib );

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
        
        Closure* cls = clsNew( state, fun, NULL );
        test.cls = cls;
        
        Fiber* fib = fibNew( state, cls );
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
        
        Closure* cls = clsNew( state, fun, NULL );
        test.cls = cls;
        
        Fiber* fib = fibNew( state, cls );
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
        
        Closure* cls = clsNew( state, fun, NULL );
        test.cls = cls;
        
        Fiber* fib = fibNew( state, cls );
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
        
        Closure* cls = clsNew( state, fun, NULL );
        test.cls = cls;
        
        Fiber* fib = fibNew( state, cls );
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
        
        Closure* cls = clsNew( state, fun, NULL );
        test.cls = cls;
        
        Fiber* fib = fibNew( state, cls );
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
        
        Closure* cls = clsNew( state, fun, NULL );
        test.cls = cls;
        
        Fiber* fib = fibNew( state, cls );
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
        
        Closure* cls = clsNew( state, fun, NULL );
        test.cls = cls;
        
        Fiber* fib = fibNew( state, cls );
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
fibNew( State* state, Closure* cls ) {
    Part fibP;
    Fiber* fib = stateAllocObj( state, &fibP, sizeof(Fiber), OBJ_FIB );
    
    uint arCap = 7;
    Part arP;
    VirAR* ar = stateAllocRaw( state, &arP, sizeof(VirAR)*arCap );
    
    uint tmpCap = 16;
    Part tmpsP;
    TVal* tmps = stateAllocRaw( state, &tmpsP, sizeof(TVal)*tmpCap );
    
    fib->state         = FIB_STOPPED;
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
    if( fib->state == FIB_RUNNING )
        stateErrFmtA( state, ten_ERR_FIBER, "Continued running fiber" );
    if( fib->state == FIB_WAITING )
        stateErrFmtA( state, ten_ERR_FIBER, "Continued waiting fiber" );
    if( fib->state == FIB_FINISHED )
        stateErrFmtA( state, ten_ERR_FIBER, "Continued finished fiber" );
    if( fib->state == FIB_FAILED )
        stateErrFmtA( state, ten_ERR_FIBER, "Continued failed fiber" );
    
    
    // Put the parent fiber (the current one at this point)
    // into a waiting state.  Remove its errDefer to prevent
    // errors from the continuation from propegating.
    Fiber* parent = state->fiber;
    if( parent ) {
        parent->state = FIB_WAITING;
        stateCancelDefer( state, &parent->errDefer );
    }
    
    // Set the fiber that's being continued to the running
    // state and install its errDefer to catch errors.
    fib->state   = FIB_RUNNING;
    fib->parent  = parent;
    state->fiber = fib;
    stateInstallDefer( state, &fib->errDefer );
    
    // Install our own error handler to localize non-critical
    // errors to the fiber.
    jmp_buf  errJmp;
    jmp_buf* oldJmp = stateSwapErrJmp( state, &errJmp );
    int err = setjmp( errJmp );
    if( err ) {
        
        // When an error actually occurs replace the original
        // handler, so any further errors go to the right place.
        stateSwapErrJmp( state, oldJmp );
        
        // Restore the parent fiber to the running state.
        state->fiber = parent;
        if( parent ) {
            parent->state = FIB_RUNNING;
            stateInstallDefer( state, &parent->errDefer );
            fib->parent = NULL;
        }
        
        // Critical errors are re-thrown, these will be caught
        // by each parent fiber, allowing them to cleanup, but
        // will ultimately propegate back to the user.
        if( err == ten_ERR_MEMORY )
            stateErrProp( state );
        
        return (Tup){ .base = NULL, .offset = 0, .size = 0 };
    }
    
    
    // This is where we jump to yield from the fiber, it'll
    // ultimately be the last code run in this function if
    // the continuation doesn't fail, since we jump back here
    // at the end.
    jmp_buf yieldJmp;
    fib->yieldJmp = &yieldJmp;
    int sig = setjmp( yieldJmp );
    if( sig ) {
        
        // Restore the calling fiber.
        state->fiber = parent;
        if( parent ) {
            parent->state = FIB_RUNNING;
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
    fib->state = FIB_FINISHED;
    longjmp( *fib->yieldJmp, 1 );
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
}

void
fibDestruct( State* state, Fiber* fib ) {
    stateFreeRaw( state, fib->arStack.ars, fib->arStack.cap*sizeof(VirAR) );
    stateFreeRaw( state, fib->tmpStack.tmps, fib->tmpStack.cap*sizeof(TVal) );
    fib->arStack.cap   = 0;
    fib->arStack.ars   = NULL;
    fib->tmpStack.cap  = 0;
    fib->tmpStack.tmps = NULL;
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
    
    Tup args2 = fibPush( state, fib, args->size );
    for( uint i = 0 ; i < args->size ; i++ )
        tupAt( args2, i ) = tupAt( *args, i );
    
    // That's all for initialization, the call routine
    // will take care of the rest.
    doCall( state, fib, false );
}

static void
contNext( State* state, Fiber* fib, Tup* args ) {
    tenAssert( fib->entry == NULL );
    
    // The previous continuation will have left its
    // return/yield values on the stack, so we pop
    // those.
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
doCall( State* state, Fiber* fib, bool tail ) {
    tenAssert( fib->state == FIB_RUNNING );
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
        regs->sp = extra + 1;
    }
    
    // For a tail call we copy the arguments and the closure
    // itself down the stack to replace the previous call's
    // frame.
    if( tail ) {
        for( uint i = 0 ; i <= argc ; i++ )
            regs->lcl[i] = argv[i];
        argv = regs->lcl;
    }
    else {
        regs->lcl = argv;
    }
    
    regs->cls = cls;
    if( cls->fun->type == FUN_VIR ) {
        VirFun* fun = & cls->fun->u.vir;
        ensureStack( state, fib, fun->nLocals + fun->nTemps );
        
        regs->sp += fun->nLocals;
        regs->ip = cls->fun->u.vir.code;
        doLoop( state, fib );
    }
    else {
        // This is used below to make sure the function call left
        // a return value on the stack.
        #ifndef rigK_NDEBUG
            uint oTop = regs->sp - fib->tmpStack.tmps;
        #endif
        
        regs->ip = NULL;
        
        // Initialize an argument tuple for the callback.
        Tup aTup = {
            .base   = &fib->tmpStack.tmps,
            .offset = regs->lcl - fib->tmpStack.tmps,
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
        tenAssert( oTop < regs->sp - fib->tmpStack.tmps );
    }
    
    
    // After the call the return values will be left at
    // the top of the stack, we need to copy them down
    // to the start of the call frame.
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

static void
doLoop( State* state, Fiber* fib ) {
    // Copy the current set of registers to a local struct
    // for faster access.  These will be copied back to the
    // original struct before returning.
    Regs* rPtr = fib->rPtr;
    Regs  regs = *fib->rPtr;
    fib->rPtr = &regs;
    
    // Original implementation of Rig used computed gotos
    // if enabled; but this isn't standard C and there's
    // no easy way to automatically test for their support
    // at compile time.  They've also been shown to perform
    // worse than a normal switch with some setups... so
    // for now I'll just use a regular switch, a computed
    // goto implementation can be added in the future if
    // it can be shown to offset a performance advantage.
    while( true ) {
        instr in = *(regs.ip++);
        switch( inGetOpc( in ) ) {
            case OPC_DEF_ONE: {
                #include "inc/ops/DEF_ONE.inc"
            } break;
            case OPC_DEF_TUP: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/DEF_TUP.inc"
            } break;
            case OPC_DEF_VTUP: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/DEF_VTUP.inc"
            } break;
            case OPC_DEF_REC: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/DEF_REC.inc"
            } break;
            case OPC_DEF_VREC: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/DEF_VREC.inc"
            } break;
            case OPC_SET_ONE: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/SET_ONE.inc"
            } break;
            case OPC_SET_TUP: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/SET_TUP.inc"
            } break;
            case OPC_SET_VTUP: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/SET_VTUP.inc"
            } break;
            case OPC_SET_REC: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/SET_REC.inc"
            } break;
            case OPC_SET_VREC: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/SET_VREC.inc"
            } break;
            

            case OPC_REC_DEF_ONE: {
                #include "inc/ops/REC_DEF_ONE.inc"
            } break;
            case OPC_REC_DEF_TUP: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/REC_DEF_TUP.inc"
            } break;
            case OPC_REC_DEF_VTUP: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/REC_DEF_VTUP.inc"
            } break;
            case OPC_REC_DEF_REC: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/REC_DEF_REC.inc"
            } break;
            case OPC_REC_DEF_VREC: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/REC_DEF_VREC.inc"
            } break;
            case OPC_REC_SET_ONE: {
                #include "inc/ops/REC_SET_ONE.inc"
            } break;
            case OPC_REC_SET_TUP: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/REC_SET_TUP.inc"
            } break;
            case OPC_REC_SET_VTUP: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/REC_SET_VTUP.inc"
            } break;
            case OPC_REC_SET_REC: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/REC_SET_REC.inc"
            } break;
            case OPC_REC_SET_VREC: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/REC_SET_VREC.inc"
            } break;
            
            case OPC_GET_CONST: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/GET_CONST.inc"
            } break;
            case OPC_GET_CONST0: {
                ushort const opr = 0;
                #include "inc/ops/GET_CONST.inc"
            } break;
            case OPC_GET_CONST1: {
                ushort const opr = 1;
                #include "inc/ops/GET_CONST.inc"
            } break;
            case OPC_GET_CONST2: {
                ushort const opr = 2;
                #include "inc/ops/GET_CONST.inc"
            } break;
            case OPC_GET_CONST3: {
                ushort const opr = 3;
                #include "inc/ops/GET_CONST.inc"
            } break;
            case OPC_GET_CONST4: {
                ushort const opr = 4;
                #include "inc/ops/GET_CONST.inc"
            } break;
            case OPC_GET_CONST5: {
                ushort const opr = 5;
                #include "inc/ops/GET_CONST.inc"
            } break;
            case OPC_GET_CONST6: {
                ushort const opr = 6;
                #include "inc/ops/GET_CONST.inc"
            } break;
            case OPC_GET_CONST7: {
                ushort const opr = 7;
                #include "inc/ops/GET_CONST.inc"
            } break;
            
            case OPC_GET_UPVAL: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/GET_UPVAL.inc"
            } break;
            case OPC_GET_UPVAL0: {
                ushort const opr = 0;
                #include "inc/ops/GET_UPVAL.inc"
            } break;
            case OPC_GET_UPVAL1: {
                ushort const opr = 1;
                #include "inc/ops/GET_UPVAL.inc"
            } break;
            case OPC_GET_UPVAL2: {
                ushort const opr = 2;
                #include "inc/ops/GET_UPVAL.inc"
            } break;
            case OPC_GET_UPVAL3: {
                ushort const opr = 3;
                #include "inc/ops/GET_UPVAL.inc"
            } break;
            case OPC_GET_UPVAL4: {
                ushort const opr = 4;
                #include "inc/ops/GET_UPVAL.inc"
            } break;
            case OPC_GET_UPVAL5: {
                ushort const opr = 5;
                #include "inc/ops/GET_UPVAL.inc"
            } break;
            case OPC_GET_UPVAL6: {
                ushort const opr = 6;
                #include "inc/ops/GET_UPVAL.inc"
            } break;
            case OPC_GET_UPVAL7: {
                ushort const opr = 7;
                #include "inc/ops/GET_UPVAL.inc"
            } break;
            
            case OPC_GET_LOCAL: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/GET_LOCAL.inc"
            } break;
            case OPC_GET_LOCAL0: {
                ushort const opr = 0;
                #include "inc/ops/GET_LOCAL.inc"
            } break;
            case OPC_GET_LOCAL1: {
                ushort const opr = 1;
                #include "inc/ops/GET_LOCAL.inc"
            } break;
            case OPC_GET_LOCAL2: {
                ushort const opr = 2;
                #include "inc/ops/GET_LOCAL.inc"
            } break;
            case OPC_GET_LOCAL3: {
                ushort const opr = 3;
                #include "inc/ops/GET_LOCAL.inc"
            } break;
            case OPC_GET_LOCAL4: {
                ushort const opr = 4;
                #include "inc/ops/GET_LOCAL.inc"
            } break;
            case OPC_GET_LOCAL5: {
                ushort const opr = 5;
                #include "inc/ops/GET_LOCAL.inc"
            } break;
            case OPC_GET_LOCAL6: {
                ushort const opr = 6;
                #include "inc/ops/GET_LOCAL.inc"
            } break;
            case OPC_GET_LOCAL7: {
                ushort const opr = 7;
                #include "inc/ops/GET_LOCAL.inc"
            } break;
            
            case OPC_GET_CLOSED: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/GET_CLOSED.inc"
            } break;
            case OPC_GET_CLOSED0: {
                ushort const opr = 0;
                #include "inc/ops/GET_CLOSED.inc"
            } break;
            case OPC_GET_CLOSED1: {
                ushort const opr = 1;
                #include "inc/ops/GET_CLOSED.inc"
            } break;
            case OPC_GET_CLOSED2: {
                ushort const opr = 2;
                #include "inc/ops/GET_CLOSED.inc"
            } break;
            case OPC_GET_CLOSED3: {
                ushort const opr = 3;
                #include "inc/ops/GET_CLOSED.inc"
            } break;
            case OPC_GET_CLOSED4: {
                ushort const opr = 4;
                #include "inc/ops/GET_CLOSED.inc"
            } break;
            case OPC_GET_CLOSED5: {
                ushort const opr = 5;
                #include "inc/ops/GET_CLOSED.inc"
            } break;
            case OPC_GET_CLOSED6: {
                ushort const opr = 6;
                #include "inc/ops/GET_CLOSED.inc"
            } break;
            case OPC_GET_CLOSED7: {
                ushort const opr = 7;
                #include "inc/ops/GET_CLOSED.inc"
            } break;
            
            case OPC_GET_GLOBAL: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/GET_GLOBAL.inc"
            } break;
            
            case OPC_GET_FIELD: {
                #include "inc/ops/GET_FIELD.inc"
            } break;
            case OPC_GET_FIELDS: {
                #include "inc/ops/GET_FIELDS.inc"
            } break;
            
            case OPC_REF_UPVAL: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/REF_UPVAL.inc"
            } break;
            case OPC_REF_LOCAL: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/REF_LOCAL.inc"
            } break;
            case OPC_REF_CLOSED: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/REF_CLOSED.inc"
            } break;
            case OPC_REF_GLOBAL: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/REF_GLOBAL.inc"
            } break;
            
            case OPC_LOAD_NIL: {
                #include "inc/ops/LOAD_NIL.inc"
            } break;
            case OPC_LOAD_UDF: {
                #include "inc/ops/LOAD_UDF.inc"
            } break;
            case OPC_LOAD_LOG: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/LOAD_LOG.inc"
            } break;
            case OPC_LOAD_INT: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/LOAD_INT.inc"
            } break;
            
            case OPC_MAKE_TUP: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/MAKE_TUP.inc"
            } break;
            case OPC_MAKE_VTUP: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/MAKE_VTUP.inc"
            } break;
            case OPC_MAKE_CLS: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/MAKE_CLS.inc"
            } break;
            case OPC_MAKE_REC: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/MAKE_REC.inc"
            } break;
            case OPC_MAKE_VREC: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/MAKE_VREC.inc"
            } break;
            
            case OPC_POP: {
                #include "inc/ops/POP.inc"
            } break;
            
            case OPC_NEG: {
                #include "inc/ops/NEG.inc"
            } break;
            case OPC_NOT: {
                #include "inc/ops/NOT.inc"
            } break;
            case OPC_FIX: {
                #include "inc/ops/FIX.inc"
            } break;
            
            case OPC_POW: {
                #include "inc/ops/POW.inc"
            } break;
            case OPC_MUL: {
                #include "inc/ops/MUL.inc"
            } break;
            case OPC_DIV: {
                #include "inc/ops/DIV.inc"
            } break;
            case OPC_MOD: {
                #include "inc/ops/MOD.inc"
            } break;
            case OPC_ADD: {
                #include "inc/ops/ADD.inc"
            } break;
            case OPC_SUB: {
                #include "inc/ops/SUB.inc"
            } break;
            case OPC_LSL: {
                #include "inc/ops/LSL.inc"
            } break;
            case OPC_LSR: {
                #include "inc/ops/LSR.inc"
            } break;
            case OPC_AND: {
                #include "inc/ops/AND.inc"
            } break;
            case OPC_XOR: {
                #include "inc/ops/XOR.inc"
            } break;
            case OPC_OR: {
                #include "inc/ops/OR.inc"
            } break;
            case OPC_IMT: {
                // Is More Than.
                #include "inc/ops/IMT.inc"
            } break;
            case OPC_ILT: {
                // Is Less Than.
                #include "inc/ops/ILT.inc"
            } break;
            case OPC_IME: {
                // Is More or Equal
                #include "inc/ops/IME.inc"
            } break;
            case OPC_ILE: {
                // Is Less or Equal
                #include "inc/ops/ILE.inc"
            } break;
            case OPC_IET: {
                // Is Equal To
                #include "inc/ops/IET.inc"
            } break;
            case OPC_NET: {
                // Not Equal To
                #include "inc/ops/NET.inc"
            } break;
            case OPC_IETU: {
                // Is Equal To Udf
                #include "inc/ops/IETU.inc"
            } break;
            
            case OPC_AND_JUMP: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/AND_JUMP.inc"
            } break;
            case OPC_OR_JUMP: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/OR_JUMP.inc"
            } break;
            case OPC_UDF_JUMP: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/UDF_JUMP.inc"
            } break;
            case OPC_ALT_JUMP: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/ALT_JUMP.inc"
            } break;
            case OPC_JUMP: {
                ushort const opr = inGetOpr( in );
                #include "inc/ops/JUMP.inc"
            } break;
            
            case OPC_CALL: {
                #include "inc/ops/CALL.inc"
            } break;
            case OPC_RETURN: {
                #include "inc/ops/RETURN.inc"
            } break;
            default: {
                tenAssertNeverReached();
            } break;
        }
    }
    end:
    
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
    AR* ar;
    if( fib->nats ) {
        NatAR* nat = fib->nats;
        fib->nats = nat->prev;
        
        ar = &nat->ar;
    }
    else
    if( fib->arStack.top >= fib->arStack.cap ) {
        NatAR** nats = &fib->arStack.ars[fib->arStack.top-1].nats;
        NatAR* nat = *nats;
        if( *nats ) {
            *nats = nat->prev;
            ar = &nat->ar;
        }
        else {
            ar = &fib->arStack.ars[fib->arStack.top-1].ar;
        }
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
    if( fib->tmpStack.tmps - fib->rPtr->sp + n < fib->tmpStack.cap )
        return;
    
    // The address of the stack may change, so save
    // the stack based pointers as offsets to be
    // restored after the resize.
    uint oSp  = fib->rPtr->sp - fib->tmpStack.tmps;
    uint oLcl = fib->rPtr->lcl - fib->tmpStack.tmps;
    
    uint cap = fib->tmpStack.cap * 2;
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
onError( State* state, Defer* defer ) {
    Fiber* fib = (void*)defer - (ullong)&((Fiber*)NULL)->errDefer;
    
    // Set the fiber's error values from the state.
    fib->errNum = state->errNum;
    fib->errVal = state->errVal;
    fib->errStr = state->errStr;
    fib->trace  = stateClaimTrace( state );
    stateClearError( state );
    
    // Set fiber to a failed state.
    fib->state = FIB_FAILED;
}

static void
errUdfAsArg( State* state, Function* fun, uint arg ) {
    stateErrFmtA(
        state, ten_ERR_CALL,
        "Passed `udf` for argument %u",
        arg
    );
}

static void
errTooFewArgs( State* state, Function* fun, uint argc ) {
    char const* func = "<anon>";
    if( fun->type == FUN_VIR && fun->u.vir.dbg )
        func = symBuf( state, fun->u.vir.dbg->func );
    else
        func = symBuf( state, fun->u.nat.name );
    
    stateErrFmtA(
        state, ten_ERR_CALL,
        "Too many arguments to `%s`",
        func
    );
}

static void
errTooManyArgs( State* state, Function* fun, uint argc ) {
    char const* func = "<anon>";
    if( fun->type == FUN_VIR && fun->u.vir.dbg )
        func = symBuf( state, fun->u.vir.dbg->func );
    else
        func = symBuf( state, fun->u.nat.name );
    
    stateErrFmtA(
        state, ten_ERR_CALL,
        "Too few arguments to `%s`",
        func
    );
}
