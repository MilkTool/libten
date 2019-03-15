#include "ten_fib.h"
#include "ten_sym.h"
#include "ten_ptr.h"
#include "ten_rec.h"
#include "ten_dat.h"
#include "ten_opcodes.h"
#include "ten_assert.h"
#include "ten_cls.h"
#include "ten_fun.h"
#include <string.h>
#include <limits.h>

typedef enum {
    REF_GLOBAL,
    REF_UPVAL,
    REF_LOCAL,
    REF_CLOSED
} RefType;

#define refType( REF )          ((REF) & 0x3)
#define refMake( TYPE, LOC )    ((RefT)(LOG) << 2 | (TYPE))

static void
ensureStack( State* state, Fiber* fib, uint n );

static void
freeStack( State* state, Fiber* fib );

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
    fib->errTrace      = NULL;
    fib->errVal        = tvUdf();
    fib->errStr        = NULL;
    fib->yieldJmp      = NULL;
    
    memset( &fib->rBuf, 0, sizeof(Regs) );
    
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
        if( parent ) {
            parent->state = FIB_RUNNING;
            stateInstallDefer( state, &parent->errDefer );
            state->fiber = parent;
            fib->parent = NULL;
        }
        
        // Critical errors are re-thrown, these will be caught
        // by each parent fiber, allowing them to cleanup, but
        // will ultimately propegate back to the user.
        if( err == ten_ERR_MEMORY )
            stateErrProp( state );
        
        // Once a fiber has failed it can never be continued
        // again, so has no need for a stack.  So we'd might
        // as well free up the resources.
        freeStack( state, fib );
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
        if( parent ) {
            parent->state = FIB_RUNNING;
            stateInstallDefer( state, &parent->errDefer );
            state->fiber = parent;
            fib->parent = NULL;
        }
        
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
    // TODO
}

