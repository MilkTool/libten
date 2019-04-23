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
errUdfAsArg( State* state, Function* fun, uint arg );

static void
errTooFewArgs( State* state, Function* fun, uint argc );

static void
errTooManyArgs( State* state, Function* fun, uint argc );

void
fibInit( State* state ) {
    state->fibState = NULL;
}

static void
onError( State* state, Defer* defer );

static void
pushFib( State* state, Fiber* fib, NatAR* nat );


Fiber*
fibNew( State* state, Closure* cls, SymT* tag ) {
    Part fibP;
    Fiber* fib = stateAllocObj( state, &fibP, sizeof(Fiber), OBJ_FIB );
    
    uint   vcap = 7;
    Part   vbufP;
    VirAR* vbuf = stateAllocRaw( state, &vbufP, sizeof(VirAR)*vcap );
    
    uint  scap = 16;
    Part  sbufP;;
    TVal* sbuf = stateAllocRaw( state, &sbufP, sizeof(TVal)*scap );
    
    fib->state         = ten_FIB_STOPPED;
    fib->nats          = NULL;
    fib->cons          = NULL;
    fib->virs.cap      = vcap;
    fib->virs.top      = 0;
    fib->virs.buf      = vbuf;
    fib->stack.cap     = scap;
    fib->stack.buf     = sbuf;
    fib->top           = NULL;
    fib->pod           = NULL;
    fib->pud           = NULL;
    fib->pop           = NULL;
    fib->push          = pushFib;
    fib->state         = ten_FIB_STOPPED;
    fib->rptr          = &fib->rbuf;
    fib->entry         = cls;
    fib->parent        = NULL;
    fib->errNum        = ten_ERR_NONE;
    fib->errVal        = tvUdf();
    fib->trace         = NULL;
    fib->defer.cb      = onError;
    fib->yjmp          = NULL;
    
    if( tag ) {
        fib->tag    = *tag;
        fib->tagged = true;
    }
    else {
        fib->tagged = false;
    }
    
    fib->rbuf = (Regs){ .sp = fib->stack.buf };
    
    stateCommitRaw( state, &vbufP );
    stateCommitRaw( state, &sbufP );
    stateCommitObj( state, &fibP );
    
    return fib;
}

Tup
fibPush( State* state, Fiber* fib, uint n ) {
    tenAssert( fib->state == ten_FIB_RUNNING );
    
    // Make sure there's enough room on the stack.
    ensureStack( state, fib, n + 1 );
    
    // Wrap the pushed values in a tuple.
    Tup tup = {
        .base   = &fib->stack.buf,
        .offset = fib->rptr->sp - fib->stack.buf,
        .size   = n
    };
    
    for( uint i = 0 ; i < n ; i++ )
        *(fib->rptr->sp++) = tvUdf();
    
    // Single value tuples should never be wrapped
    // in a tuple header.
    if( n != 1 )
        *(fib->rptr->sp++) = tvTup( n );
    
    return tup;
}

Tup
fibTop( State* state, Fiber* fib ) {
    tenAssert( fib->rptr->sp > fib->stack.buf );
    
    uint loc  = fib->rptr->sp - fib->stack.buf - 1;
    uint size = 1;
    if( tvIsTup( fib->stack.buf[loc] ) ) {
        size = tvGetTup( fib->stack.buf[loc] );
        
        tenAssert( loc >= size );
        loc -= size;
    }
    
    return (Tup){
        .base   = &fib->stack.buf,
        .offset = loc,
        .size   = size
    };
}

