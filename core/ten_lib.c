#include "ten_lib.h"
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
#include "ten_state.h"
#include "ten_assert.h"
#include "ten_macros.h"

#include <stdio.h>
#include <string.h>

typedef enum {
    IDENT_require,
    IDENT_import,
    IDENT_type,
    IDENT_panic,
    IDENT_assert,
    IDENT_expect,
    IDENT_collect,
    IDENT_loader,
    
    IDENT_log,
    IDENT_int,
    IDENT_dec,
    IDENT_sym,
    IDENT_str,
    
    IDENT_keys,
    IDENT_vals,
    IDENT_pairs,
    IDENT_stream,
    IDENT_bytes,
    IDENT_chars,
    IDENT_items,
    
    IDENT_show,
    IDENT_warn,
    IDENT_input,
    
    IDENT_T,
    IDENT_N,
    IDENT_R,
    IDENT_L,
    IDENT_A,
    IDENT_Q,
    IDENT_Z,
    
    IDENT_ucode,
    IDENT_uchar,
    
    IDENT_cat,
    IDENT_join,
    
    IDENT_each,
    IDENT_fold,
    IDENT_while,
    IDENT_until,
    
    IDENT_expand,
    IDENT_cons,
    IDENT_list,
    
    IDENT_fiber,
    IDENT_cont,
    IDENT_yield,
    IDENT_status,
    
    IDENT_tag,
    
    IDENT_LAST
} Ident;

typedef enum {
    OPER_ILT,
    OPER_IMT,
    OPER_IET,
    OPER_ILE,
    OPER_IME,
    OPER_NET,
    OPER_LAST
} Oper;

struct LibState {
    Finalizer finl;
    Scanner   scan;
    
    TVal val1;
    TVal val2;
    
    Record* loaders;
    Record* translators;
    Record* modules;
    
    SymT idents[IDENT_LAST];
    SymT opers[OPER_LAST];
    SymT types[OBJ_LAST];
};

static void
libScan( State* state, Scanner* scan ) {
    LibState* lib = structFromScan( LibState, scan );
    
    tvMark( lib->val1 );
    tvMark( lib->val2 );
    
    if( lib->loaders )
        stateMark( state, lib->loaders );
    if( lib->translators )
        stateMark( state, lib->translators );
    if( lib->modules )
        stateMark( state, lib->modules );
    
    
    if( !state->gcFull )
        return;
    
    for( uint i = 0 ; i < IDENT_LAST ; i++ )
        symMark( state, lib->idents[i] );
    for( uint i = 0 ; i < OPER_LAST ; i++ )
        symMark( state, lib->opers[i] );
    for( uint i = 0 ; i < OBJ_LAST ; i++ )
        symMark( state, lib->types[i] );
}

static void
libFinl( State* state, Finalizer* finl ) {
    LibState* lib = structFromFinl( LibState, finl );
    stateRemoveScanner( state, &lib->scan );
    stateFreeRaw( state, lib, sizeof(LibState) );
}