void
fibDestruct( State* state, Fiber* fib ) {
    // TODO
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
    doCall( state, fib );
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
doCall( State* state, Fiber* fib ) {
    tenAssert( fib->state == FIB_RUNNING );
    tenAssert( fib->rPtr->sp > fib->tmpStack.tmps + 1 );
    
    Regs* regs = fib->rPtr;
    
    // Figure out how many arguments were passed,
    // and where they start.
    TVal* args = &regs->sp[-1];
    uint  argc = 1;
    TVal* argv = args;
    if( tvIsTup( *args ) ) {
        argc = tvGetTup( *args );
        argv -= argc;
        tenAssert( argc < fib->tmpStack.tmps - args );
        
        // Pop the tuple header, it's no longer needed.
        regs->sp--;
    }
    
    // Closure should be on the stack just below the arguments.
    tenAssert( tvIsObj( argv[-1] ) );
    tenAssert( datGetTag( tvGetObj( argv[-1] ) ) == OBJ_CLS );
    
    Closure* cls = tvGetObj( argv[-1] );
    uint parc = cls->fun->nParams;
    
    // Check the arguments for `udf`.
    for( uint i = 0 ; i < argc ; i++ )
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
        
        uint diff = argc - parc;
        for( uint i = 0 ; i < diff ; i++ ) {
            TVal key = tvInt( i );
            recDef( state, rec, key, argv[parc+i] );
        }
        
        // Record is set as the last argument, after the
        // place of the last non-variatic parameter.
        argv[parc] = tvObj( rec );
        argc = parc + 1;
        
        // Adjust the stack pointer to again point to the
        // slot just after the arguments.
        regs->sp = regs->sp - diff + 1;
    }
    
    // At this point we need to allocate room for the function
    // on the stack.  But this may reallocate the stack itself,
    // rendering our pointers invalid.  So we back up the base
    // of this call frame (that's the location of the closure)
    // as an offset from the stack pointer.
    uint base = argv - 1 - fib->tmpStack.tmps;
    uint nLocals = 0;
    uint nTemps  = 0;
    if( cls->fun->type == FUN_VIR ) {
        nLocals = cls->fun->u.vir.nLocals;
        nTemps  = cls->fun->u.vir.nTemps;
        ensureStack( state, fib, nLocals + nTemps );
    }
    
    // Finally initialize the registers and execute the bytecode
    // or call the user callback if the function is a native one.
    regs->sp += nLocals;
    regs->cls = cls;
    
    // This is used below to make sure the function call left
    // a return value on the stack.
    #ifndef rigK_NDEBUG
        uint oTop = regs->sp - fib->tmpStack.tmps;
    #endif
    
    if( cls->fun->type == FUN_VIR ) {
        regs->ip   = cls->fun->u.vir.code;
        regs->lcl  = fib->tmpStack.tmps + base;
        doLoop( state, fib );
    }
    else {
        regs->ip   = NULL;
        regs->lcl  = NULL;
        
        // Initialize an argument tuple for the callback.
        Tup aTup = {
            .base   = &fib->tmpStack.tmps,
            .offset = base,
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
            cls->fun->u.nat.cb( (ten_Core*)state, (ten_Tup*)&aTup, (ten_Tup*)&mTup, dat->data );
        }
        else {
            cls->fun->u.nat.cb( (ten_Core*)state, (ten_Tup*)&aTup, NULL, NULL );
        }
    }
    
    // Make sure the call left a return value.
    tenAssert( oTop < regs->sp - fib->tmpStack.tmps );
    
    // After the call the return values will be left at
    // the top of the stack, we need to copy them down
    // to the start of the call frame.
    uint  retc = 1;
    TVal* retv = &regs->sp[-1];
    if( tvIsTup( *retv ) ) {
        retc += tvGetTup( *retv );
        retv -= retc - 1;
    }
    TVal* dstv = fib->tmpStack.tmps + base;
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
        switch( inGetOpr( in ) ) {
            case OPC_DEF_ONE: {
                #include "inc/ops/DEF_ONE.inc"
            } break;
            case OPC_DEF_TUP: {
                #include "inc/ops/DEF_TUP.inc"
            } break;
            case OPC_DEF_VTUP: {
                #include "inc/ops/DEF_VTUP.inc"
            } break;
            case OPC_DEF_REC: {
                #include "inc/ops/DEF_REC.inc"
            } break;
            case OPC_DEF_VREC: {
                #include "inc/ops/DEF_VREC.inc"
            } break;
            case OPC_SET_ONE: {
                #include "inc/ops/SET_ONE.inc"
            } break;
            case OPC_SET_TUP: {
                #include "inc/ops/SET_TUP.inc"
            } break;
            case OPC_SET_VTUP: {
                #include "inc/ops/SET_VTUP.inc"
            } break;
            case OPC_SET_REC: {
                #include "inc/ops/SET_REC.inc"
            } break;
            case OPC_SET_VREC: {
                #include "inc/ops/SET_VREC.inc"
            } break;
            

            case OPC_REC_DEF_ONE: {
                #include "inc/ops/REC_DEF_ONE.inc"
            } break;
            case OPC_REC_DEF_TUP: {
                #include "inc/ops/REC_DEF_TUP.inc"
            } break;
            case OPC_REC_DEF_VTUP: {
                #include "inc/ops/REC_DEF_VTUP.inc"
            } break;
            case OPC_REC_DEF_REC: {
                #include "inc/ops/REC_DEF_REC.inc"
            } break;
            case OPC_REC_DEF_VREC: {
                #include "inc/ops/REC_DEF_VREC.inc"
            } break;
            case OPC_REC_SET_ONE: {
                #include "inc/ops/REC_SET_ONE.inc"
            } break;
            case OPC_REC_SET_TUP: {
                #include "inc/ops/REC_SET_TUP.inc"
            } break;
            case OPC_REC_SET_VTUP: {
                #include "inc/ops/REC_SET_VTUP.inc"
            } break;
            case OPC_REC_SET_REC: {
                #include "inc/ops/REC_SET_REC.inc"
            } break;
            case OPC_REC_SET_VREC: {
                #include "inc/ops/REC_SET_VREC.inc"
            } break;
            
            case OPC_GET_CONST: {
                #include "inc/ops/GET_CONST.inc"
            } break;
            case OPC_GET_CONST0: {
                #include "inc/ops/GET_CONST0.inc"
            } break;
            case OPC_GET_CONST1: {
                #include "inc/ops/GET_CONST1.inc"
            } break;
            case OPC_GET_CONST2: {
                #include "inc/ops/GET_CONST2.inc"
            } break;
            case OPC_GET_CONST3: {
                #include "inc/ops/GET_CONST3.inc"
            } break;
            case OPC_GET_CONST4: {
                #include "inc/ops/GET_CONST4.inc"
            } break;
            case OPC_GET_CONST5: {
                #include "inc/ops/GET_CONST5.inc"
            } break;
            case OPC_GET_CONST6: {
                #include "inc/ops/GET_CONST6.inc"
            } break;
            case OPC_GET_CONST7: {
                #include "inc/ops/GET_CONST7.inc"
            } break;
            
            case OPC_GET_UPVAL: {
                #include "inc/ops/GET_UPVAL.inc"
            } break;
            case OPC_GET_UPVAL0: {
                #include "inc/ops/GET_UPVAL0.inc"
            } break;
            case OPC_GET_UPVAL1: {
                #include "inc/ops/GET_UPVAL1.inc"
            } break;
            case OPC_GET_UPVAL2: {
                #include "inc/ops/GET_UPVAL2.inc"
            } break;
            case OPC_GET_UPVAL3: {
                #include "inc/ops/GET_UPVAL3.inc"
            } break;
            case OPC_GET_UPVAL4: {
                #include "inc/ops/GET_UPVAL4.inc"
            } break;
            case OPC_GET_UPVAL5: {
                #include "inc/ops/GET_UPVAL5.inc"
            } break;
            case OPC_GET_UPVAL6: {
                #include "inc/ops/GET_UPVAL6.inc"
            } break;
            case OPC_GET_UPVAL7: {
                #include "inc/ops/GET_UPVAL7.inc"
            } break;
            
            case OPC_GET_LOCAL: {
                #include "inc/ops/GET_LOCAL.inc"
            } break;
            case OPC_GET_LOCAL0: {
                #include "inc/ops/GET_LOCAL0.inc"
            } break;
            case OPC_GET_LOCAL1: {
                #include "inc/ops/GET_LOCAL1.inc"
            } break;
            case OPC_GET_LOCAL2: {
                #include "inc/ops/GET_LOCAL2.inc"
            } break;
            case OPC_GET_LOCAL3: {
                #include "inc/ops/GET_LOCAL3.inc"
            } break;
            case OPC_GET_LOCAL4: {
                #include "inc/ops/GET_LOCAL4.inc"
            } break;
            case OPC_GET_LOCAL5: {
                #include "inc/ops/GET_LOCAL5.inc"
            } break;
            case OPC_GET_LOCAL6: {
                #include "inc/ops/GET_LOCAL6.inc"
            } break;
            case OPC_GET_LOCAL7: {
                #include "inc/ops/GET_LOCAL7.inc"
            } break;
            
            case OPC_GET_CLOSED: {
                #include "inc/ops/GET_CLOSED.inc"
            } break;
            case OPC_GET_CLOSED0: {
                #include "inc/ops/GET_CLOSED0.inc"
            } break;
            case OPC_GET_CLOSED1: {
                #include "inc/ops/GET_CLOSED1.inc"
            } break;
            case OPC_GET_CLOSED2: {
                #include "inc/ops/GET_CLOSED2.inc"
            } break;
            case OPC_GET_CLOSED3: {
                #include "inc/ops/GET_CLOSED3.inc"
            } break;
            case OPC_GET_CLOSED4: {
                #include "inc/ops/GET_CLOSED4.inc"
            } break;
            case OPC_GET_CLOSED5: {
                #include "inc/ops/GET_CLOSED5.inc"
            } break;
            case OPC_GET_CLOSED6: {
                #include "inc/ops/GET_CLOSED6.inc"
            } break;
            case OPC_GET_CLOSED7: {
                #include "inc/ops/GET_CLOSED7.inc"
            } break;
            
            case OPC_GET_GLOBAL: {
                #include "inc/ops/GET_GLOBAL.inc"
            } break;
            
            case OPC_GET_FIELD: {
                #include "inc/ops/GET_FIELD.inc"
            } break;
            case OPC_GET_FIELDS: {
                #include "inc/ops/GET_FIELDS.inc"
            } break;
            
            case OPC_REF_UPVAL: {
                #include "inc/ops/REF_UPVAL.inc"
            } break;
            case OPC_REF_LOCAL: {
                #include "inc/ops/REF_LOCAL.inc"
            } break;
            case OPC_REF_CLOSED: {
                #include "inc/ops/REF_CLOSED.inc"
            } break;
            case OPC_REF_GLOBAL: {
                #include "inc/ops/REF_GLOBAL.inc"
            } break;
            
            case OPC_LOAD_NIL: {
                #include "inc/ops/LOAD_NIL.inc"
            } break;
            case OPC_LOAD_UDF: {
                #include "inc/ops/LOAD_UDF.inc"
            } break;
            case OPC_LOAD_LOG: {
                #include "inc/ops/LOAD_LOG.inc"
            } break;
            case OPC_LOAD_INT: {
                #include "inc/ops/LOAD_INT.inc"
            } break;
            
            case OPC_MAKE_TUP: {
                #include "inc/ops/MAKE_TUP.inc"
            } break;
            case OPC_MAKE_VTUP: {
                #include "inc/ops/MAKE_VTUP.inc"
            } break;
            case OPC_MAKE_CLS: {
                #include "inc/ops/MAKE_CLS.inc"
            } break;
            case OPC_MAKE_REC: {
                #include "inc/ops/MAKE_REC.inc"
            } break;
            case OPC_MAKE_VREC: {
                #include "inc/ops/MAKE_VREC.inc"
            } break;
            
            case OPC_POP: {
                #include "inc/ops/POP.inc"
            } break;
            case OPC_DUP: {
                #include "inc/ops/DUP.inc"
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
                #include "inc/ops/AND_JUMP.inc"
            } break;
            case OPC_OR_JUMP: {
                #include "inc/ops/OR_JUMP.inc"
            } break;
            case OPC_UDF_JUMP: {
                #include "inc/ops/UDF_JUMP.inc"
            } break;
            case OPC_ALT_JUMP: {
                #include "inc/ops/ALT_JUMP.inc"
            } break;
            case OPC_JUMP: {
                #include "inc/ops/JUMP.inc"
            } break;
            
            case OPC_CALL: {
                #include "inc/ops/CALL.inc"
            } break;
            case OPC_RETURN: {
                #include "inc/ops/RETURN.inc"
            } break;
        }
    }
    
    // Restore old register set.
    *rPtr = regs;
    fib->rPtr = rPtr;
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
        .sz  = sizeof(TVal*)*cap
    };
    fib->tmpStack.tmps = stateResizeRaw( state, &tmpsP, sizeof(TVal)*cap );
    
    stateCommitRaw( state, &tmpsP );
    fib->rPtr->sp  = fib->tmpStack.tmps + oSp;
    fib->rPtr->lcl = fib->tmpStack.tmps + oLcl;
}

static void
freeStack( State* state, Fiber* fib ) {
    memset( fib->rPtr, 0, sizeof(Regs) );
    stateFreeRaw( state, fib->arStack.ars, fib->arStack.cap*sizeof(VirAR) );
    stateFreeRaw( state, fib->tmpStack.tmps, fib->tmpStack.cap*sizeof(TVal) );
    fib->arStack.cap   = 0;
    fib->arStack.ars   = NULL;
    fib->tmpStack.cap  = 0;
    fib->tmpStack.tmps = NULL;
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
