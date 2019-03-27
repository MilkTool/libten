#include "ten_state.h"
#include "ten_assert.h"
#include "ten_macros.h"
#include "ten_api.h"
#include "ten_com.h"
#include "ten_gen.h"
#include "ten_env.h"
#include "ten_fmt.h"
#include "ten_sym.h"
#include "ten_str.h"
#include "ten_idx.h"
#include "ten_rec.h"
#include "ten_fun.h"
#include "ten_cls.h"
#include "ten_fib.h"
#include "ten_upv.h"
#include "ten_dat.h"
#include "ten_ptr.h"

#include <string.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>

void
apiInit( State* state );

void
apiTest( State* state );

static void*
mallocRaw( State* state, size_t nsz );

static void*
reallocRaw( State* state, void* old, size_t osz, size_t nsz );

static void
freeRaw( State* state, void* old, size_t osz );

static void
collect( State* state, size_t extra );

static void
onError( State* state );

#ifdef ten_DEBUG
    static void
    initDefer( State* state, Defer* defer, char const* file, uint line );

    static void
    initObjPart( State* state, Part* part, char const* file, uint line );

    static void
    initRawPart( State* state, Part* part, char const* file, uint line );

    static void
    checkDefer( State* state, Defer* defer, char const* file, uint line );

    static void
    checkObjPart( State* state, Part* part, char const* file, uint line );

    static void
    checkRawPart( State* state, Part* part, char const* file, uint line );
    
    static void
    clearPart( State* state, Part* part );

    static void
    clearDefer( State* state, Defer* defer );
    
#endif

static void
freeParts( State* state );

static void
destructObj( State* state, Object* obj );

static void
freeObj( State* state, Object* obj );

static void*
frealloc( void* _, void* old, size_t osz, size_t nsz ) {
    if( nsz > 0 )
        return realloc( old, nsz );
    free( old );
    return NULL;
}

void
stateInit( State* state, ten_Config const* config, jmp_buf* errJmp ) {
    memset( state, 0, sizeof(State) );
    
    state->config   = *config;
    state->errVal   = tvUdf();
    state->errJmp   = errJmp;
    state->memLimit = MEM_LIMIT_INIT;
    
    state->tmpNext = 0;
    state->tmpBase = state->tmpVals;
    state->tmpTup = (Tup){
        .base   = &state->tmpBase,
        .offset = 0,
        .size   = NUM_TMP_VARS
    };
    for( uint i = 0 ; i < NUM_TMP_VARS ; i++ ) {
        state->tmpVals[i] = tvUdf();
        state->tmpVars[i] = (ten_Var){
            .tup = (ten_Tup*)&state->tmpTup,
            .loc = i
        };
    }
    
    
    if( state->config.frealloc == NULL )
        state->config.frealloc = frealloc;
    if( state->config.memGrowth == 0.0 )
        state->config.memGrowth = MEM_LIMIT_GROWTH;
    
    fmtInit( state ); CHECK_STATE;
    genInit( state ); CHECK_STATE;
    comInit( state ); CHECK_STATE;
    envInit( state ); CHECK_STATE;
    symInit( state ); CHECK_STATE;
    strInit( state ); CHECK_STATE;
    idxInit( state ); CHECK_STATE;
    recInit( state ); CHECK_STATE;
    funInit( state ); CHECK_STATE;
    clsInit( state ); CHECK_STATE;
    fibInit( state ); CHECK_STATE;
    upvInit( state ); CHECK_STATE;
    datInit( state ); CHECK_STATE;
    ptrInit( state ); CHECK_STATE;
    apiInit( state ); CHECK_STATE;
}