static TVal
load( State* state, String* mod ) {
    LibState* lib = state->libState;
    
    uint i = 0;
    while( mod->buf[i] != ':' &&  i < mod->len )
        i++;
    if( i == mod->len )
        panic( "No module type specified in '%v'", tvObj( mod ) );
    
    TVal modkey = tvSym( symGet( state, mod->buf, mod->len ) );
    TVal module = recGet( state, lib->modules, modkey );
    if( !tvIsUdf( module ) )
        return module;
    
    Tup parts = statePush( state, 2 );
    unsigned const typeL = 0;
    unsigned const pathL = 1;
    
    SymT typeS = symGet( state, mod->buf, i );
    tupAt( parts, typeL ) = tvSym( typeS );
    
    String* pathS = strNew( state, &mod->buf[ i + 1 ], mod->len - i - 1 );
    tupAt( parts, pathL ) = tvObj( pathS );
    
    
    TVal trans = recGet( state, lib->translators, tvSym( typeS ) );
    if( tvIsObjType( trans, OBJ_CLS ) ) {
        Closure* cls = tvGetObj( trans );
        
        Tup args = statePush( state, 1 );
        tupAt( args, 0 ) = tvObj( pathS );
        
        Tup rets = fibCall( state, cls, &args );
        if( rets.size != 1 )
            panic( "Import translator returned tuple" );
        TVal ret = tupAt( rets, 0 );
        if( !tvIsObjType( ret, OBJ_STR ) )
            panic( "Import translator return is not Str" );
        
        pathS = tvGetObj( ret );
        tupAt( parts, pathL ) = tvObj( pathS );
        
        size_t typeLen = symLen( state, typeS );
        size_t pathLen = pathS->len;
        
        size_t len = typeLen + 1 + pathLen;
        char   buf[len];
        memcpy( buf, symBuf( state, typeS ), typeLen );
        buf[typeLen] = ':';
        memcpy( buf + typeLen + 1, pathS->buf, pathLen );
        mod = strNew( state, buf, len );
        
        statePop( state );
        statePop( state );
    }
    
    modkey = tvSym( symGet( state, mod->buf, mod->len ) );
    module = recGet( state, lib->modules, modkey );
    if( !tvIsUdf( module ) )
        return module;
    
    TVal loadr = recGet( state, lib->loaders, tvSym( typeS ) );
    if( !tvIsObjType( loadr, OBJ_CLS ) )
        return tvUdf();
    
    Closure* cls = tvGetObj( loadr );
    
    Tup args = statePush( state, 1 );
    tupAt( args, 0 ) = tvObj( pathS );
    
    Tup rets = fibCall( state, cls, &args );
    if( rets.size != 1 )
        panic( "Import loader returned tuple" );
    
    module = tupAt( rets, 0 );
    if( !tvIsUdf( module ) )
        recDef( state, lib->modules, modkey, module );
    
    statePop( state );
    statePop( state );
    
    return module;
}

TVal
libRequire( State* state, String* mod ) {
    TVal module = load( state, mod );
    if( tvIsUdf( module ) )
        panic( "Import failed for '%v'", tvObj( mod ) );
    
    return module;
}

TVal
libImport( State* state, String* mod ) {
    return load( state, mod );
}

SymT
libType( State* state, TVal val ) {
    LibState* lib = state->libState;
    
    int  tag = tvGetTag( val );
    if( tag == VAL_OBJ )
        tag = datGetTag( tvGetObj( val ) );
    
    if( tag == VAL_PTR ) {
        PtrT     ptr  = tvGetPtr( val );
        PtrInfo* info = ptrInfo( state, ptr );
        if( info )
            return info->type;
        else
            return lib->types[VAL_PTR];
    }
    
    if( tag <= OBJ_IDX )
        return lib->types[tag];
    
    if( tag == OBJ_REC ) {
        TVal rTag = recGet( state, tvGetObj( val ), tvSym( lib->idents[IDENT_tag] ) );
        if( !tvIsUdf( rTag ) ) {
            char const* str = fmtA( state, false, "Rec:%v", rTag );
            size_t      len = fmtLen( state );
            return symGet( state, str, len );
        }
        else {
            return lib->types[OBJ_REC];
        }
    }
    if( tag == OBJ_FUN )
        return lib->types[OBJ_FUN];
    if( tag == OBJ_CLS )
        return lib->types[OBJ_CLS];
    if( tag == OBJ_FIB )
        return lib->types[OBJ_FIB];
    if( tag == OBJ_DAT ) {
        Data* dat = tvGetObj( val );
        return dat->info->type;
    }
    
    char const* str = fmtA( state, false, "%t", val );
    size_t      len = fmtLen( state );
    return symGet( state, str, len );
}

void
libPanic( State* state, TVal val ) {
    panic( "%v", val );
}

void
libAssert( State* state, TVal cond, TVal what ) {
    if( ( tvIsLog( cond ) && tvGetLog( cond ) == false ) || tvIsNil( cond ) )
        panic( "Assertion failed: %v", what );
}

