#include "ten.h"
#include "ten_state.h"
#include "ten_assert.h"
#include "ten_macros.h"
#include "ten_math.h"
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
#include "ten_lib.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

ten_Version const ten_VERSION = {
    .major = 0,
    .minor = 4,
    .patch = 0
};


struct ApiState {
    Scanner   scan;
    Finalizer finl;
    
    TVal val1;
    TVal val2;
    
    Tup     typeTup;
    TVal*   typeBase;
    ten_Var typeVars[OBJ_LAST];
    TVal    typeVals[OBJ_LAST];
    
    Fiber* fib;
};

void
apiFinl( State* state, Finalizer* finl ) {
    ApiState* api = structFromFinl( ApiState, finl );
    stateRemoveScanner( state, &api->scan );
    stateFreeRaw( state, api, sizeof(ApiState) );
}

void
apiScan( State* state, Scanner* scan ) {
    ApiState* api = structFromScan( ApiState, scan );
    
    tvMark( api->val1 );
    tvMark( api->val2 );
    if( api->fib )
        stateMark( state, api->fib );
    
    if( !state->gcFull )
        return;
    
    for( uint i = 0 ; i < OBJ_LAST ; i++ )
        tvMark( api->typeVals[i] );
}

void
apiInit( State* state ) {
    Part apiP;
    ApiState* api = stateAllocRaw( state, &apiP, sizeof(ApiState) );
    api->val1 = tvUdf();
    api->val2 = tvUdf();
    api->fib  = NULL;
    for( uint i = 0 ; i < OBJ_LAST ; i++ ) {
        api->typeVals[i] = tvUdf();
        api->typeVars[i] = (ten_Var){ (ten_Tup*)&api->typeTup, .loc = i };
    }
    
    api->finl.cb = apiFinl; stateInstallFinalizer( state, &api->finl );
    api->scan.cb = apiScan; stateInstallScanner( state, &api->scan );
    
    #define TYPE( I, N ) \
        api->typeVals[I] = tvSym( symGet( state, #N, sizeof(#N)-1 ) )
    
    TYPE( VAL_SYM, Sym );
    TYPE( VAL_PTR, Ptr );
    TYPE( VAL_UDF, Udf );
    TYPE( VAL_NIL, Nil );
    TYPE( VAL_LOG, Log );
    TYPE( VAL_INT, Int );
    TYPE( VAL_DEC, Dec );
    TYPE( OBJ_STR, Str );
    TYPE( OBJ_IDX, Idx );
    TYPE( OBJ_REC, Rec );
    TYPE( OBJ_FUN, Fun );
    TYPE( OBJ_CLS, Cls );
    TYPE( OBJ_FIB, Fib );
    TYPE( OBJ_DAT, Dat );
    
    api->typeTup  = (Tup){ .base = &api->typeBase, .offset = 0, .size = OBJ_LAST };
    api->typeBase = api->typeVals;
    
    stateCommitRaw( state, &apiP );
    
    state->apiState = api;
}

static void*
frealloc( void* _, void* old, size_t osz, size_t nsz ) {
    if( nsz > 0 )
        return realloc( old, nsz );
    free( old );
    return NULL;
}

ten_State*
ten_make( ten_Config* config, jmp_buf* errJmp ) {
    ten_Config notnull = { 0 };
    if( !config )
        config = &notnull;
    
    if( config->frealloc == NULL )
        config->frealloc = frealloc;
    if( config->memGrowth == 0.0 )
        config->memGrowth = DEFAULT_MEM_GROWTH;
    
    State* state = config->frealloc( config->udata, NULL, 0, sizeof(State) );
    stateInit( state, config, errJmp );
    return (ten_State*)state;
}

void
ten_free( ten_State* s ) {
    State* state = (State*)s;
    stateFinl( state );
    
    void*      udata    = state->config.udata;
    ten_MemFun frealloc = state->config.frealloc;
    frealloc( udata, s, sizeof(State), 0 );
}

ten_Tup
ten_pushA( ten_State* s, char const* pat, ... ) {
    va_list ap; va_start( ap, pat );
    ten_Tup t = ten_pushV( s, pat, ap );
    va_end( ap );
    return t;
}

ten_Tup
ten_pushV( ten_State* s, char const* pat, va_list ap ) {
    State* state = (State*)s;
    uint   n   = (uint)strlen( pat );
    Tup    tup = statePush( state, n );
    
    for( uint i = 0 ; i < n ; i++ ) {
        switch( pat[i] ) {
            case 'U':
                tupAt( tup, i ) = tvUdf();
            break;
            case 'N':
                tupAt( tup, i ) = tvNil();
            break;
            case 'L':
                tupAt( tup, i ) = tvLog( va_arg( ap, int ) );
            break;
            case 'I':
                tupAt( tup, i ) = tvInt( va_arg( ap, long ) );
            break;
            case 'D': {
                double d = va_arg( ap, double );
                funAssert( !isnan( d ), "NaN given as Dec value", NULL );
                tupAt( tup, i ) = tvDec( d );
            } break;
            case 'S': {
                char const* str = va_arg( ap, char const* );
                SymT        sym = symGet( state, str, strlen( str ) );
                tupAt( tup, i ) = tvSym( sym );
            } break;
            case 'P': {
                PtrT ptr = ptrGet( state, NULL, va_arg( ap, void* ) );
                tupAt( tup, i ) = tvPtr( ptr );
            } break;
            case 'V': {
                ten_Var* var = va_arg( ap, ten_Var* );
                tupAt( tup, i ) = vget( *var );
            } break;
            default: {
                strAssert( false, "Invalid type char" );
            } break;
        }
    }
    
    ten_Tup t; memcpy( &t, &tup, sizeof(Tup) );
    return t;
}

void
ten_pop( ten_State* s ) {
    State* state = (State*)s;
    statePop( state );
}

ten_Tup
ten_dup( ten_State* s, ten_Tup* tup ) {
    State* state = (State*)s;
    uint   n   = ((Tup*)tup)->size;
    Tup    dup = statePush( state, n );
    
    for( uint i = 0 ; i < n ; i++ )
        tupAt( dup, i ) = tupAt( *(Tup*)tup, i );
    
    ten_Tup t; memcpy( &t, &dup, sizeof(Tup) );
    return t;
}

unsigned
ten_size( ten_State* state, ten_Tup* tup ) {
    return ((Tup*)tup)->size;
}

void
ten_def( ten_State* s, ten_Var* name, ten_Var* val ) {
    State* state = (State*)s;
    TVal nameV = vget( *name );
    funAssert( tvIsSym( nameV ), "Wrong type for 'name', need Sym", NULL );
    
    SymT nameS = tvGetSym( nameV );
    uint loc   = envAddGlobal( state, nameS );
    
    TVal* gval = envGetGlobalByLoc( state, loc );
    *gval = vget( *val );
}

void
ten_set( ten_State* s, ten_Var* name, ten_Var* val ) {
    State* state = (State*)s;
    TVal nameV = vget( *name );
    funAssert( tvIsSym( nameV ), "Wrong type for 'name', need Sym", NULL );
    
    SymT  nameS = tvGetSym( nameV );
    TVal* gval  = envGetGlobalByName( state, nameS );
    *gval = vget( *val );
}

void
ten_get( ten_State* s, ten_Var* name, ten_Var* dst ) {
    State* state = (State*)s;
    TVal nameV = vget( *name );
    funAssert( tvIsSym( nameV ), "Wrong type for 'name', need Sym", NULL );
    
    SymT     nameS = tvGetSym( nameV );
    TVal*    gval  = envGetGlobalByName( state, nameS );
    if( gval )
        vset( *dst, *gval );
    else
        vset( *dst, tvUdf() );
}

void
ten_type( ten_State* s, ten_Var* var, ten_Var* dst ) {
    State*    state = (State*)s;
    ApiState* api   = state->apiState;
    
    vset( *dst, tvSym( libType( state, vget( *var ) ) ) );
}

void
ten_expect( ten_State* s, char const* what, ten_Var* type, ten_Var* var ) {
    State*    state = (State*)s;
    ApiState* api   = state->apiState;
    
    TVal typeV = vget( *type );
    funAssert( tvIsSym( typeV ), "Wrong type for 'type', need Sym", NULL );
    
    libExpect( state, what, tvGetSym( typeV ), vget( *var ) );
}

bool
ten_equal( ten_State* s, ten_Var* var1, ten_Var* var2 ) {
    State* state = (State*)s;
    
    return tvEqual( vget( *var1 ), vget( *var2 ) );
}

void
ten_copy( ten_State* s, ten_Var* src, ten_Var* dst ) {
    State* state = (State*)s;
    vset( *dst, vget( *src ) );
}

char const*
ten_string( ten_State* s, ten_Tup* tup ) {
    State* state = (State*)s;
    Tup*   t     = (Tup*)tup;
    
    if( t->size == 0 ) {
        return fmtA( state, false, "()" );
    }
    
    if( t->size == 1 ) {
        return fmtA( state, false, "%q", tupAt( *t, 0 ) );
    }
    
    fmtA( state, false, "( %q", tupAt( *t, 0 ) );
    for( uint i = 1 ; i < t->size ; i++ )
        fmtA( state, true, ", %q", tupAt( *t, i ) );
    
    return fmtA( state, true, " )" );
}


void
ten_loader( ten_State* s, ten_Var* type, ten_Var* loadr, ten_Var* trans ) {
    State* state = (State*)s;
    funAssert(
        tvIsSym( vget( *type ) ),
        "Wrong type for 'type', need Sym",
        NULL
    );
    funAssert(
        tvIsObjType( vget( *loadr ), OBJ_CLS ),
        "Wrong type for 'loadr', need Cls",
        NULL
    );
    funAssert(
        tvIsObjType( vget( *trans ), OBJ_CLS ),
        "Wrong type for 'trans', need Cls",
        NULL
    );
    
    
    SymT     typeS  = tvGetSym( vget( *type ) );
    Closure* loadrO = tvGetObj( vget( *loadr ) );
    Closure* transO = tvGetObj( vget( *trans ) );
    
    libLoader( state, typeS, loadrO, transO );
}

ten_Var*
ten_udf( ten_State* s ) {
    State* state = (State*)s;
    return stateTmp( state, tvUdf() );
}

ten_Var*
ten_nil( ten_State* s ) {
    State* state = (State*)s;
    return stateTmp( state, tvNil() );
}

ten_Var*
ten_log( ten_State* s, bool log ) {
    State* state = (State*)s;
    return stateTmp( state, tvLog( log ) );
}

ten_Var*
ten_int( ten_State* s, long in ) {
    State* state = (State*)s;
    return stateTmp( state, tvInt( in ) );
}

ten_Var*
ten_dec( ten_State* s, double dec ) {
    State* state = (State*)s;
    return stateTmp( state, tvDec( dec ) );
}

ten_Var*
ten_sym( ten_State* s, char const* sym ) {
    State* state = (State*)s;
    SymT t = symGet( state, sym, strlen( sym ) );
    return stateTmp( state, tvSym( t ) );
}

ten_Var*
ten_ptr( ten_State* s, void* ptr ) {
    State* state = (State*)s;
    PtrT t = ptrGet( state, ptr, NULL );
    return stateTmp( state, tvPtr( t ) );
}

ten_Var*
ten_str( ten_State* s, char const* str ) {
    State* state = (State*)s;
    String* t = strNew( state, str, strlen( str ) );
    return stateTmp( state, tvObj( t ) );
}

typedef struct {
    ten_Source base;
    State*     state;
} Source;

typedef struct {
    ten_Source base;
    State*     state;
    FILE*      file;
} FileSource;

static void
fileSourceFinl( ten_Source* s ) {
    FileSource* src = (FileSource*)s;
    
    fclose( src->file );
    stateFreeRaw( src->state, (char*)src->base.name, strlen(src->base.name) + 1 );
    stateFreeRaw( src->state, src, sizeof(FileSource) );
}

static int
fileSourceNext( ten_Source* s ) {
    FileSource* src = (FileSource*)s;
    return getc( src->file );
}

typedef struct {
    ten_Source  base;
    State*      state;
    char const* str;
    size_t      loc;
} StringSource;

static void
stringSourceFinl( ten_Source* s ) {
    StringSource* src = (StringSource*)s;
    
    stateFreeRaw( src->state, (char*)src->base.name, strlen(src->base.name) + 1 );
    stateFreeRaw( src->state, (char*)src->str, strlen(src->str) + 1 );
    stateFreeRaw( src->state, src, sizeof(StringSource) );
}

static int
stringSourceNext( ten_Source* s ) {
    StringSource* src = (StringSource*)s;
    if( src->str[src->loc] == '\0' )
        return -1;
    else
        return src->str[src->loc++];
}

ten_Source*
ten_fileSource( ten_State* s, FILE* file, char const* name ) {
    State* state = (State*)s;
    
    Part nameP;
    size_t nameLen = strlen(name);
    char*  nameCpy = stateAllocRaw( state, &nameP, nameLen + 1 );
    strcpy( nameCpy, name );
    
    Part srcP;
    FileSource* src = stateAllocRaw( state, &srcP, sizeof(FileSource) );
    src->base.name = nameCpy;
    src->base.next = fileSourceNext;
    src->base.finl = fileSourceFinl;
    src->file      = file;
    src->state     = state;
    
    stateCommitRaw( state, &nameP );
    stateCommitRaw( state, &srcP );
    
    return (ten_Source*)src;
}

ten_Source*
ten_pathSource( ten_State* s, char const* path ) {
    State* state = (State*)s;
    
    FILE* file = fopen( path, "r" );
    if( file == NULL )
        stateErrFmtA( state, ten_ERR_SYSTEM, "%s", strerror( errno ) );
    
    return ten_fileSource( s, file, path );
}

ten_Source*
ten_stringSource( ten_State* s, char const* string, char const* name ) {
    State* state = (State*)s;
    
    Part nameP;
    size_t nameLen = strlen(name);
    char*  nameCpy = stateAllocRaw( state, &nameP, nameLen + 1 );
    strcpy( nameCpy, name );
    
    Part stringP;
    size_t stringLen = strlen(string);
    char*  stringCpy = stateAllocRaw( state, &stringP, stringLen + 1 );
    strcpy( stringCpy, string );
    
    Part srcP;
    StringSource* src = stateAllocRaw( state, &srcP, sizeof(StringSource) );
    src->base.name = nameCpy;
    src->base.next = stringSourceNext;
    src->base.finl = stringSourceFinl;
    src->str       = stringCpy;
    src->loc       = 0;
    src->state     = state;
    
    stateCommitRaw( state, &nameP );
    stateCommitRaw( state, &stringP );
    stateCommitRaw( state, &srcP );
    
    return (ten_Source*)src;
}


typedef struct {
    Defer   base;
    Source* src;
} FreeSourceDefer;

static void
freeSourceDefer( State* state, Defer* defer ) {
    FreeSourceDefer* d = (FreeSourceDefer*)defer;
    d->src->base.finl( (ten_Source*)d->src );
}


static Closure*
compileScript( State* state, char const** upvals, ten_Source* src, ten_ComScope scope )  {
    FreeSourceDefer defer = {
        .base = { .cb = freeSourceDefer },
        .src  =  (Source*)src
    };
    stateInstallDefer( state, (Defer*)&defer );
    
    ComParams params = {
        .file   = src->name,
        .params = NULL,
        .upvals = upvals,
        .debug  = !state->config.ndebug,
        .global = (scope == ten_SCOPE_GLOBAL),
        .script = true,
        .src    = src
    };
    Closure* cls = comCompile( state, &params );
    
    stateCommitDefer( state, (Defer*)&defer );
    return cls;
}

static Closure*
compileExpr( State* state, char const** upvals, ten_Source* src, ten_ComScope scope ) {
    FreeSourceDefer defer = {
        .base = { .cb = freeSourceDefer },
        .src  =  (Source*)src
    };
    stateInstallDefer( state, (Defer*)&defer );
    
    ComParams params = {
        .file   = src->name,
        .params = NULL,
        .upvals = upvals,
        .debug  = !state->config.ndebug,
        .global = ( scope == ten_SCOPE_GLOBAL),
        .script = false,
        .src    = src
    };
    Closure* cls = comCompile( state, &params );
    
    stateCommitDefer( state, (Defer*)&defer );
    return cls;
}

void
ten_compileScript( ten_State* s, char const** upvals, ten_Source* src, ten_ComScope scope, ten_ComType out, ten_Var* dst ) {
    State*    state = (State*)s;
    ApiState* api   = state->apiState;
    
    Closure* cls = compileScript( state, upvals, src, scope );
    if( out == ten_COM_CLS ) {
        vset( *dst, tvObj( cls ) );
        return;
    }
    api->val1 = tvObj( cls );
    
    Fiber* fib = fibNew( state, cls, NULL );
    vset( *dst, tvObj( fib ) );
    
    api->val1 = tvUdf();
}

void
ten_compileExpr( ten_State* s, char const** upvals, ten_Source* src, ten_ComScope scope, ten_ComType out, ten_Var* dst ) {
    State*    state = (State*)s;
    ApiState* api   = state->apiState;
    
    Closure* cls = compileExpr( state, upvals, src, scope );
    if( out == ten_COM_CLS ) {
        vset( *dst, tvObj( cls ) );
        return;
    }
    api->val1 = tvObj( cls );
    
    Fiber* fib = fibNew( state, cls, NULL );
    vset( *dst, tvObj( fib ) );
    
    api->val1 = tvUdf();
}

void
ten_executeScript( ten_State* s, ten_Source* src, ten_ComScope scope ) {
    State*    state = (State*)s;
    ApiState* api   = state->apiState;
    
    Closure* cls = compileScript( state, NULL, src, scope );
    api->val1 = tvObj( cls );
    Fiber* fib = fibNew( state, cls, NULL );
    api->fib = fib;
    
    Tup args = statePush( state, 0 );
    fibCont( state, fib, &args );
    statePop( state );
    
    api->val1 = tvUdf();
    fibPropError( state, fib );
}

ten_Tup
ten_executeExpr( ten_State* s, ten_Source* src, ten_ComScope scope ) {
    State*    state = (State*)s;
    ApiState* api   = state->apiState;
    
    Closure* cls = compileExpr( state, NULL, src, scope );
    api->val1 = tvObj( cls );
    Fiber* fib = fibNew( state, cls, NULL );
    api->fib = fib;
    
    Tup args = statePush( state, 0 );
    Tup ret  = fibCont( state, fib, &args );
    statePop( state );
    
    api->val1 = tvUdf();
    fibPropError( state, fib );
    
    return ten_dup( s, (ten_Tup*)&ret );
}

bool
ten_isUdf( ten_State* s, ten_Var* var ) {
    State* state = (State*)s;
    return tvIsUdf( vget( *var ) );
}

bool
ten_areUdf( ten_State* s, ten_Tup* tup ) {
    Tup* t = (Tup*)tup;
    for( uint i = 0 ; i < t->size ; i++ ) {
        if( !tvIsUdf( tupAt( *t, i ) ) )
            return false;
    }
    return true;
}

ten_Var*
ten_udfType( ten_State* s ) {
    State* state = (State*)s;
    return &state->apiState->typeVars[VAL_UDF];
}

bool
ten_isNil( ten_State* s, ten_Var* var ) {
    State* state = (State*)s;
    return tvIsNil( vget( *var ) );
}

bool
ten_areNil( ten_State* s, ten_Tup* tup ) {
    Tup* t = (Tup*)tup;
    for( uint i = 0 ; i < t->size ; i++ ) {
        if( !tvIsNil( tupAt( *t, i ) ) )
            return false;
    }
    return true;
}
ten_Var*
ten_nilType( ten_State* s ) {
    State* state = (State*)s;
    return &state->apiState->typeVars[VAL_NIL];
}

bool
ten_isLog( ten_State* s, ten_Var* var ) {
    State* state = (State*)s;
    return tvIsLog( vget( *var ) );
}

bool
ten_getLog( ten_State* s, ten_Var* var ) {
    State* state = (State*)s;
    funAssert( tvIsLog( vget( *var ) ), "Wrong type for 'var', need Log", NULL );
    return tvGetLog( vget( *var ) );
}

ten_Var*
ten_logType( ten_State* s ) {
    State* state = (State*)s;
    return &state->apiState->typeVars[VAL_LOG];
}

bool
ten_isInt( ten_State* s, ten_Var* var ) {
    State* state = (State*)s;
    return tvIsInt( vget( *var ) );
}


long
ten_getInt( ten_State* s, ten_Var* var ) {
    State* state = (State*)s;
    funAssert( tvIsInt( vget( *var ) ), "Wrong type for 'var', need Int", NULL );
    return tvGetInt( vget( *var ) );
}

ten_Var*
ten_intType( ten_State* s ) {
    State* state = (State*)s;
    return &state->apiState->typeVars[VAL_INT];
}

bool
ten_isDec( ten_State* s, ten_Var* var ) {
    State* state = (State*)s;
    return tvIsDec( vget( *var ) );
}

double
ten_getDec( ten_State* s, ten_Var* var ) {
    State* state = (State*)s;
    funAssert( tvIsDec( vget( *var ) ), "Wrong type for 'var', need Dec", NULL );
    
    return tvGetDec( vget( *var ) );
}

ten_Var*
ten_decType( ten_State* s ) {
    State* state = (State*)s;
    return &state->apiState->typeVars[VAL_DEC];
}

bool
ten_isSym( ten_State* s, ten_Var* var ) {
    State* state = (State*)s;
    return tvIsSym( vget( *var ) );
}

void
ten_setSym( ten_State* s, char const* sym, size_t len, ten_Var* dst ) {
    State* state = (State*)s;
    vset( *dst, tvSym( symGet( state, sym, len ) ) );
}

char const*
ten_getSymBuf( ten_State* s, ten_Var* var ) {
    State* state = (State*)s;
    funAssert( tvIsSym( vget( *var ) ), "Wrong type for 'var', need Sym", NULL );
    return symBuf( state, tvGetSym( vget( *var ) ) );
}

size_t
ten_getSymLen( ten_State* s, ten_Var* var ) {
    State* state = (State*)s;
    funAssert( tvIsSym( vget( *var ) ), "Wrong type for 'var', need Sym", NULL );
    return symLen( state, tvGetSym( vget( *var ) ) );
}

bool
ten_isPtr( ten_State* s, ten_Var* var, ten_PtrInfo* info ) {
    State* state = (State*)s;
    if( !tvIsPtr( vget( *var ) ) )
        return false;
    if( !info )
        return true;
    
    return ptrInfo( state, tvGetPtr( vget( *var ) ) ) == (PtrInfo*)info;
}

void
ten_setPtr( ten_State* s, void* addr, ten_PtrInfo* info, ten_Var* dst ) {
    State* state = (State*)s;
    vset( *dst, tvPtr( ptrGet( state, addr, (PtrInfo*)info ) ) );
}

ten_Var*
ten_symType( ten_State* s ) {
    State* state = (State*)s;
    return &state->apiState->typeVars[VAL_SYM];
}

void*
ten_getPtrAddr( ten_State* s, ten_Var* var ) {
    State* state = (State*)s;
    funAssert( tvIsPtr( vget( *var ) ), "Wrong type for 'var', need Ptr", NULL );
    
    return ptrAddr( state, tvGetPtr( vget( *var ) ) );
}

ten_PtrInfo*
ten_getPtrInfo( ten_State* s, ten_Var* var ) {
    State* state = (State*)s;
    funAssert( tvIsPtr( vget( *var ) ), "Wrong type for 'var', need Ptr", NULL );
    
    return (ten_PtrInfo*)ptrInfo( state, tvGetPtr( vget( *var ) ) );
}

ten_PtrInfo*
ten_addPtrInfo( ten_State* s, ten_PtrConfig* config ) {
    return (ten_PtrInfo*)ptrAddInfo( (State*)s, config );
}

ten_Var*
ten_ptrType( ten_State* s, ten_PtrInfo* info ) {
    State* state = (State*)s;
    if( info )
        return &((PtrInfo*)info)->typeVar;
    else
        return &state->apiState->typeVars[VAL_PTR];
}

bool
ten_isStr( ten_State* s, ten_Var* var ) {
    State* state = (State*)s;
    TVal val = vget( *var );
    return tvIsObj( val ) && datGetTag( tvGetObj( val ) ) == OBJ_STR;
}

void
ten_newStr( ten_State* s, char const* str, size_t len, ten_Var* dst ) {
    State* state = (State*)s;
    vset( *dst, tvObj( strNew( state, str, len ) ) );
}

char const*
ten_getStrBuf( ten_State* s, ten_Var* var ) {
    State* state = (State*)s;
    TVal val = vget( *var );
    funAssert(
        tvIsObj( val ) && datGetTag( tvGetObj( val ) ) == OBJ_STR,
        "Wrong type for 'var', need Str",
        NULL
    );
    return strBuf( state, tvGetObj( val ) );
}

size_t
ten_getStrLen( ten_State* s, ten_Var* var ) {
    State* state = (State*)s;
    TVal val = vget( *var );
    funAssert(
        tvIsObj( val ) && datGetTag( tvGetObj( val ) ) == OBJ_STR,
        "Wrong type for 'var', need Str",
        NULL
    );
    return strLen( state, tvGetObj( val ) );
}


ten_Var*
ten_strType( ten_State* s ) {
    State* state = (State*)s;
    return &state->apiState->typeVars[OBJ_STR];
}

bool
ten_isIdx( ten_State* s, ten_Var* var ) {
    State* state = (State*)s;
    TVal val = vget( *var );
    return tvIsObj( val ) && datGetTag( tvGetObj( val ) ) == OBJ_IDX;
}

void
ten_newIdx( ten_State* s, ten_Var* dst ) {
    State* state = (State*)s;
    vset( *dst, tvObj( idxNew( state ) ) );
}

ten_Var*
ten_idxType( ten_State* s ) {
    State* state = (State*)s;
    return &state->apiState->typeVars[OBJ_IDX];
}

bool
ten_isRec( ten_State* s, ten_Var* var ) {
    State* state = (State*)s;
    TVal val = vget( *var );
    return tvIsObj( val ) && datGetTag( tvGetObj( val ) ) == OBJ_REC;
}

void
ten_newRec( ten_State* s, ten_Var* idx, ten_Var* dst ) {
    State* state = (State*)s;
    Index* idxO   = NULL;
    if( idx ) {
        TVal idxV = vget( *idx );
        funAssert(
            tvIsObj( idxV ) && datGetTag( tvGetObj( idxV ) ) == OBJ_IDX,
            "Wrong type for 'idx', need Idx",
            NULL
        );
        idxO = tvGetObj( idxV );
    }
    else {
        idxO = idxNew( state );
        state->apiState->val1 = tvObj( idxO );
    }
    
    vset( *dst, tvObj( recNew( state, idxO ) ) );
}

void
ten_recSep( ten_State* s, ten_Var* rec ) {
    State* state = (State*)s;
    TVal recV = vget( *rec );
    funAssert(
        tvIsObj( recV ) && datGetTag( tvGetObj( recV ) ) == OBJ_REC,
        "Wrong type for 'rec', need Rec",
        NULL
    );
    recSep( state, tvGetObj( recV ) );
}

void
ten_recDef( ten_State* s, ten_Var* rec, ten_Var* key, ten_Var* val ) {
    State* state = (State*)s;
    TVal recV = vget( *rec );
    funAssert(
        tvIsObj( recV ) && datGetTag( tvGetObj( recV ) ) == OBJ_REC,
        "Wrong type for 'rec', need Rec",
        NULL
    );
    recDef( state, tvGetObj( recV ), vget( *key ), vget( *val ) );
}
void
ten_recSet( ten_State* s, ten_Var* rec, ten_Var* key, ten_Var* val ) {
    State* state = (State*)s;
    TVal recV = vget( *rec );
    funAssert(
        tvIsObj( recV ) && datGetTag( tvGetObj( recV ) ) == OBJ_REC,
        "Wrong type for 'rec', need Rec",
        NULL
    );
    recSet( state, tvGetObj( recV ), vget( *key ), vget( *val ) );
}

void
ten_recGet( ten_State* s, ten_Var* rec, ten_Var* key, ten_Var* dst ) {
    State* state = (State*)s;
    TVal recV = vget( *rec );
    funAssert(
        tvIsObj( recV ) && datGetTag( tvGetObj( recV ) ) == OBJ_REC,
        "Wrong type for 'rec', need Rec",
        NULL
    );
    vset( *dst, recGet( state, tvGetObj( recV ), vget( *key ) ) );
}

ten_Var*
ten_recType( ten_State* s ) {
    State* state = (State*)s;
    return &state->apiState->typeVars[OBJ_REC];
}

bool
ten_isFun( ten_State* s, ten_Var* var ) {
    State* state = (State*)s;
    TVal val = vget( *var );
    return tvIsObj( val ) && datGetTag( tvGetObj( val ) ) == OBJ_FUN;
}

void
ten_newFun( ten_State* s, ten_FunParams* p, ten_Var* dst ) {
    State* state = (State*)s;
    
    SymT sparams[TUP_MAX];
    
    uint nParams = 0;
    bool vParams = false;
    if( p->params ) {
        for( uint i = 0 ; p->params[i] != NULL ; i++ ) {
            if( i > TUP_MAX )
                stateErrFmtA( state, ten_ERR_USER, "Too many parameters, max is %u", (uint)TUP_MAX );
            if( vParams )
                stateErrFmtA( state, ten_ERR_USER, "Extra parameters after '...'" );
            
            size_t len = 0;
            if( !isalpha( p->params[i][0] ) && p->params[i][0] != '_' )
                stateErrFmtA( state, ten_ERR_USER, "Invalid parameter name '%s'", p->params[i] );
            
            for( uint j = 0 ; p->params[i][j] != '\0' && p->params[i][j] != '.' ; j++ ) {
                if( !isalnum( p->params[i][j] ) && p->params[i][j] != '_' )
                    stateErrFmtA( state, ten_ERR_USER, "Invalid parameter name" );
                len++;
            }
            
            if( p->params[i][len] == '.' ) {
                if( !strcmp( &p->params[i][len], "..." ) )
                    vParams = true;
                else
                    stateErrFmtA( state, ten_ERR_USER, "Invalid parameter name" );
            }
            else {
                nParams++;
            }
            
            sparams[i] = symGet( state, p->params[i], len );
        }
    }
    
    Part paramsP;
    SymT* params = stateAllocRaw( state, &paramsP, sizeof(SymT)*nParams );
    memcpy( params, sparams, sizeof(SymT)*nParams );
    
    Function* fun =
        funNewNat( state, nParams, vParams ? idxNew( state ) : NULL, p->cb );
    fun->u.nat.params = params;
    if( p->name )
        fun->u.nat.name = symGet( state, p->name, strlen( p->name ) );
    
    stateCommitRaw( state, &paramsP );
    vset( *dst, tvObj( fun ) );
}

ten_Var*
ten_funType( ten_State* s ) {
    State* state = (State*)s;
    return &state->apiState->typeVars[OBJ_FUN];
}

bool
ten_isCls( ten_State* s, ten_Var* var ) {
    State* state = (State*)s;
    TVal val = vget( *var );
    return tvIsObj( val ) && datGetTag( tvGetObj( val ) ) == OBJ_CLS;
}

void
ten_newCls( ten_State* s, ten_Var* fun, ten_Var* dat, ten_Var* dst ) {
    State* state = (State*)s;
    TVal funV = vget( *fun );
    funAssert(
        tvIsObj( funV ) && datGetTag( tvGetObj( funV ) ) == OBJ_FUN,
        "Wrong type for 'fun', need Fun",
        NULL
    );
    Function* funO = tvGetObj( funV );
    
    Data* datO = NULL;
    if( dat ) {
        TVal datV = vget( *dat );
        funAssert(
            tvIsObj( datV ) && datGetTag( tvGetObj( datV ) ) == OBJ_DAT,
            "Wrong type for 'dat', need Dat",
            NULL
        );
        datO = tvGetObj( datV );
    }
    
    if( funO->type == FUN_NAT )
        vset( *dst, tvObj( clsNewNat( state, funO, datO ) ) );
    else
        vset( *dst, tvObj( clsNewVir( state, funO, NULL ) ) );
}

void
ten_getUpvalue( ten_State* s, ten_Var* cls, unsigned upv, ten_Var* dst ) {
    State* state = (State*)s;
    TVal clsV = vget( *cls );
    funAssert(
        tvIsObjType( clsV, OBJ_CLS ),
        "Wrong type for 'cls', need Cls",
        NULL
    );
    Closure* clsO = tvGetObj( clsV );
    funAssert(
        clsO->fun->type == FUN_VIR, 
        "Can't get upvalue of native closure 'cls'",
        NULL
    );
    funAssert(
        upv < clsO->fun->u.vir.nUpvals,
        "Closure 'cls' has no upvalue %u",
        upv,
        NULL
    );
    vset( *dst, clsO->dat.upvals[upv]->val );
}

void
ten_setUpvalue( ten_State* s, ten_Var* cls, unsigned upv, ten_Var* src ) {
    State* state = (State*)s;
    TVal clsV = vget( *cls );
    funAssert(
        tvIsObjType( clsV, OBJ_CLS ),
        "Wrong type for 'cls', need Cls",
        NULL
    );
    Closure* clsO = tvGetObj( clsV );
    funAssert(
        clsO->fun->type == FUN_VIR, 
        "Can't set upvalue of native closure 'cls'",
        NULL
    );
    funAssert(
        upv < clsO->fun->u.vir.nUpvals,
        "Closure 'cls' has no upvalue %u",
        upv,
        NULL
    );
    clsO->dat.upvals[upv] = upvNew( state, vget( *src ) );
}

ten_Var*
ten_clsType( ten_State* s ) {
    State* state = (State*)s;
    return &state->apiState->typeVars[OBJ_CLS];
}

bool
ten_isFib( ten_State* s, ten_Var* var ) {
    State* state = (State*)s;
    TVal val = vget( *var );
    return tvIsObj( val ) && datGetTag( tvGetObj( val ) ) == OBJ_FIB;
}

void
ten_newFib( ten_State* s, ten_Var* cls, ten_Var* tag, ten_Var* dst ) {
    State* state = (State*)s;
    TVal clsV = vget( *cls );
    funAssert(
        tvIsObj( clsV ) && datGetTag( tvGetObj( clsV ) ) == OBJ_CLS,
        "Wrong type for 'cls', need Cls",
        NULL
    );
    Closure* clsO = tvGetObj( clsV );
    
    if( tag ) {
        TVal tagV = vget( *tag );
        funAssert(
            tvIsSym( tagV ),
            "Wrong type for 'tag', need Sym",
            NULL
        );
        SymT tagS = tvGetSym( tagV );
        vset( *dst, tvObj( fibNew( state, clsO, &tagS ) ) );
    }
    else {
        vset( *dst, tvObj( fibNew( state, clsO, NULL ) ) );
    }
}

ten_FibState
ten_state( ten_State* s, ten_Var* fib ) {
    State* state = (State*)s;
    TVal fibV = vget( *fib );
    funAssert(
        tvIsObj( fibV ) && datGetTag( tvGetObj( fibV ) ) == OBJ_FIB,
        "Wrong type for 'fib', need Fib",
        NULL
    );
    Fiber* fibO = tvGetObj( fibV );
    return fibO->state;
}

ten_Tup
ten_cont( ten_State* s, ten_Var* fib, ten_Tup* args ) {
    State* state = (State*)s;
    TVal fibV = vget( *fib );
    funAssert(
        tvIsObj( fibV ) && datGetTag( tvGetObj( fibV ) ) == OBJ_FIB,
        "Wrong type for 'fib', need Fib",
        NULL
    );
    Fiber* fibO = tvGetObj( fibV );
    
    Tup tup = fibCont( state, fibO, (Tup*)args );
    
    return ten_dup( s, (ten_Tup*)&tup );
}

void
ten_panic( ten_State* s, ten_Var* val ) {
    State* state = (State*)s;
    stateErrVal( state, ten_ERR_PANIC, vget( *val ) );
}

ten_Tup
ten_call_( ten_State* s, ten_Var* cls, ten_Tup* args, char const* file, unsigned line ) {
    State* state = (State*)s;
    
    funAssert( state->fiber, "Call without running fiber", NULL );
    
    TVal clsV = vget( *cls );
    funAssert(
        tvIsObj( clsV ) && datGetTag( tvGetObj( clsV ) ) == OBJ_CLS,
        "Wrong type for 'cls', need Cls",
        NULL
    );
    Closure* clsO = tvGetObj( clsV );
    
    Tup tup = fibCall_( state, clsO, (Tup*)args, file, line );
    
    ten_Tup t;
    memcpy( &t, &tup, sizeof(Tup) );
    return t;
}

void
ten_yield( ten_State* s, ten_Tup* vals ) {
    State* state = (State*)s;
    
    funAssert( state->fiber, "Yield without running fiber", NULL );
    
    fibYield( state, (Tup*)vals, false );
}

long
ten_seek( ten_State* s, void* ctx, size_t size ) {
    State* state = (State*)s;
    
    funAssert( state->fiber, "Yield without running fiber", NULL );
    
    return fibSeek( state, ctx, size );
}

void
ten_checkpoint( ten_State* s, unsigned cp, ten_Tup* dst ) {
    State* state = (State*)s;
    
    funAssert( state->fiber, "Yield without running fiber", NULL );
    
    fibCheckpoint( state, cp, (Tup*)dst );
}

ten_Var*
ten_fibType( ten_State* s ) {
    State* state = (State*)s;
    return &state->apiState->typeVars[OBJ_FIB];
}

ten_ErrNum
ten_getErrNum( ten_State* s, ten_Var* fib ) {
    State* state = (State*)s;
    if( fib ) {
        TVal fibV = vget( *fib );
        funAssert(
            tvIsObj( fibV ) && datGetTag( tvGetObj( fibV ) ) == OBJ_FIB,
            "Wrong type for 'fib', need Fib",
            NULL
        );
        Fiber* fibO = tvGetObj( fibV );
        return fibO->errNum;
    }
    else {
        return state->errNum;
    }
}

void
ten_getErrVal( ten_State* s, ten_Var* fib, ten_Var* dst ) {
    State* state = (State*)s;
    if( fib ) {
        TVal fibV = vget( *fib );
        funAssert(
            tvIsObj( fibV ) && datGetTag( tvGetObj( fibV ) ) == OBJ_FIB,
            "Wrong type for 'fib', need Fib",
            NULL
        );
        Fiber* fibO = tvGetObj( fibV );
        vset( *dst, fibO->errVal );
    }
    else {
        vset( *dst, state->errVal );
    }
}

char const*
ten_getErrStr( ten_State* s, ten_Var* fib ) {
    State* state = (State*)s;
    if( fib ) {
        TVal fibV = vget( *fib );
        funAssert(
            tvIsObj( fibV ) && datGetTag( tvGetObj( fibV ) ) == OBJ_FIB,
            "Wrong type for 'fib', need Fib",
            NULL
        );
        Fiber* fibO = tvGetObj( fibV );
        return fmtA( state, false, "%v", fibO->errVal );
    }
    else {
        return fmtA( state, false, "%v", state->errVal );
    }
}

ten_Trace*
ten_getTrace( ten_State* s, ten_Var* fib ) {
    State* state = (State*)s;
    return state->trace;
}

void
ten_clearError( ten_State* s, ten_Var* fib ) {
    State* state = (State*)s;
    if( fib ) {
        TVal fibV = vget( *fib );
        funAssert(
            tvIsObj( fibV ) && datGetTag( tvGetObj( fibV ) ) == OBJ_FIB,
            "Wrong type for 'fib', need Fib",
            NULL
        );
        Fiber* fibO = tvGetObj( fibV );
        fibClearError( state, fibO );
    }
    else {
        stateClearError( state );
    }
}

void
ten_propError( ten_State* s, ten_Var* fib ) {
    State* state = (State*)s;
    if( fib ) {
        TVal fibV = vget( *fib );
        funAssert(
            tvIsObj( fibV ) && datGetTag( tvGetObj( fibV ) ) == OBJ_FIB,
            "Wrong type for 'fib', need Fib",
            NULL
        );
        Fiber* fibO = tvGetObj( fibV );
        fibPropError( state, fibO );
    }
    else {
        stateErrProp( state );
    }
}

jmp_buf*
ten_swapErrJmp( ten_State* s, jmp_buf* errJmp ) {
    return stateSwapErrJmp( (State*)s, errJmp );
}

bool
ten_isDat( ten_State* s, ten_Var* var, ten_DatInfo* info ) {
    State* state = (State*)s;
    TVal val = vget( *var );
    
    if( !tvIsObjType( val, OBJ_DAT ) )
        return false;
    if( !info )
        return true;
    
    Data* dat = tvGetObj( val );
    return dat->info == (DatInfo*)info;
}

void*
ten_newDat( ten_State* s, ten_DatInfo* info, ten_Var* dst ) {
    State* state = (State*)s;
    funAssert( info, "DatInfo 'info' is required", NULL );
    
    Data* dat = datNew( state, (DatInfo*)info );
    vset( *dst, tvObj( dat ) );
    return dat->data;
}

void
ten_setMember( ten_State* s, ten_Var* dat, unsigned mem, ten_Var* val ) {
    State* state = (State*)s;
    TVal datV = vget( *dat );
    funAssert(
        tvIsObj( datV ) && datGetTag( tvGetObj( datV ) ) == OBJ_DAT,
        "Wrong type for 'dat', need Dat",
        NULL
    );
    Data* datO = tvGetObj( datV );
    
    funAssert( mem < datO->info->nMems, "No member %u", mem );
    datO->mems[mem] = vget( *val );
}

void
ten_getMember( ten_State* s, ten_Var* dat, unsigned mem, ten_Var* dst ) {
    State* state = (State*)s;
    TVal datV = vget( *dat );
    funAssert(
        tvIsObj( datV ) && datGetTag( tvGetObj( datV ) ) == OBJ_DAT,
        "Wrong type for 'dat', need Dat",
        NULL
    );
    Data* datO = tvGetObj( datV );
    
    funAssert( mem < datO->info->nMems, "No member %u", mem );
    vset( *dst, datO->mems[mem] );
}

ten_DatInfo*
ten_getDatInfo( ten_State* s, ten_Var* dat ) {
    State* state = (State*)s;
    TVal datV = vget( *dat );
    funAssert(
        tvIsObj( datV ) && datGetTag( tvGetObj( datV ) ) == OBJ_DAT,
        "Wrong type for 'dat', need Dat",
        NULL
    );
    Data* datO = tvGetObj( datV );
    return (ten_DatInfo*)datO->info;
}

void*
ten_getDatBuf( ten_State* s, ten_Var* dat ) {
    State* state = (State*)s;
    TVal datV = vget( *dat );
    funAssert(
        tvIsObj( datV ) && datGetTag( tvGetObj( datV ) ) == OBJ_DAT,
        "Wrong type for 'dat', need Dat",
        NULL
    );
    Data* datO = tvGetObj( datV );
    return datO->data;
}

ten_DatInfo*
ten_addDatInfo( ten_State* s, ten_DatConfig* config ) {
    return (ten_DatInfo*)datAddInfo( (State*)s, config );
}


ten_Var*
ten_datType( ten_State* s, ten_DatInfo* info ) {
    State* state = (State*)s;
    if( info )
        return &((DatInfo*)info)->typeVar;
    else
        return &state->apiState->typeVars[OBJ_DAT];
}