#ifdef ten_TEST
    typedef struct Test Test;
    struct Test {
        Test* next;
    };
    #define tstTrav( STATE, TST ) \
        if( (TST)->next ) stateMark( (STATE), (TST)->next );
    #define tstDest( STATE, TST )
    #define tstSize( STATE, TST ) sizeof(Test)
    
    Test*
    tstNew( State* state, Test* next ) {
        Part  testP;
        Test* test = stateAllocObj( state, &testP, sizeof(Test), OBJ_TST );
        test->next = next;
        stateCommitObj( state, &testP );
        return test;
    }
    
    typedef struct {
        Defer base;
        bool  called;
    } TestDefer;
    
    void
    testDeferCb( State* state, Defer* d ) {
        TestDefer* td = (TestDefer*)d;
        td->called = true;
    }
    
    typedef struct TestAlloc {
        struct TestAlloc*  next;
        struct TestAlloc** link;
        size_t size;
        char   mem[];
    } TestAlloc;
    
    TestAlloc* allocs = NULL;
    
    /*
    size_t
    allocated( void ) {
        size_t allocated = 0;
        
        TestAlloc* aIt = allocs;
        while( aIt ) {
            allocated += aIt->size;
            aIt = aIt->next;
        }
        
        return allocated;
    }
    */
    
    void*
    testFreealloc( void* udat, void* old, size_t osz, size_t nsz ) {
        //tenAssert( allocated() == ((State*)udat)->memUsed );
        tenAssert( old != NULL || osz == 0 );
        
        TestAlloc* oAlloc = NULL;
        if( old ) {
            oAlloc = old - sizeof(TestAlloc);
            tenAssert( oAlloc->size == osz );
            remNode( oAlloc );
        }
        
        if( nsz == 0 ) {
            free( oAlloc );
            return NULL;
        }
        
        TestAlloc* nAlloc = realloc( oAlloc, sizeof(TestAlloc) + nsz );
        addNode( &allocs, nAlloc );
        nAlloc->size = nsz;
        return nAlloc->mem;
    }
    
    void
    stateTest( void ) {
        State  sbuf;
        State* state = &sbuf;
        
        ten_Config config = { .frealloc = testFreealloc, .udata = &sbuf };
        
        bool fmtPassed = false;
        jmp_buf errJmp;
        int err = setjmp( errJmp );
        if( err ) {
            if( state->errStr ) {
                fprintf( stderr, "Error: %s\n", state->errStr );
            }
            else {
                if( fmtPassed ) {
                    fmtA( state, false, "Error: %v\n", state->errVal );
                    fputs( fmtBuf( state ), stderr );
                }
                else {
                    fputs( "Error: <can't print>\n", stderr );
                }
            }
            
            ten_Trace* tIt = state->trace;
            while( tIt ) {
                fprintf( stderr, "%s: %u\n", tIt->file, tIt->line );
                tIt = tIt->next;
            }
            
            stateFinl( state );
            
            exit( err );
        }
        stateInit( state, &config, &errJmp );
        CHECK_STATE;
        
        // Allocate some objects to test object allocation and GC.
        state->test = NULL;
        for( uint i = 0 ; i < 10000 ; i++ )
            state->test = tstNew( state, state->test );
        
        Part objP;
        stateAllocObj( state, &objP, sizeof(Test), OBJ_TST );
        stateCancelObj( state, &objP );
        CHECK_STATE;
        
        // Allocate and free some raw data chunks to test allocation.
        Part rawP;
        uint* raw = stateAllocRaw( state, &rawP, sizeof(uint)*100 );
        for( uint i = 0 ; i < 100 ; i++ )
            raw[i] = 0xFFFFFFFF;
        stateCommitRaw( state, &rawP );
        stateFreeRaw( state, raw, sizeof(uint)*100 );
        CHECK_STATE;
        
        stateAllocRaw( state, &rawP, sizeof(uint)*100 );
        stateCancelRaw( state, &rawP );
        CHECK_STATE;
        
        // Test error and defer handling.
        jmp_buf  newJmp;
        jmp_buf* oldJmp = stateSwapErrJmp( state, &newJmp );
        int sig = setjmp( newJmp );
        if( sig ) {
            tenAssert( !strcmp( state->errStr, "Dummy Error" ) );
            tenAssert( sig == ten_ERR_TEST );
            stateSwapErrJmp( state, oldJmp );
            stateClearError( state );
            goto afterErrorTest;
        }
        
        TestDefer defer1 = { .base = { .cb = testDeferCb }, .called = false };
        TestDefer defer2 = { .base = { .cb = testDeferCb }, .called = false };
        TestDefer defer3 = { .base = { .cb = testDeferCb }, .called = false };
        stateInstallDefer( state, (Defer*)&defer1 );
        stateInstallDefer( state, (Defer*)&defer2 );
        stateInstallDefer( state, (Defer*)&defer3 );
        CHECK_STATE;
        
        stateCommitDefer( state, (Defer*)&defer1 );
        tenAssert( defer1.called == true );
        CHECK_STATE;
        
        stateCancelDefer( state, (Defer*)&defer2 );
        tenAssert( defer2.called == false );
        CHECK_STATE;
        
        stateErrStr( state, ten_ERR_TEST, "Dummy Error" );
        tenAssert( false );
        {afterErrorTest:
            tenAssert( defer3.called == true );
            tenAssert( defer2.called == false );
            CHECK_STATE;
        }
        
        // Test trace creation and detachment.
        statePushTrace( state, "file1", 1 );
        statePushTrace( state, "file2", 2 );
        statePushTrace( state, "file3", 3 );
        CHECK_STATE;
        
        ten_Trace* trace = stateClaimTrace( state );
        stateClearTrace( state );
        CHECK_STATE;
        
        ten_Trace* trace3 = trace;
        ten_Trace* trace2 = trace3->next;
        ten_Trace* trace1 = trace2->next;
        tenAssert( trace1->next == NULL );
        tenAssert( !strcmp( trace1->file, "file1" ) );
        tenAssert( !strcmp( trace2->file, "file2" ) );
        tenAssert( !strcmp( trace3->file, "file3" ) );
        tenAssert( trace1->line == 1 );
        tenAssert( trace2->line == 2 );
        tenAssert( trace3->line == 3 );
        
        stateFreeTrace( state, trace );
        CHECK_STATE;
        
        
        // Run component tests.
        fmtTest( state ); CHECK_STATE; fmtPassed = true;
        genTest( state ); CHECK_STATE;
        comTest( state ); CHECK_STATE;
        envTest( state ); CHECK_STATE;
        symTest( state ); CHECK_STATE;
        strTest( state ); CHECK_STATE;
        idxTest( state ); CHECK_STATE;
        recTest( state ); CHECK_STATE;
        funTest( state ); CHECK_STATE;
        clsTest( state ); CHECK_STATE;
        fibTest( state ); CHECK_STATE;
        upvTest( state ); CHECK_STATE;
        datTest( state ); CHECK_STATE;
        ptrTest( state ); CHECK_STATE;
        apiTest( state ); CHECK_STATE;
        
        // Force GC.
        stateCollect( state );
        
        // Test stack operations after everything else
        // as they rely on the env and fib components.
        Tup tup;
        statePush( state, 3 );
        
        tup = statePush( state, 5 );
        tenAssert( tup.size == 5 );
        
        tup = stateTop( state );
        tenAssert( tup.size == 5 );
        
        statePop( state );
        tenAssert( stateTop( state ).size == 3 );
        
        stateFinl( state );
    }