void
libExpect( State* state, char const* what, SymT type, TVal val ) {
    LibState* lib = state->libState;
    
    int  tag = tvGetTag( val );
    if( tag == VAL_OBJ )
        tag = datGetTag( tvGetObj( val ) );
    
    if( tag == VAL_PTR ) {
        if( type == lib->types[VAL_PTR] )
            goto good;
        
        PtrT     ptr  = tvGetPtr( val );
        PtrInfo* info = ptrInfo( state, ptr );
        if( info && type == info->type )
            goto good;
        else
            goto bad;
    }
    
    if( tag <= OBJ_IDX ) {
        if( type == lib->types[tag] )
            goto good;
        else
            goto bad;
    }
    
    if( tag == OBJ_REC ) {
        if( type == lib->types[OBJ_REC] )
            goto good;
        
        TVal rTag = recGet( state, tvGetObj( val ), tvSym( lib->idents[IDENT_tag] ) );
        if( !tvIsUdf( rTag ) ) {
            char const* str = fmtA( state, false, "Rec:%v", rTag );
            if( !strcmp( symBuf( state, type ), str ) )
                goto good;
            else
                goto bad;
        }
        else {
            goto bad;
        }
    }
    if( tag == OBJ_FUN ) {
        if( type == lib->types[OBJ_FUN] )
            goto good;
        else
            goto bad;
    }
    if( tag == OBJ_CLS ) {
        if( type == lib->types[OBJ_CLS] )
            goto good;
        else
            goto bad;
    }
    if( tag == OBJ_FIB ) {
        if( type == lib->types[OBJ_FIB] )
            goto good;
        else
            goto bad;
    }
    if( tag == OBJ_DAT ) {
        if( type == lib->types[OBJ_DAT] )
            goto good;
        
        Data* dat = tvGetObj( val );
        if( type == dat->info->type )
            goto good;
        else
            goto bad;
    }
    
    good: {
        return;
    }
    bad: {
        panic( "Wrong type %t for '%s', need %v", val, what, tvSym( type ) );
    }
}

void
libCollect( State* state ) {
    stateCollect( state );
}

void
libLoader( State* state, SymT type, Closure* loadr, Closure* trans ) {
    LibState* lib = state->libState;
    
    if( loadr )
        recDef( state, lib->loaders, tvSym( type ), tvObj( loadr ) );
    else
        recDef( state, lib->loaders, tvSym( type ), tvUdf() );
    if( trans )
        recDef( state, lib->translators, tvSym( type ), tvObj( trans ) );
    else
        recDef( state, lib->translators, tvSym( type ), tvUdf() );
}

TVal
libLog( State* state, TVal val ) {
    if( tvIsLog( val ) )
        return val;
    if( tvIsInt( val ) )
        return tvLog( tvGetInt( val ) != 0 );
    if( tvIsDec( val ) )
        return tvLog( tvGetDec( val ) != 0.0 );
    if( tvIsSym( val ) ) {
        if( !strcmp( symBuf( state, tvGetSym( val ) ), "true" ) )
            return tvLog( true );
        else
        if( !strcmp( symBuf( state, tvGetSym( val ) ), "false" ) )
            return tvLog( false );
        else
            return tvUdf();
    }
    if( tvIsObjType( val, OBJ_STR ) ) {
        if( !strcmp( strBuf( state, tvGetObj( val ) ), "true" ) )
            return tvLog( true );
        else
        if( !strcmp( strBuf( state, tvGetObj( val ) ), "false" ) )
            return tvLog( false );
        else
            return tvUdf();
    }
    return tvUdf();
}

TVal
libInt( State* state, TVal val ) {
    if( tvIsInt( val ) )
        return val;
    if( tvIsLog( val ) )
        return tvInt( !!tvGetLog( val ) );
    if( tvIsDec( val ) )
        return tvInt( (IntT)tvGetDec( val ) );
    if( tvIsSym( val ) ) {
        SymT sym = tvGetSym( val );
        
        char*       end;
        char const* start = symBuf( state, sym );
        
        IntT i = strtol( start, &end, 0 );
        if( end != &start[symLen( state, sym )] )
            return tvUdf();
        
        return tvInt( i );
    }
    if( tvIsObjType( val, OBJ_STR ) ) {
        String* str = tvGetObj( val );
        
        char*       end;
        char const* start = strBuf( state, str );
        
        IntT i = strtol( start, &end, 0 );
        if( end != &start[strLen( state, str )] )
            return tvUdf();
        
        return tvInt( i );
    }
    return tvUdf();
}