void
fibPop( State* state, Fiber* fib ) {
    tenAssert( fib->rptr->sp > fib->stack.buf );
    
    if( tvIsTup( fib->rptr->sp[-1] ) ) {
        uint size = tvGetTup( fib->rptr->sp[-1] );
        
        tenAssert( fib->rptr->sp - size >= fib->stack.buf );
        fib->rptr->sp -= size;
    }
    tenAssert( fib->rptr->sp > fib->stack.buf );
    fib->rptr->sp--;
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
    // into a waiting state.  Remove its defer to prevent
    // errors from the continuation from propegating.
    Fiber* parent = state->fiber;
    if( parent ) {
        parent->state = ten_FIB_WAITING;
        stateCancelDefer( state, &parent->defer );
    }
    
    // Set the fiber that's being continued to the running
    // state and install its defer to catch errors.
    fib->state   = ten_FIB_RUNNING;
    fib->parent  = parent;
    state->fiber = fib;
    stateInstallDefer( state, &fib->defer );
    
    // Install our own error handler to localize non-critical
    // errors to the fiber.
    jmp_buf  errJmp;
    jmp_buf* oldJmp = stateSwapErrJmp( state, &errJmp );
    if( setjmp( errJmp ) ) {
        
        // When an error actually occurs replace the original
        // handler, so any further errors go to the right place.
        stateSwapErrJmp( state, oldJmp );
        
        // Restore the parent fiber to the running state.
        state->fiber = parent;
        if( parent ) {
            parent->state = ten_FIB_RUNNING;
            stateInstallDefer( state, &parent->defer );
            fib->parent = NULL;
        }
        
        // Critical errors are re-thrown, these will be caught
        // by each parent fiber, allowing them to cleanup, but
        // will ultimately propegate back to the user.
        if( fib->errNum == ten_ERR_FATAL )
            stateErrProp( state );
        
        return (Tup){ .base = &fib->stack.buf, .offset = 0, .size = 0 };
    }
    
    
    // This is where we jump to yield from the fiber, it'll
    // ultimately be the last code run in this function if
    // the continuation doesn't fail, since we jump back here
    // at the end.
    jmp_buf yjmp;
    fib->yjmp = &yjmp;
    if( setjmp( yjmp ) ) {
        
        // Restore old error jump.
        stateSwapErrJmp( state, oldJmp );
        
        // Restore the calling fiber.
        state->fiber = parent;
        if( parent ) {
            parent->state = ten_FIB_RUNNING;
            stateInstallDefer( state, &parent->defer );
            fib->parent = NULL;
        }
        
        // Cancel the fiber's error handling defer.
        stateCancelDefer( state, &fib->defer );
        
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
    longjmp( *fib->yjmp, 1 );
}

static void
popVir( State* state, Fiber* fib );

static void
convertFibNats( State* state, Fiber* fib );

void
fibYield( State* state, Tup* vals, bool pop ) {
    Fiber* fib = state->fiber;
    
    // Copy yielded values to expected location on the stack.
    uint valc = vals->size;
    ensureStack( state, fib, valc + 1 );
    
    TVal* dstv = fib->rptr->lcl;
    TVal* valv = *vals->base + vals->offset;
    for( uint i = 0 ; i < valc ; i++ )
        dstv[i] = valv[i];
    fib->rptr->sp = dstv + valc;
    
    if( valc != 1 )
        *(fib->rptr->sp++) = tvTup( valc );
    
    convertFibNats( state, fib );
    
    if( pop )
        fib->pop( state, fib );
    
    // If the current register set holds a context,
    // then replace it with a copy since the current
    // one is either stack allocated in <finishCon>
    // or a pointer to the user's context.
    if( fib->rptr->ip == NULL && fib->rptr->context != NULL ) {
        Part  ctxP;
        void* ctx = stateAllocRaw( state, &ctxP, fib->rptr->ctxSize );
        memcpy( ctx, fib->rptr->context, fib->rptr->ctxSize );
        fib->rptr->context = ctx;
        stateCommitRaw( state, &ctxP );
    }

    // Save register set to buffer.
    fib->rbuf  = *fib->rptr;
    fib->rptr  = &fib->rbuf;
    
    fib->state = ten_FIB_STOPPED;
    longjmp( *fib->yjmp, 1 );
}

long
fibSeek( State* state, void* ctx, size_t size ) {
    Fiber* fib = state->fiber;
    if( fib->rptr->context ) {
        tenAssert( size == fib->rptr->ctxSize );
        memcpy( ctx, fib->rptr->context, size );
        fib->rptr->context = ctx;
    }
    else {
        fib->rptr->context      = ctx;
        fib->rptr->ctxSize      = size;
    }
    return fib->rptr->checkpoint;
}

void
fibCheckpoint( State* state, unsigned cp, Tup* tup ) {
    Fiber* fib = state->fiber;
    tenAssert( fib->rptr->context );
    
    fib->rptr->checkpoint = cp;
    fib->rptr->dstOffset  = (void*)tup - fib->rptr->context;
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
    fib->push( state, fib, &nat );
    doCall( state, fib );
    fib->pop( state, fib );
    
    return fibTop( state, fib );
}

void
fibClearError( State* state, Fiber* fib ) {
    if( fib->errNum == ten_ERR_NONE )
        return;
    
    fib->errNum = ten_ERR_NONE;
    fib->errVal = tvUdf();
    stateFreeTrace( state, fib->trace );
}

void
fibPropError( State* state, Fiber* fib ) {
    if( fib->errNum == ten_ERR_NONE )
        return;
    
    state->errNum = fib->errNum;
    state->errVal = fib->errVal;
    state->trace  = fib->trace;
    fib->trace = NULL;
    
    stateErrProp( state );
}


void
fibTraverse( State* state, Fiber* fib ) {
/*
    NatAR* nIt = fib->nats;
    while( nIt ) {
        stateMark( state, nIt->base.cls );
        nIt = nIt->prev;
    }
    NatAR* cIt = fib->cons;
    while( cIt ) {
        stateMark( state, cIt->base.cls );
        cIt = cIt->prev;
    }
    
    for( uint i = 0 ; i < fib->virs.top ; i++ ) {
        stateMark( state, fib->virs.buf[i].ar.cls );
        
        NatAR* nIt = fib->virs.ars[i].nats;
        while( nIt ) {
            stateMark( state, nIt->base.cls );
            nIt = nIt->prev;
        }

        NatAR* cIt = fib->virs.ars[i].cons;
        while( cIt ) {
            stateMark( state, cIt->base.cls );
            cIt = cIt->prev;
        }
    }
*/

    for( TVal* v = fib->stack.buf ; v < fib->rptr->sp ; v++ )
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
    stateFreeRaw( state, fib->virs.buf, fib->virs.cap*sizeof(VirAR) );
    stateFreeRaw( state, fib->stack.buf, fib->stack.cap*sizeof(TVal) );
    
    if( fib->state == ten_FIB_STOPPED && fib->rptr->context )
        stateFreeRaw( state, fib->rptr->context, fib->rptr->ctxSize );
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
finishCon( State* state, ConAR* con, bool free );

static void
finishCons( State* state, ConAR** cons );

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
    
    // If yield was made from a native function then
    // we need to finish its execution and pop its 
    // frame off the stack; this pop will finish any
    // other continuations automatically, passing
    // the results from the first as continuation
    // arguments.  Then we continue to finish the
    // virtual function calls by entering the interpret
    // loop.
    if( fib->rptr->ip == NULL ) {
        ConAR dum = { 0 };
        dum.base.cls   = fib->rptr->cls,
        dum.base.lcl   = fib->rptr->lcl - fib->stack.buf;
        dum.context    = fib->rptr->context;
        dum.ctxSize    = fib->rptr->ctxSize;
        dum.dstOffset  = fib->rptr->dstOffset;
        dum.checkpoint = fib->rptr->checkpoint;
        finishCon( state, &dum, false );
        
        // The ConARs in fib->cons will only be finished
        // by the pop function if there are also VirARs on
        // the stack; otherwise do it here.
        if( fib->virs.top == 0 ) {
            finishCons( state, &fib->cons );
            fib->cons = NULL;
            fib->pop  = NULL;
            fib->pod  = NULL;
        }
        else {
            fib->pop( state, fib );
            tenAssert( fib->rptr->ip != NULL );
            
            // Finally, we enter the interpret loop.
            doLoop( state, fib );
        }
    }
    else {
        doLoop( state, fib );
    }
}

static void
doCall( State* state, Fiber* fib ) {
    tenAssert( fib->state == ten_FIB_RUNNING );
    tenAssert( fib->rptr->sp > fib->stack.buf + 1 );
    
    Regs* regs = fib->rptr;
    
    // Figure out how many arguments were passed,
    // and where they start.
    TVal* args = &regs->sp[-1];
    uint  argc = 1;
    TVal* argv = args - 1;
    if( tvIsTup( *args ) ) {
        argc = tvGetTup( *args );
        argv -= argc;
        tenAssert( argc < args - fib->stack.buf );
        
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
        
        regs->ip         = NULL;
        regs->context    = NULL;
        regs->checkpoint = -1;
        
        // Initialize an argument tuple for the callback.
        Tup aTup = {
            .base   = &fib->stack.buf,
            .offset = regs->lcl - fib->stack.buf + 1,
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
    Regs* rptr = fib->rptr;
    Regs  regs = *fib->rptr;
    fib->rptr = &regs;
    
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
    *rptr = regs;
    fib->rptr = rptr;
}

static VirAR*
allocVir( State* state ) {
    Fiber* fib = state->fiber;
    if( fib->virs.top >= fib->virs.cap ) {
        uint    vcap = fib->virs.cap * 2;
        Part    bufP = { .ptr = fib->virs.buf, .sz = fib->virs.cap };
        VirAR*  vbuf = stateResizeRaw( state, &bufP, sizeof(NatAR)*vcap );
        
        fib->virs.cap = vcap;
        fib->virs.buf = vbuf;
    }
    
    return &fib->virs.buf[fib->virs.top++];
}

static ConAR*
convertNats( State* state, NatAR* nats, ConAR* tail ) {
    ConAR*  first = NULL;
    ConAR** end   = &first;
    
    NatAR* nat = nats;
    while( nat ) {
        Part   conP;
        ConAR* con = stateAllocRaw( state, &conP, sizeof(ConAR) );
        
        // A memcpy() would be cleaner, but the break
        // would go unnoticed if we change the structures.
        con->base       = nat->base;
        con->file       = nat->file;
        con->line       = nat->line;
        con->context    = nat->context;
        con->ctxSize    = nat->ctxSize;
        con->dstOffset  = nat->dstOffset;
        con->checkpoint = nat->checkpoint;
        if( nat->context ) {
            Part  ctxP;
            con->context = stateAllocRaw( state, &ctxP, nat->ctxSize );
            memcpy( con->context, nat->context, nat->ctxSize );
            stateCommitRaw( state, &ctxP );
        }
        stateCommitRaw( state, &conP );
        
        con->prev = *end;
        *end = con;
        
        nat = nat->prev;
    }
    
    if( *end )
        (**end).prev = tail;
    else
        *end = tail;
    
    return first;
}

static void
convertFibNats( State* state, Fiber* fib ) {
    tenAssert( fib->cons == NULL );
    fib->cons = convertNats( state, fib->nats, fib->cons );
    fib->nats = NULL;
    
    // Since there won't be any NatARs anymore,
    // make sure we start popping VirARs when
    // continued.
    if( fib->virs.top > 0 )
        fib->pop = popVir;
    else
        fib->pop = NULL;
    
    for( uint i = 0 ; i < fib->virs.top ; i++ ) {
        VirAR* vir = &fib->virs.buf[i];
        
        tenAssert( vir->cons == NULL );
        vir->cons = convertNats( state, vir->nats, vir->cons );
        vir->nats = NULL;
    }
}


static void
finishCon( State* state, ConAR* con, bool free ) {
    Fiber* fib = state->fiber;
    
    Closure* cls  = con->base.cls;
    Regs*    regs = fib->rptr;
    
    char context[con->ctxSize];
    memcpy( context, con->context, con->ctxSize );
    
    regs->cls        = con->base.cls;
    regs->ip         = NULL;
    regs->lcl        = fib->stack.buf + con->base.lcl;
    regs->context    = con->ctxSize > 0 ? context : NULL;
    regs->ctxSize    = con->ctxSize;
    regs->dstOffset  = con->dstOffset;
    regs->checkpoint = con->checkpoint;
    
    if( con->context )
        stateFreeRaw( state, con->context, con->ctxSize );
    if( free )
        stateFreeRaw( state, con, sizeof(ConAR) );
    
    // The native function continuation system is optional,
    // if the native function doesn't register a context
    // then instead of being continued it'll just implicitly
    // return an empty tuple.
    if( regs->context == NULL ) {
        regs->sp = regs->lcl;
        fibPush( state, fib, 0 );
        return;
    }
    
    uint argc = cls->fun->nParams;
    if( cls->fun->vargIdx )
        argc++;
    
    Tup args = {
        .base   = &fib->stack.buf,
        .offset = regs->lcl - fib->stack.buf + 1,
        .size   = argc
    };
    
    // If the native callback specified a checkpoint,
    // then it's expected the destination tuple at
    // (ctx + dstOffset) to be populated with the
    // continuation values.  These will either by the
    // continuation arguments (if this is the function
    // that originally yielded), or the results from
    // the yielding function that it called.
    if( regs->checkpoint >= 0 ) {
        Tup* dst = regs->context + regs->dstOffset;
        *dst = fibTop( state, fib );
    }
    
    ten_Tup t;
    if( cls->dat.dat != NULL ) {
        Data* dat = cls->dat.dat;
        Tup mems = {
            .base   = &dat->mems,
            .offset = 0,
            .size   = dat->info->nMems
        };
        t = cls->fun->u.nat.cb( (ten_State*)state, (ten_Tup*)&args, (ten_Tup*)&mems, dat->data );
    }
    else {
        t = cls->fun->u.nat.cb( (ten_State*)state, (ten_Tup*)&args, NULL, NULL );
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

static void
finishCons( State* state, ConAR** cons ) {
    while( *cons ) {
        ConAR* con = *cons;
        *cons = con->prev;
        
        finishCon( state, con, true );
    }
}

static void
popFibNats( State* state, Fiber* fib ) {
    NatAR* top = fib->nats;
    fib->rptr->cls = top->base.cls;
    fib->rptr->lcl = fib->stack.buf + top->base.lcl;
    fib->rptr->ip  = NULL;
    
    fib->rptr->context      = top->context;
    fib->rptr->ctxSize      = top->ctxSize;
    fib->rptr->dstOffset    = top->dstOffset;
    fib->rptr->checkpoint   = top->checkpoint;
    
    fib->nats = fib->nats->prev;
    if( fib->nats ) {
        fib->top = &fib->nats->base;
    }
    else {
        fib->top = NULL;
        fib->pop = NULL;
        fib->pod = NULL;
    }
}


static void
popVir( State* state, Fiber* fib ) {
    VirAR* top = &fib->virs.buf[fib->virs.top-1];
    
    finishCons( state, &top->cons );
    
    fib->rptr->cls = top->base.cls;
    fib->rptr->lcl = fib->stack.buf + top->base.lcl;
    fib->rptr->ip  = top->ip;
    fib->virs.top--;
    if( fib->virs.top > 0 ) {
        fib->top = (AR*)(top - 1);
    }
    else {
        finishCons( state, &fib->cons );
        fib->cons = NULL;
        
        if( fib->nats ) {
            fib->pop  = popFibNats;
            fib->pod  = NULL;
            fib->pop( state, fib );
        }
        else {
            fib->top = NULL;
            fib->pop = NULL;
            fib->pod = NULL;
        }
    }
}

static void
popVirNats( State* state, Fiber* fib ) {
    VirAR* vir = fib->pod;
    
    NatAR* top = vir->nats;
    fib->rptr->cls = top->base.cls;
    fib->rptr->lcl = fib->stack.buf + top->base.lcl;
    fib->rptr->ip  = NULL;
    
    fib->rptr->context      = top->context;
    fib->rptr->ctxSize      = top->ctxSize;
    fib->rptr->dstOffset    = top->dstOffset;
    fib->rptr->checkpoint   = top->checkpoint;
    
    vir->nats = vir->nats->prev;
    if( vir->nats ) {
        fib->top = &vir->nats->base;
    }
    else {
        fib->top = &vir->base;
        fib->pop = popVir;
        fib->pod = NULL;
    }
}


static void
pushVir( State* state, Fiber* fib, NatAR* nat ) {
    AR* ar = NULL;
    if( nat ) {
        VirAR* vir = fib->pud;
        nat->prev = vir->nats;
        vir->nats = nat;
        
        nat->context    = fib->rptr->context;
        nat->ctxSize    = fib->rptr->ctxSize;
        nat->dstOffset  = fib->rptr->dstOffset;
        nat->checkpoint = fib->rptr->checkpoint;
        
        ar = &nat->base;
        
        fib->pop = popVirNats;
        fib->pod = vir;
    }
    else {
        VirAR* vir = allocVir( state );
        vir->cons  = NULL;
        vir->nats  = NULL;
        
        vir->ip = fib->rptr->ip;
        
        ar = &vir->base;
        
        fib->pop  = popVir;
        fib->pod  = NULL;
    }
    ar->cls = fib->rptr->cls;
    ar->lcl = fib->rptr->lcl - fib->stack.buf;
    
    fib->top = ar;
}

static void
pushFib( State* state, Fiber* fib, NatAR* nat ) {
    AR* ar = NULL;
    if( nat ) {
        nat->prev = fib->nats;
        fib->nats = nat;
        
        nat->context    = fib->rptr->context;
        nat->ctxSize    = fib->rptr->ctxSize;
        nat->dstOffset  = fib->rptr->dstOffset;
        nat->checkpoint = fib->rptr->checkpoint;
        
        ar = &nat->base;
        
        fib->pop  = popFibNats;
        fib->pod  = NULL;
    }
    else {
        VirAR* vir = allocVir( state );
        vir->cons  = NULL;
        vir->nats  = NULL;
        
        vir->ip = fib->rptr->ip;
        
        ar = &vir->base;
        
        fib->push = pushVir;
        fib->pud  = vir;
        fib->pop  = popVir;
        fib->pod  = NULL;
    }
    ar->cls = fib->rptr->cls;
    ar->lcl = fib->rptr->lcl - fib->stack.buf;
    
    fib->top = ar;
}

static void
ensureStack( State* state, Fiber* fib, uint n ) {
    uint top = fib->rptr->sp - fib->stack.buf;
    if( top + n < fib->stack.cap )
        return;
    
    // The address of the stack may change, so save
    // the stack based pointers as offsets to be
    // restored after the resize.
    uint osp  = fib->rptr->sp - fib->stack.buf;
    uint olcl = fib->rptr->lcl - fib->stack.buf;
    
    uint cap = ( top + n ) * 2;
    Part tmpsP = {
        .ptr = fib->stack.buf,
        .sz  = sizeof(TVal)*fib->stack.cap
    };
    fib->stack.buf = stateResizeRaw( state, &tmpsP, sizeof(TVal)*cap );
    fib->stack.cap  = cap;
    stateCommitRaw( state, &tmpsP );
    fib->rptr->sp  = fib->stack.buf + osp;
    fib->rptr->lcl = fib->stack.buf + olcl;
}

static void
genTrace( State* state, Fiber* fib ) {
    char const* tag = NULL;
    if( fib->tagged )
        tag = symBuf( state, fib->tag );
    
    if( state->config.ndebug )
        return;
    
    
    // We can only generate a trace entry for the
    // current position when in a bytecode function.
    if( fib->rptr->ip ) {
        VirFun* vir  = &fib->rptr->cls->fun->u.vir;
        ullong place = fib->rptr->ip - vir->code;
        
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
    
    // All NatAR's should have been converted to ConARs by
    // this point, so we only use VirARs and NatARs for the
    // trace.
    
    for( long i = (long)fib->virs.top - 1 ; i >= 0 ; i-- ) {
        VirAR* vir = &fib->virs.buf[i];
        
        tenAssert( vir->nats == NULL );
        
        ConAR* con = fib->virs.buf[i].cons;
        while( con ) {
            statePushTrace( state, tag, con->file, con->line );
            con = con->prev;
        }
        VirFun* fun   = &vir->base.cls->fun->u.vir;
        ullong  place = vir->ip - fun->code;
        tenAssert( fun->dbg );
        
        uint      nLines = fun->dbg->nLines;
        LineInfo* lines  = fun->dbg->lines;
        for( uint i = 0 ; i < nLines ; i++ ) {
            if( lines[i].start <= place && place < lines[i].end ) {
                uint        line = lines[i].line;
                char const* file = symBuf( state, fun->dbg->file );
                statePushTrace( state, tag, file, line );
                break;
            }
        }
    }
    
    ConAR* con = fib->cons;
    while( con ) {
        statePushTrace( state, tag, con->file, con->line );
        con = con->prev;
    }
    
    if( fib->parent )
        genTrace( state, fib->parent );
}


static void
onError( State* state, Defer* defer ) {
    if( state->errNum == ten_ERR_FATAL )
        return;
    
    Fiber* fib = (void*)defer - (uintptr_t)&((Fiber*)NULL)->defer;
    convertFibNats( state, fib );
    genTrace( state, fib );
    
    // Set the fiber's error values from the state.
    fib->errNum = state->errNum;
    fib->errVal = state->errVal;
    fib->trace  = stateClaimTrace( state );
    stateClearError( state );
    
    // Set fiber to a failed state.
    fib->state = ten_FIB_FAILED;

    state->fiber->rbuf  = *state->fiber->rptr;
    state->fiber->rptr  = &state->fiber->rbuf;
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