#endif

void
stateFinl( State* state ) {
    // Destruct and free all objects.
    Object* oIt;
    
    oIt = state->objects;
    while( oIt ) {
        Object* obj = oIt;
        oIt = tpGetPtr( oIt->next );
        
        destructObj( state, obj );
    }
    oIt = state->objects;
    while( oIt ) {
        Object* obj = oIt;
        oIt = tpGetPtr( oIt->next );
        
        freeObj( state, obj );
    }
    state->objects = NULL;
    
    // Free all the incomplete parts.
    freeParts( state );
    
    // Invoke all the finalizers.
    Finalizer* fIt = state->finalizers;
    while( fIt ) {
        Finalizer* finl = fIt;
        fIt = fIt->next;
        
        finl->cb( state, finl );
    }
    state->finalizers = NULL;
}

Tup
statePush( State* state, uint n ) {
    if( state->fiber )
        return fibPush( state, state->fiber, n );
    else
        return envPush( state, n );
}

Tup
stateTop( State* state ) {
    if( state->fiber )
        return fibTop( state, state->fiber );
    else
        return envTop( state );
}

void
statePop( State* state ) {
    if( state->fiber )
        return fibPop( state, state->fiber );
    else
        return envPop( state );
}

ten_Var*
stateTmp( State* state ) {
    return &state->tmpVars[state->tmpNext++ % NUM_TMP_VARS ];
}