TVal
libDec( State* state, TVal val ) {
    if( tvIsDec( val ) )
        return val;
    if( tvIsLog( val ) )
        return tvDec( (DecT)!!tvGetLog( val ) );
    if( tvIsInt( val ) )
        return tvDec( (DecT)tvGetInt( val ) );
    if( tvIsSym( val ) ) {
        SymT sym = tvGetSym( val );
        
        char*       end;
        char const* start = symBuf( state, sym );
        
        DecT f = strtof( start, &end );
        if( end != &start[symLen( state, sym )] )
            return tvUdf();
        
        return tvDec( f );
    }
    if( tvIsObjType( val, OBJ_STR ) ) {
        String* str = tvGetObj( val );
        
        char*       end;
        char const* start = strBuf( state, str );
        
        DecT f = strtof( start, &end );
        if( end != &start[strLen( state, str )] )
            return tvUdf();
        
        return tvDec( f );
    }
    return tvUdf();
}

TVal
libSym( State* state, TVal val ) {
    if( tvIsSym( val ) )
        return val;
    if( tvIsObjType( val, OBJ_STR ) ) {
        String* str = tvGetObj( val );
        SymT    sym = symGet( state, strBuf( state, str ), strLen( state, str ) );
        
        return tvSym( sym );
    }
    char const* buf = fmtA( state, false, "%v", val );
    size_t      len = fmtLen( state );
    
    return tvSym( symGet( state, buf, len ) );
}

TVal
libStr( State* state, TVal val ) {
    if( tvIsObjType( val, OBJ_STR ) )
        return val;
    if( tvIsSym( val ) ) {
        SymT    sym = tvGetSym( val );
        String* str = strNew( state, symBuf( state, sym ), symLen( state, sym ) );
        
        return tvObj( str );
    }
    char const* buf = fmtA( state, false, "%v", val );
    size_t      len = fmtLen( state );
    
    return tvObj( strNew( state, buf, len ) );
}

void
libShow( State* state, Record* rec ) {
    fmtA( state, false, "" );
    
    uint  i = 0;
    TVal  v = recGet( state, rec, tvInt( i++ ) );
    while( !tvIsUdf( v ) ) {
        fmtA( state, true, "%v", v );
        v = recGet( state, rec, tvInt( i++ ) );
    }
    fwrite( fmtBuf( state ), 1, fmtLen( state ), stdout );
}

TVal
libBcmp( State* state, String* str1, SymT opr, String* str2 ) {
    LibState* lib = state->libState;
    
    size_t len = str2->len < str1->len ? str2->len : str1->len;
    int r = memcmp( str1->buf, str2->buf, len + 1 );
    
    if( opr == lib->opers[OPER_ILT] )
        return  tvLog( r < 0 );
    if( opr == lib->opers[OPER_IMT] )
        return tvLog( r > 0 );
    if( opr == lib->opers[OPER_IET] )
        return tvLog( r == 0 );
    if( opr == lib->opers[OPER_ILE] )
        return tvLog( r <= 0 );
    if( opr == lib->opers[OPER_IME] )
        return tvLog( r >= 0 );
    if( opr == lib->opers[OPER_NET] )
        return tvLog( r != 0 );
    
    return tvUdf();
}



#define expectArg( ARG, TYPE ) \
    libExpect( state, #ARG, state->libState->types[TYPE], ref(&ARG ## Arg) ) 

static void
requireFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var modArg = { .tup = args, .loc = 0 };
    expectArg( mod, OBJ_STR );
    
    Tup rets = statePush( state, 1 );
    tupAt( rets, 0 ) = libRequire( state, tvGetObj( ref(&modArg) ) );
}

static void
importFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var modArg = { .tup = args, .loc = 0 };
    expectArg( mod, OBJ_STR );
    
    Tup rets = statePush( state, 1 );
    tupAt( rets, 0 ) = libImport( state, tvGetObj( ref(&modArg) ) );
}

static void
typeFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var valArg = { .tup = args, .loc = 0 };
    
    Tup rets = statePush( state, 1 );
    tupAt( rets, 0 ) = tvSym( libType( state, ref(&valArg) ) );
}

static void
panicFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var errArg = { .tup = args, .loc = 0 };
    panic( "%v", ref(&errArg) );
}

static void
assertFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var condArg = { .tup = args, .loc = 0 };
    ten_Var whatArg = { .tup = args, .loc = 1 };
    
    TVal cond = ref(&condArg);
    TVal what = ref(&whatArg);
    libAssert( state, cond, what );
    
    statePush( state, 0 );
}

