#include "ten_state.h"
#include "ten_assert.h"
#include "ten_macros.h"
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

// Temporary prototypes.
void
apiInit( State* state );
void
fmtInit( State* state );
void
comInit( State* state );

static void*
mallocRaw( State* state, size_t nsz );

static void*
reallocRaw( State* state, void* old, size_t osz, size_t nsz );

static void
freeRaw( State* state, void* old, size_t osz );

static void
collect( State* state, size_t extra );

void
stateInit( State* state, ten_Config const* config, jmp_buf* errJmp ) {
    memset( state, 0, sizeof(*state) );
    
    state->config   = *config;
    state->errVal   = tvUdf();
    state->memLimit = MEM_LIMIT_INIT;
    
    apiInit( state );
    comInit( state );
    genInit( state );
    envInit( state );
    fmtInit( state );
    symInit( state );
    strInit( state );
    idxInit( state );
    recInit( state );
    funInit( state );
    clsInit( state );
    fibInit( state );
    upvInit( state );
    datInit( state );
    ptrInit( state );
}

void
stateFinl( State* state ) {
    // TODO
}

ten_Tup
statePush( State* state, uint n ) {
    // TODO
    return (ten_Tup){ 0 };
}

ten_Tup
stateTop( State* state ) {
    // TODO
    return (ten_Tup){ 0 };
}

void
statePop( State* state ) {
    // TODO
}

void
stateErrStr( State* state, ten_ErrNum err, char const* str ) {
    // TODO
}

void
stateErrFmt( State* state, ten_ErrNum err, char const* fmt, ... ) {
    // TODO
}

void
stateErrProp( State* state ) {
    // TODO
}

void
stateErrVal( State* state, ten_ErrNum err, TVal val ) {
    // TODO
}

jmp_buf*
stateSwapErrJmp( State* state, jmp_buf* jmp ) {
    jmp_buf* old = state->errJmp;
    state->errJmp = jmp;
    return old;
}

void*
stateAllocObj( State* state, Part* p, size_t sz, ObjTag tag ) {
    Object* obj = mallocRaw( state, sizeof(Object) + sz );
    obj->next = tpMake( tag, NULL );
    p->ptr = objGetDat( obj );
    p->sz  = sz;
    
    addNode( &state->objParts, p );
    return p->ptr;
}

void
stateCommitObj( State* state, Part* p ) {
    Object* obj = datGetObj( p->ptr );
    obj->next = tpMake( tpGetTag( obj->next ), state->objects );
    state->objects = obj;
    
    remNode( p );
}

void
stateCancelObj( State* state, Part* p ) {
    remNode( p );
    
    Object* obj = datGetObj( p->ptr );
    freeRaw( state, obj, sizeof(Object) + p->sz );
}

void*
stateAllocRaw( State* state, Part* p, size_t sz ) {
   void* raw = mallocRaw( state, sz );
    p->ptr = raw;
    p->sz  = sz;
    
    addNode( &state->rawParts, p );
    return p->ptr;
}

void*
stateResizeRaw( State* state, Part* p, size_t sz ) {
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
    remNode( p );
}

void
stateCancelRaw( State* state, Part* p ) {
    remNode( p );
    freeRaw( state, p->ptr, p->sz );
}

void
stateFreeRaw( State* state, void* old, size_t osz ) {
    freeRaw( state, old, osz );
}

void
stateInstallDefer( State* state, Defer* defer ) {
    addNode( &state->defers, defer );
}

void
stateCommitDefer( State* state, Defer* defer ) {
    remNode( defer );
    defer->cb( state, defer );
}

void
stateCancelDefer( State* state, Defer* defer ) {
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
    // TODO
}

void
stateClearTrace( State* state ) {
    // TODO
}

void
stateClearError( State* state ) {
    // TODO
}

void
stateMark( State* state, void* ptr ) {
    Object* obj  = datGetObj( ptr );
    int     tag  = tpGetTag( obj->next );
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
    void* ptr = objGetDat( obj );
    switch( tpGetTag( obj->next ) ) {
        case OBJ_STR: strDest( state, (String*)ptr );       break;
        case OBJ_IDX: idxDest( state, (Index*)ptr );        break;
        case OBJ_REC: recDest( state, (Record*)ptr );       break;
        case OBJ_FUN: funDest( state, (Function*)ptr );     break;
        case OBJ_CLS: clsDest( state, (Closure*)ptr );      break;
        case OBJ_FIB: fibDest( state, (Fiber*)ptr );        break;
        case OBJ_UPV: upvDest( state, (Upvalue*)ptr );      break;
        case OBJ_DAT: datDest( state, (Data*)ptr );         break;
        default: tenAssertNeverReached();                   break;
    }
}

static void
freeObj( State* state, Object* obj ) {
    void* ptr = objGetDat( obj );
    size_t sz;
    switch( tpGetTag( obj->next ) ) {
        case OBJ_STR: sz = strSize( state, (String*)ptr );   break;
        case OBJ_IDX: sz = idxSize( state, (Index*)ptr );    break;
        case OBJ_REC: sz = recSize( state, (Record*)ptr );   break;
        case OBJ_FUN: sz = funSize( state, (Function*)ptr ); break;
        case OBJ_CLS: sz = clsSize( state, (Closure*)ptr );  break;
        case OBJ_FIB: sz = fibSize( state, (Fiber*)ptr );    break;
        case OBJ_UPV: sz = upvSize( state, (Upvalue*)ptr );  break;
        case OBJ_DAT: sz = datSize( state, (Data*)ptr );     break;
        default: tenAssertNeverReached();                    break;
    }
    freeRaw( state, obj, sizeof(Object) + sz );
}

static void
adjustMemLimit( State* state, size_t extra ) {
    tenAssert( state->config.memLimitGrowth >  1.0 );
    tenAssert( state->config.memLimitGrowth <= 2.0 );
    
    double mul = state->config.memLimitGrowth + 1.0;
    state->memLimit = (double)state->memUsed * mul;
}

static void
collect( State* state, size_t extra ) {
    
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
    stateMark( state, state->fiber );
    tvMark( state->errVal );
    
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
            obj->next = tpMake( tag, garbage );
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