void
stateErrStr( State* state, ten_ErrNum err, char const* str ) {
    state->errNum = err;
    state->errStr = str;
    state->errVal = tvUdf();
    onError( state );
}

void
stateErrFmtA( State* state, ten_ErrNum err, char const* fmt, ... ) {
    va_list ap;
    va_start( ap, fmt );
    fmtV( state, false, fmt, ap );
    va_end( ap );
    
    state->errNum = err;
    state->errVal = tvObj( strNew( state, fmtBuf( state ), fmtLen( state ) ) );
    state->errStr = NULL;
    onError( state );
}

void
stateErrFmtV( State* state, ten_ErrNum err, char const* fmt, va_list ap ) {
    fmtV( state, false, fmt, ap );
    
    state->errNum = err;
    state->errVal = tvObj( strNew( state, fmtBuf( state ), fmtLen( state ) ) );
    state->errStr = NULL;
    onError( state );
}

void
stateErrVal( State* state, ten_ErrNum err, TVal val ) {
    state->errNum = err;
    state->errVal = val;
    state->errStr = NULL;
    onError( state );
}

void
stateErrProp( State* state ) {
    onError( state );
}


jmp_buf*
stateSwapErrJmp( State* state, jmp_buf* jmp ) {
    jmp_buf* old = state->errJmp;
    state->errJmp = jmp;
    return old;
}