static void
expectFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var whatArg = { .tup = args, .loc = 0 };
    ten_Var typeArg = { .tup = args, .loc = 1 };
    ten_Var valArg  = { .tup = args, .loc = 2 };
    
    expectArg( what, OBJ_STR );
    String* what = tvGetObj( ref(&whatArg) );
    
    expectArg( type, VAL_SYM );
    SymT type = tvGetSym( ref(&typeArg) );
    
    libExpect( state, what->buf, type, ref(&valArg) );
    
    statePush( state, 0 );
}

static void
collectFun( ten_PARAMS ) {
    State* state = (State*)ten;
    libCollect( state );
}

static void
loaderFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var typeArg  = { .tup = args, .loc = 0 };
    ten_Var loadrArg = { .tup = args, .loc = 1 };
    ten_Var transArg = { .tup = args, .loc = 2 };
    
    expectArg( type, VAL_SYM );
    SymT type = tvGetSym( ref(&typeArg) );
    
    expectArg( loadr, OBJ_CLS );
    Closure* loadr = tvGetObj( ref(&loadrArg) );
    
    expectArg( trans, OBJ_CLS );
    Closure* trans = tvGetObj( ref(&transArg) );
    
    libLoader( state, type, loadr, trans );
    
    statePush( state, 0 );
}

static void
logFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var valArg = { .tup = args, .loc = 0 };
    
    ten_Tup retTup = ten_pushA( (ten_State*)state, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    ref(&retVar) = libLog( state, ref(&valArg) );
}

static void
intFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var valArg = { .tup = args, .loc = 0 };
    
    ten_Tup retTup = ten_pushA( (ten_State*)state, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    ref(&retVar) = libInt( state, ref(&valArg) );
}

static void
decFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var valArg = { .tup = args, .loc = 0 };
    
    ten_Tup retTup = ten_pushA( (ten_State*)state, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    ref(&retVar) = libDec( state, ref(&valArg) );
}

static void
symFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var valArg = { .tup = args, .loc = 0 };
    
    ten_Tup retTup = ten_pushA( (ten_State*)state, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    ref(&retVar) = libSym( state, ref(&valArg) );
}

static void
strFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var valArg = { .tup = args, .loc = 0 };
    
    ten_Tup retTup = ten_pushA( (ten_State*)state, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    ref(&retVar) = libStr( state, ref(&valArg) );
}

static void
showFun( ten_PARAMS ) {
    State* state = (State*)ten;
    ten_Var valsArg = { .tup = args, .loc = 0 };
    
    tenAssert( tvIsObjType( ref(&valsArg), OBJ_REC ) );
    
    libShow( state, tvGetObj( ref(&valsArg) ) );
    
    statePush( state, 0 );
}

static void
bcmpFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var str1Arg = { .tup = args, .loc = 0 };
    ten_Var oprArg  = { .tup = args, .loc = 1 };
    ten_Var str2Arg = { .tup = args, .loc = 2 };
    
    expectArg( str1, OBJ_STR );
    expectArg( opr, VAL_SYM );
    expectArg( str2, OBJ_STR );
    
    String* str1 = tvGetObj( ref(&str1Arg) );
    SymT    opr  = tvGetSym( ref(&oprArg) );
    String* str2 = tvGetObj( ref(&str2Arg) );
    
    TVal r = libBcmp( state, str1, opr, str2 );
    
    ten_pushA( ten, "V", stateTmp( state, r ) );
}