#ifdef ten_DEBUG
void*
_stateAllocObj( State* state, Part* p, size_t sz, ObjTag tag, char const* file, uint line ) {
    initObjPart( state, p, file, line );
#else
void*
stateAllocObj( State* state, Part* p, size_t sz, ObjTag tag ) {
#endif

    Object* obj = mallocRaw( state, sizeof(Object) + sz );
    obj->next = tpMake( tag << OBJ_TAG_SHIFT, NULL );
    p->ptr = objGetDat( obj );
    p->sz  = sz;
    
    addNode( &state->objParts, p );
    return p->ptr;
}

void
stateCommitObj( State* state, Part* p ) {
    #ifdef ten_DEBUG
        checkObjPart( state, p, __FILE__, __LINE__ );
        clearPart( state, p );
    #endif
    
    Object* obj = datGetObj( p->ptr );
    obj->next = tpMake( tpGetTag( obj->next ), state->objects );
    state->objects = obj;
    
    remNode( p );
}

void
stateCancelObj( State* state, Part* p ) {
    #ifdef ten_DEBUG
        checkObjPart( state, p, __FILE__, __LINE__ );
        clearPart( state, p );
    #endif
    
    remNode( p );
    
    Object* obj = datGetObj( p->ptr );
    freeRaw( state, obj, sizeof(Object) + p->sz );
}

#ifdef ten_DEBUG
void*
_stateAllocRaw( State* state, Part* p, size_t sz, char const* file, uint line ) {
    initRawPart( state, p, file, line );
#else
void*
stateAllocRaw( State* state, Part* p, size_t sz ) {
#endif

   void* raw = mallocRaw( state, sz );
    p->ptr = raw;
    p->sz  = sz;
    
    addNode( &state->rawParts, p );
    return p->ptr;
}

#ifdef ten_DEBUG
void*
_stateResizeRaw( State* state, Part* p, size_t sz, char const* file, uint line ) {
    initRawPart( state, p, file, line );
#else
void*
stateResizeRaw( State* state, Part* p, size_t sz ) {
#endif
    void* raw = reallocRaw( state, p->ptr, p->sz, sz );
    p->ptr = raw;
    p->sz  = sz;
    
    if( p->link )
        remNode( p );
    
    addNode( &state->rawParts, p );
    return p->ptr;
}

void
stateCommitRaw( State* state, Part* p ) {
    #ifdef ten_DEBUG
        checkRawPart( state, p, __FILE__, __LINE__ );
        clearPart( state, p );
    #endif
    remNode( p );
}

void
stateCancelRaw( State* state, Part* p ) {
    #ifdef ten_DEBUG
        checkRawPart( state, p, __FILE__, __LINE__ );
        clearPart( state, p );
    #endif
    remNode( p );
    freeRaw( state, p->ptr, p->sz );
}

void
stateFreeRaw( State* state, void* old, size_t osz ) {
    freeRaw( state, old, osz );
}

#ifdef ten_DEBUG
void
_stateInstallDefer( State* state, Defer* defer, char const* file, uint line ) {
    initDefer( state, defer, file, line );
#else
void
stateInstallDefer( State* state, Defer* defer ) {
#endif

    addNode( &state->defers, defer );
}

void
stateCommitDefer( State* state, Defer* defer ) {
    #ifdef ten_DEBUG
        checkDefer( state, defer, __FILE__, __LINE__ );
        clearDefer( state, defer );
    #endif
    
    remNode( defer );
    defer->cb( state, defer );
}

void
stateCancelDefer( State* state, Defer* defer ) {
    #ifdef ten_DEBUG
        checkDefer( state, defer, __FILE__, __LINE__ );
        clearDefer( state, defer );
    #endif
    
    remNode( defer );
}


void
stateInstallScanner( State* state, Scanner* scanner ) {
    addNode( &state->scanners, scanner );
}

void
stateRemoveScanner( State* state, Scanner* scanner ) {
    remNode( scanner );
}

void
stateInstallFinalizer( State* state, Finalizer* finalizer ) {
    addNode( &state->finalizers, finalizer );
}

void
stateRemoveFinalizer( State* state, Finalizer* finalizer ) {
    remNode( finalizer );
}

void
statePushTrace( State* state, char const* file, uint line ) {
    Part traceP;
    ten_Trace* trace = stateAllocRaw( state, &traceP, sizeof(ten_Trace) );
    
    size_t fileLen = strlen( file );
    Part   fileP;
    char* fileCpy = stateAllocRaw( state, &fileP, fileLen + 1 );
    strcpy( fileCpy, file );
    
    trace->file = fileCpy;
    trace->line = line;
    trace->next = state->trace;
    state->trace = trace;
    
    stateCommitRaw( state, &traceP );
    stateCommitRaw( state, &fileP );
}

ten_Trace*
stateClaimTrace( State* state ) {
    ten_Trace* trace = state->trace;
    state->trace = NULL;
    return trace;
}

void
stateClearTrace( State* state ) {
    stateFreeTrace( state, state->trace );
    state->trace = NULL;
}


void
stateFreeTrace( State* state, ten_Trace* trace ) {
    ten_Trace* tIt = trace;
    while( tIt ) {
        ten_Trace* t = tIt;
        tIt = tIt->next;
        
        size_t fileLen = strlen( t->file );
        stateFreeRaw( state, (char*)t->file, fileLen + 1 );
        stateFreeRaw( state, t, sizeof(ten_Trace) );
    }
}

void
stateClearError( State* state ) {
    state->errNum = ten_ERR_NONE;
    state->errStr = NULL;
    state->errVal = tvUdf();
    stateClearTrace( state );
}

void
stateMark( State* state, void* ptr ) {
    Object* obj  = datGetObj( ptr );
    uint    tag  = tpGetTag( obj->next );
    void*   next = tpGetPtr( obj->next );
    
    // If the mark bit is already set then the object
    // has already been marked and traversed, so do
    // nothing.
    if( tag & OBJ_MARK_BIT )
        return;
    
    // Set the object's mark bit.
    obj->next = tpMake( tag | OBJ_MARK_BIT, next );
    
    switch( ( tag & OBJ_TAG_BITS ) >> OBJ_TAG_SHIFT ) {
        case OBJ_STR: strTrav( state, (String*)ptr );    break;
        case OBJ_IDX: idxTrav( state, (Index*)ptr );     break;
        case OBJ_REC: recTrav( state, (Record*)ptr );    break;
        case OBJ_FUN: funTrav( state, (Function*)ptr );  break;
        case OBJ_CLS: clsTrav( state, (Closure*)ptr );   break;
        case OBJ_FIB: fibTrav( state, (Fiber*)ptr );     break;
        case OBJ_UPV: upvTrav( state, (Upvalue*)ptr );   break;
        case OBJ_DAT: datTrav( state, (Data*)ptr );      break;
        #ifdef ten_TEST
            case OBJ_TST: tstTrav( state, (Test*)ptr );  break;
        #endif
        default: tenAssertNeverReached();                break;
    }
}

void
stateCollect( State* state ) {
    collect( state, 0 );
}

static void*
mallocRaw( State* state, size_t nsz ) {
    return reallocRaw( state, NULL, 0, nsz );
}

static void*
reallocRaw( State* state, void* old, size_t osz, size_t nsz ) {
    size_t need = state->memUsed + nsz;
    if( need > state->memLimit )
        collect( state, nsz );
    
    void* mem = state->config.frealloc( state->config.udata, old, osz, nsz );
    if( nsz > 0 && !mem )
        stateErrStr( state, ten_ERR_MEMORY, "Allocation failed" );
    
    state->memUsed += nsz;
    state->memUsed -= osz;
    
    return mem;
}

static void
freeRaw( State* state, void* old, size_t osz ) {
    tenAssert( state->memUsed >= osz );
    state->config.frealloc( state->config.udata, old, osz, 0 );
    state->memUsed -= osz;
}



static void
destructObj( State* state, Object* obj ) {
    void*  ptr = objGetDat( obj );
    uint   tag = tpGetTag( obj->next );
    switch( ( tag & OBJ_TAG_BITS ) >> OBJ_TAG_SHIFT ) {
        case OBJ_STR: strDest( state, (String*)ptr );       break;
        case OBJ_IDX: idxDest( state, (Index*)ptr );        break;
        case OBJ_REC: recDest( state, (Record*)ptr );       break;
        case OBJ_FUN: funDest( state, (Function*)ptr );     break;
        case OBJ_CLS: clsDest( state, (Closure*)ptr );      break;
        case OBJ_FIB: fibDest( state, (Fiber*)ptr );        break;
        case OBJ_UPV: upvDest( state, (Upvalue*)ptr );      break;
        case OBJ_DAT: datDest( state, (Data*)ptr );         break;
        #ifdef ten_TEST
            case OBJ_TST: /* NADA */                        break;
        #endif
        default: tenAssertNeverReached();                   break;
    }
    obj->next = tpMake( tag | OBJ_DEAD_BIT, tpGetPtr( obj->next ) );
    tenAssert( datIsDead( ptr ) );
}

static void
freeObj( State* state, Object* obj ) {
    void*  ptr = objGetDat( obj );
    uint   tag = tpGetTag( obj->next );
    size_t sz;
    switch( ( tag & OBJ_TAG_BITS ) >> OBJ_TAG_SHIFT ) {
        case OBJ_STR: sz = strSize( state, (String*)ptr );   break;
        case OBJ_IDX: sz = idxSize( state, (Index*)ptr );    break;
        case OBJ_REC: sz = recSize( state, (Record*)ptr );   break;
        case OBJ_FUN: sz = funSize( state, (Function*)ptr ); break;
        case OBJ_CLS: sz = clsSize( state, (Closure*)ptr );  break;
        case OBJ_FIB: sz = fibSize( state, (Fiber*)ptr );    break;
        case OBJ_UPV: sz = upvSize( state, (Upvalue*)ptr );  break;
        case OBJ_DAT: sz = datSize( state, (Data*)ptr );     break;
        #ifdef ten_TEST
            case OBJ_TST: sz = tstSize( state, (Test*)ptr ); break;
        #endif
        default: tenAssertNeverReached();                    break;
    }
    freeRaw( state, obj, sizeof(Object) + sz );
}

static void
adjustMemLimit( State* state, size_t extra ) {
    tenAssert( state->config.memGrowth >  1.0 );
    tenAssert( state->config.memGrowth <= 2.0 );
    
    double mul = state->config.memGrowth + 1.0;
    state->memLimit = (double)(state->memUsed + extra) * mul;
}

static void
collect( State* state, size_t extra ) {
    CHECK_STATE;
    
    // Every 5th cycle we do a full traversal to
    // scan for Pointers and Symbols as well as
    // normal objects.
    if( state->gcCount++ % 5 == 0 ) {
        state->gcFull = true;
        symStartCycle( state );
        ptrStartCycle( state );
    }
    
    // Run all the scanners.
    Scanner* sIt = state->scanners;
    while( sIt ) {
        sIt->cb( state, sIt );
        sIt = sIt->next;
    }
    
    // Mark the State owned objects.
    if( state->fiber )
        stateMark( state, state->fiber );
    for( uint i = 0 ; i < NUM_TMP_VARS ; i++ )
        tvMark( state->tmpVals[i] );
    
    tvMark( state->errVal );
    
    #ifdef ten_TEST
        if( state->test )
            stateMark( state, state->test );
    #endif
    
    // By now we've finished scanning for references,
    // so divide the objects into two lists, marked
    // and garbage; as we add items to the `marked`
    // list clear the mark bit so we don't have to
    // perform an extra iteration.
    Object* marked  = NULL;
    Object* garbage = NULL;
    
    Object* oIt = state->objects;
    while( oIt ) {
        Object* obj  = oIt;
        int     tag  = tpGetTag( oIt->next );
        oIt = tpGetPtr( oIt->next );
        
        if( tag & OBJ_MARK_BIT ) {
            obj->next = tpMake( tag & ~OBJ_MARK_BIT, marked );
            marked = obj;
        }
        else {
            destructObj( state, obj );
            obj->next = tpMake( tpGetTag( obj->next ), garbage );
            garbage = obj;
        }
    }
    
    // Free the unmarked objects, this has to be done
    // separately from destruction since some destruction
    // routines depend on the variables of other objects.
    oIt = garbage;
    while( oIt ) {
        Object* obj = oIt;
        oIt = tpGetPtr( obj->next );
        freeObj( state, obj );
    }
    
    // Use the marked list as the new objects list.
    state->objects = marked;
    
    // Adjust the heap limit.
    adjustMemLimit( state, extra );
    
    // Tell the Symbol and Pointer components that we're
    // done collecting.
    if( state->gcFull ) {
        state->gcFull = false;
        symFinishCycle( state );
        ptrFinishCycle( state );
    }
}

static void
onError( State* state ) {
    CHECK_STATE;
    
    Defer* dIt = state->defers;
    while( dIt ) {
        dIt->cb( state, dIt );
        dIt = dIt->next;
    }
    
    state->defers = NULL;
    freeParts( state );
    
    longjmp( *state->errJmp, state->errNum );
}

#ifdef ten_DEBUG
    static void
    initDefer( State* state, Defer* defer, char const* file, uint line ) {
        #ifdef ten_DEBUG
            defer->beginNum = DEFER_BEGIN_NUM;
            defer->file     = file;
            defer->line     = line;
            defer->endNum   = DEFER_END_NUM;
        #endif
    }

    static void
    initObjPart( State* state, Part* part, char const* file, uint line ) {
        #ifdef ten_DEBUG
            part->beginNum = OBJ_PART_BEGIN_NUM;
            part->file     = file;
            part->line     = line;
            part->endNum   = OBJ_PART_END_NUM;
        #endif
    }

    static void
    initRawPart( State* state, Part* part, char const* file, uint line ) {
        #ifdef ten_DEBUG
            part->beginNum = RAW_PART_BEGIN_NUM;
            part->file     = file;
            part->line     = line;
            part->endNum   = RAW_PART_END_NUM;
        #endif
    }

    static void
    checkDefer( State* state, Defer* defer, char const* file, uint line ) {
        #ifdef ten_DEBUG
            if(
                defer->beginNum != DEFER_BEGIN_NUM ||
                defer->endNum   != DEFER_END_NUM
            ) {
                fprintf( stderr, "%s:%u: Defer check failed\n", file, line );
                abort();
            }
        #endif
    }

    static void
    checkObjPart( State* state, Part* part, char const* file, uint line ) {
        #ifdef ten_DEBUG
            if(
                part->beginNum != OBJ_PART_BEGIN_NUM ||
                part->endNum   != OBJ_PART_END_NUM
            ) {
                fprintf( stderr, "%s:%u: ObjPart check failed\n", file, line );
                abort();
            }
        #endif
    }

    static void
    checkRawPart( State* state, Part* part, char const* file, uint line ) {
        #ifdef ten_DEBUG
            if(
                part->beginNum != RAW_PART_BEGIN_NUM ||
                part->endNum   != RAW_PART_END_NUM
            ) {
                fprintf( stderr, "%s:%u: RawPart check failed\n", file, line );
                abort();
            }
        #endif
    }


    static void
    clearPart( State* state, Part* part ) {
        #ifdef ten_DEBUG
            part->beginNum = 0;
            part->endNum   = 0;
        #endif
    }

    static void
    clearDefer( State* state, Defer* defer ) {
        #ifdef ten_DEBUG
            defer->beginNum = 0;
            defer->endNum   = 0;
        #endif
    }
    
    void
    stateCheckState( State* state, char const* file, uint line ) {
        
        // Empty array to try and zero out the call frame
        // of the previous few calls, to help catch missing
        // commits.
        uint dummy[100] = { 0 };
        
        Part* pIt;
        
        pIt = state->objParts;
        while( pIt ) {
            checkObjPart( state, pIt, file, line );
            pIt = pIt->next;
        }
        
        pIt = state->rawParts;
        while( pIt ) {
            checkRawPart( state, pIt, file, line );
            pIt = pIt->next;
        }
        
        Defer* dIt = state->defers;
        while( dIt ) {
            checkDefer( state, dIt, file, line );
            dIt = dIt->next;
        }
    }
#endif

static void
freeParts( State* state ) {
    Part* pIt;
    
    pIt = state->objParts;
    while( pIt ) {
        checkObjPart( state, pIt, __FILE__, __LINE__ );
        stateFreeRaw( state, pIt->ptr, sizeof(Object) + pIt->sz );
        pIt = pIt->next;
    }
    state->objParts = NULL;
    
    pIt = state->rawParts;
    while( pIt ) {
        checkRawPart( state, pIt, __FILE__, __LINE__ );
        stateFreeRaw( state, pIt->ptr, pIt->sz );
        pIt = pIt->next;
    }
    state->rawParts = NULL;
}