void
libInit( State* state ) {
    ten_State* s = (ten_State*)state;
    
    Part libP;
    LibState* lib = stateAllocRaw( state, &libP, sizeof(LibState) );
    lib->val1 = tvUdf();
    lib->val2 = tvUdf();
    lib->loaders     = NULL;
    lib->translators = NULL;
    lib->modules     = NULL;
    
    for( uint i = 0 ; i < IDENT_LAST ; i++ )
        lib->idents[i] = symGet( state, "", 0 );
    for( uint i = 0 ; i < OPER_LAST ; i++ )
        lib->opers[i] = symGet( state, "", 0 );
    for( uint i = 0 ; i < OBJ_LAST ; i++ )
        lib->types[i] = symGet( state, "", 0 );
    
    lib->scan.cb = libScan; stateInstallScanner( state, &lib->scan );
    lib->finl.cb = libFinl; stateInstallFinalizer( state, &lib->finl );
    
    stateCommitRaw( state, &libP );
    
    ten_Tup varTup = ten_pushA( (ten_State*)state, "UUU" );
    ten_Var idxVar = { .tup = &varTup, .loc = 0 };
    ten_Var funVar = { .tup = &varTup, .loc = 1 };
    ten_Var clsVar = { .tup = &varTup, .loc = 2 };
    
    
    #define IDENT( I ) \
        lib->idents[IDENT_ ## I] = symGet( state, #I, sizeof(#I)-1 )
    
    IDENT( require );
    IDENT( import );
    IDENT( type );
    IDENT( panic );
    IDENT( assert );
    IDENT( expect );
    IDENT( collect );
    IDENT( loader );
    
    IDENT( log );
    IDENT( int );
    IDENT( dec );
    IDENT( sym );
    IDENT( str );
    
    IDENT( keys );
    IDENT( vals );
    IDENT( pairs );
    IDENT( stream );
    IDENT( bytes );
    IDENT( chars );
    IDENT( items );
    
    IDENT( show );
    IDENT( warn );
    IDENT( input );
    
    IDENT( T );
    IDENT( N );
    IDENT( R );
    IDENT( L );
    IDENT( A );
    IDENT( Q );
    IDENT( Z );
    
    IDENT( ucode );
    IDENT( uchar );
    
    IDENT( cat );
    IDENT( join );
    
    IDENT( each );
    IDENT( fold );
    IDENT( while );
    IDENT( until );
    
    IDENT( expand );
    IDENT( cons );
    IDENT( list );
    
    IDENT( fiber );
    IDENT( cont );
    IDENT( yield );
    IDENT( status );
    
    IDENT( tag );
    

    #define OPER( N, O ) \
        lib->opers[OPER_ ## N] = symGet( state, O, sizeof(O)-1 )
    
    OPER( ILT, "<" );
    OPER( IMT, ">" );
    OPER( IET, "=" );
    OPER( ILE, "<=" );
    OPER( IME, ">=" );
    OPER( NET, "~=" );
    
    #define TYPE( T, N ) \
        lib->types[T] = symGet( state, #N, sizeof(#N)-1 )
    
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
    
    
    #define FUN( N, P, V )                                          \
    do {                                                            \
        Index* idx = NULL;                                          \
        if( (V) ) {                                                 \
            idx = idxNew( state );                                  \
            ref(&idxVar) = tvObj( idx );                            \
        }                                                           \
                                                                    \
        Function* fun = funNewNat( state, (P), idx, N ## Fun );     \
        fun->u.nat.name = symGet( state, #N, sizeof(#N)-1 );        \
        ref(&funVar) = tvObj( fun );                                \
                                                                    \
        Closure* cls = clsNewNat( state, fun, NULL );               \
        ref(&clsVar) = tvObj( cls );                                \
                                                                    \
        ten_def( s, ten_sym( s, #N ), &clsVar );                    \
    } while( 0 )
    
    FUN( require, 1, false );
    FUN( import, 1, false );
    FUN( type, 1, false );
    FUN( panic, 1, false );
    FUN( assert, 2, false );
    FUN( expect, 3, false );
    FUN( collect, 0, false );
    FUN( loader, 3, false );
    FUN( log, 1, false );
    FUN( int, 1, false );
    FUN( dec, 1, false );
    FUN( sym, 1, false );
    FUN( str, 1, false );
    FUN( show, 0, true );
    FUN( bcmp, 3, false );
    
    ten_def( s, ten_sym( s, "N" ), ten_sym( s, "\n" ) );
    
    statePop( state ); // varTup
    
    Index* importIdx = idxNew( state );
    lib->val1 = tvObj( importIdx );
    
    lib->loaders     = recNew( state, importIdx );
    lib->translators = recNew( state, importIdx );
    
    Index* moduleIdx = idxNew( state );
    lib->val1 = tvObj( moduleIdx );
    
    lib->modules = recNew( state, moduleIdx );
    
    
    state->libState = lib;
}

#ifdef ten_TEST
void
libTest( State* state ) {
    // TODO
}
#endif
