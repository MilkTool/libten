#include "ten_lib.h"
#include "ten.h"
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
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>

// For detecting UTF-8 character type.
#define isSingleChr( c ) ( (unsigned char)(c) >> 7 == 0  )
#define isDoubleChr( c ) ( (unsigned char)(c) >> 5 == 6  )
#define isTripleChr( c ) ( (unsigned char)(c) >> 4 == 14 )
#define isQuadChr( c )   ( (unsigned char)(c) >> 3 == 30 )
#define isAfterChr( c )  ( (unsigned char)(c) >> 6 == 2  )

// UTF-8 character ranges.
#define SINGLE_END 0x80L
#define DOUBLE_END 0x800L
#define TRIPLE_END 0x10000L
#define QUAD_END   0x10FFFL

typedef enum {
    IDENT_require,
    IDENT_import,
    IDENT_type,
    IDENT_panic,
    IDENT_assert,
    IDENT_expect,
    IDENT_collect,
    IDENT_loader,
    IDENT_clock,
    IDENT_rand,
    
    IDENT_log,
    IDENT_int,
    IDENT_dec,
    IDENT_sym,
    IDENT_str,
    
    IDENT_hex,
    IDENT_oct,
    IDENT_bin,
    
    IDENT_keys,
    IDENT_vals,
    IDENT_pairs,
    IDENT_seq,
    IDENT_rseq,
    IDENT_bytes,
    IDENT_chars,
    IDENT_split,
    IDENT_items,
    IDENT_drange,
    IDENT_irange,
    
    IDENT_show,
    IDENT_warn,
    IDENT_input,
    
    IDENT_ucode,
    IDENT_uchar,
    
    IDENT_cat,
    IDENT_join,
    IDENT_bcmp,
    IDENT_ccmp,
    IDENT_bsub,
    IDENT_csub,
    IDENT_blen,
    IDENT_clen,
    
    IDENT_each,
    IDENT_fold,
    
    IDENT_sep,
    
    IDENT_cons,
    IDENT_list,
    IDENT_explode,
    
    IDENT_fiber,
    IDENT_cont,
    IDENT_yield,
    IDENT_state,
    IDENT_errval,
    IDENT_trace,
    
    IDENT_script,
    IDENT_expr,
    
    IDENT_tag,
    IDENT_car,
    IDENT_cdr,
    
    IDENT_unit,
    IDENT_file,
    IDENT_line,
    
    IDENT_running,
    IDENT_waiting,
    IDENT_stopped,
    IDENT_finished,
    IDENT_failed,
    
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
    
    Index* cellIdx;
    Index* traceIdx;
    
    Record* loaders;
    Record* translators;
    Record* modules;
    
    SymT idents[IDENT_LAST];
    SymT opers[OPER_LAST];
    SymT types[OBJ_LAST];
    
    ten_DatInfo* recIterInfo;
    ten_DatInfo* strIterInfo;
    ten_DatInfo* splitIterInfo;
    ten_DatInfo* seqInfo;
    ten_DatInfo* listIterInfo;
    ten_DatInfo* dRangeInfo;
    ten_DatInfo* iRangeInfo;
};

static void
libScan( State* state, Scanner* scan ) {
    LibState* lib = structFromScan( LibState, scan );
    
    tvMark( lib->val1 );
    tvMark( lib->val2 );
    
    if( lib->cellIdx )
        stateMark (state, lib->cellIdx );
    if( lib->traceIdx )
        stateMark (state, lib->traceIdx );
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
        if( tvIsUdf( ret ) )
            return tvUdf();
        if( !tvIsObjType( ret, OBJ_STR ) )
            panic( "Import translator return is not Str or Udf" );
        
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
            return tvGetSym( info->typeVal );
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
        return tvGetSym( dat->info->typeVal );
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
        if( info && type == tvGetSym( info->typeVal ) )
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
        if( type == tvGetSym( dat->info->typeVal ) )
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


DecT
libClock( State* state ) {
    return (double)clock()/CLOCKS_PER_SEC;
}


DecT
libRand( State* state ) {
    return (double)rand()/INT_MAX;
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

TVal
libHex( State* state, String* str ) {
    char const* chr = str->buf;
    char const* end = str->buf + str->len;
    
    double val = 0.0;
    while( chr != end ) {
        if( *chr == '.' )
            break;
        
        val *= 16.0;
        if( *chr >= '0' && *chr <= '9' )
            val += *chr - '0';
        else
        if( *chr >= 'a' && *chr <= 'f' )
            val += 10 + *chr - 'a';
        else
        if( *chr >= 'A' && *chr <= 'F' )
            val += 10 + *chr - 'A';
        else
            return tvUdf();
        
        chr++;
    }
    if( chr == end ) {
        if( val > INT32_MAX || val < INT32_MIN )
            panic( "Number is too large" );
        return tvInt( (IntT)val );
    }
    
    chr++;
    
    double mul = 1.0;
    while( chr != end ) {
        mul /= 16.0;
        if( *chr >= '0' && *chr <= '9' )
            val += (*chr - '0')*mul;
        else
        if( *chr >= 'a' && *chr <= 'f' )
            val += (10 + *chr - 'a')*mul;
        else
        if( *chr >= 'A' && *chr <= 'F' )
            val += (10 + *chr - 'A')*mul;
        else
            return tvUdf();
        
        chr++;
    }
    
    return tvDec( val );
}

TVal
libOct( State* state, String* str ) {
    char const* chr = str->buf;
    char const* end = str->buf + str->len;
    
    double val = 0.0;
    while( chr != end ) {
        if( *chr == '.' )
            break;
        
        val *= 8.0;
        if( *chr >= '0' && *chr <= '7' )
            val += *chr - '0';
        else
            return tvUdf();
        
        chr++;
    }
    if( chr == end ) {
        if( val > INT32_MAX || val < INT32_MIN )
            panic( "Number is too large" );
        return tvInt( (IntT)val );
    }
    
    chr++;
    
    double mul = 1.0;
    while( chr != end ) {
        mul /= 8.0;
        if( *chr >= '0' && *chr <= '9' )
            val += (*chr - '0')*mul;
        else
            return tvUdf();
        
        chr++;
    }
    
    return tvDec( val );
}

TVal
libBin( State* state, String* str ) {
    char const* chr = str->buf;
    char const* end = str->buf + str->len;
    
    double val = 0.0;
    while( chr != end ) {
        if( *chr == '.' )
            break;
        
        val *= 2.0;
        if( *chr >= '0' && *chr <= '1' )
            val += *chr - '0';
        else
            return tvUdf();
        
        chr++;
    }
    if( chr == end ) {
        if( val > INT32_MAX || val < INT32_MIN )
            panic( "Number is too large" );
        return tvInt( (IntT)val );
    }
    
    chr++;
    
    double mul = 1.0;
    while( chr != end ) {
        mul /= 2.0;
        if( *chr >= '0' && *chr <= '1' )
            val += (*chr - '0')*mul;
        else
            return tvUdf();
        
        chr++;
    }
    
    return tvDec( val );
}

typedef struct {
    State*   state;
    IdxIter* iter;
} RecIter;

typedef enum {
    RecIter_REC,
    RecIter_LAST
} RecIterMem;

static void
recIterDestr( void* dat ) {
    RecIter* iter = dat;
    if( iter->iter )
        idxIterFree( iter->state, iter->iter );
}

static ten_Tup
keyIterNext( ten_PARAMS ) {
    State*   state = (State*)ten;
    RecIter* iter  = dat;
    
    ten_Tup retTup = ten_pushA( ten, "N" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    if( !iter->iter )
        return retTup;
    
    ten_Var recVar = {.tup = mems, .loc = RecIter_REC };
    Record* rec  = tvGetObj( vget( recVar ) );
    uint    cap  = rec->cap;
    TVal*   vals = rec->vals;
    
    TVal key;
    uint loc;
    
    loop: {
        bool has = idxIterNext( (State*)ten, iter->iter, &key, &loc );
        if( !has ) {
            idxIterFree( state, iter->iter );
            iter->iter = NULL;
            return retTup;
        }
        
        if( loc >= cap )
            goto loop;
        if( tvIsUdf( vals[loc] ) )
            goto loop;
    }
    
    vset( retVar, key );
    return retTup;
}

Closure*
libKeys( State* state, Record* rec ) {
    LibState*  lib = state->libState;
    ten_State* ten = (ten_State*)state;
    
    ten_Tup varTup = ten_pushA( ten, "UUUU" );
    ten_Var recVar = { .tup = &varTup, .loc = 0 };
    ten_Var datVar = { .tup = &varTup, .loc = 1 };
    ten_Var funVar = { .tup = &varTup, .loc = 2 };
    ten_Var clsVar = { .tup = &varTup, .loc = 3 };
    
    RecIter* iter = ten_newDat( ten, lib->recIterInfo, &datVar );
    iter->iter  = idxIterMake( state, tpGetPtr( rec->idx ) );
    iter->state = state;
    
    vset( recVar, tvObj( rec ) );
    ten_setMember( ten, &datVar, RecIter_REC, &recVar );
    
    ten_FunParams p = {
        .name   = fmtA( state, false, "keys#%llu", (ullong)(uintptr_t)iter ),
        .params = NULL,
        .cb     = keyIterNext
    };
    ten_newFun( ten, &p, &funVar );
    ten_newCls( ten, &funVar, &datVar, &clsVar );
    
    Closure* cls = tvGetObj( vget( clsVar ) );
    ten_pop( ten );
    
    return cls;
}

static ten_Tup
valIterNext( ten_PARAMS ) {
    State*   state = (State*)ten;
    RecIter* iter  = dat;
    
    ten_Tup retTup = ten_pushA( ten, "N" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    if( !iter->iter )
        return retTup;
    
    ten_Var recVar = {.tup = mems, .loc = RecIter_REC };
    Record* rec  = tvGetObj( vget( recVar ) );
    uint    cap  = rec->cap;
    TVal*   vals = rec->vals;
    
    TVal key;
    uint loc;

    loop: {
        bool has = idxIterNext( (State*)ten, iter->iter, &key, &loc );
        if( !has ) {
            idxIterFree( state, iter->iter );
            iter->iter = NULL;
            return retTup;
        }
        
        if( loc >= cap )
            goto loop;
        if( tvIsUdf( vals[loc] ) )
            goto loop;
    }
    
    vset( retVar, vals[loc] );
    return retTup;
}

Closure*
libVals( State* state, Record* rec ) {
    LibState*  lib = state->libState;
    ten_State* ten = (ten_State*)state;
    
    ten_Tup varTup = ten_pushA( ten, "UUUU" );
    ten_Var recVar = { .tup = &varTup, .loc = 0 };
    ten_Var datVar = { .tup = &varTup, .loc = 1 };
    ten_Var funVar = { .tup = &varTup, .loc = 2 };
    ten_Var clsVar = { .tup = &varTup, .loc = 3 };
    
    RecIter* iter = ten_newDat( ten, lib->recIterInfo, &datVar );
    iter->iter  = idxIterMake( state, tpGetPtr( rec->idx ) );
    iter->state = state;
    
    vset( recVar, tvObj( rec ) );
    ten_setMember( ten, &datVar, RecIter_REC, &recVar );
    
    ten_FunParams p = {
        .name   = fmtA( state, false, "vals#%llu", (ullong)(uintptr_t)iter ),
        .params = NULL,
        .cb     = valIterNext
    };
    ten_newFun( ten, &p, &funVar );
    ten_newCls( ten, &funVar, &datVar, &clsVar );
    
    Closure* cls = tvGetObj( vget( clsVar ) );
    ten_pop( ten );
    
    return cls;
}

static ten_Tup
pairIterNext( ten_PARAMS ) {
    State*   state = (State*)ten;
    RecIter* iter  = dat;
    
    ten_Tup retTup = ten_pushA( ten, "NN" );
    ten_Var keyVar = { .tup = &retTup, .loc = 0 };
    ten_Var valVar = { .tup = &retTup, .loc = 1 };
    if( !iter->iter )
        return retTup;
    
    ten_Var recVar = {.tup = mems, .loc = RecIter_REC };
    Record* rec  = tvGetObj( vget( recVar ) );
    uint    cap  = rec->cap;
    TVal*   vals = rec->vals;
    
    TVal key;
    uint loc;

    loop: {
        bool has = idxIterNext( (State*)ten, iter->iter, &key, &loc );
        if( !has ) {
            idxIterFree( state, iter->iter );
            iter->iter = NULL;
            return retTup;
        }
        
        if( loc >= cap )
            goto loop;
        if( tvIsUdf( vals[loc] ) )
            goto loop;
    }
    
    vset( keyVar, key );
    vset( valVar, vals[loc] );
    return retTup;
}

Closure*
libPairs( State* state, Record* rec ) {
    LibState*  lib = state->libState;
    ten_State* ten = (ten_State*)state;
    
    ten_Tup varTup = ten_pushA( ten, "UUUU" );
    ten_Var recVar = { .tup = &varTup, .loc = 0 };
    ten_Var datVar = { .tup = &varTup, .loc = 1 };
    ten_Var funVar = { .tup = &varTup, .loc = 2 };
    ten_Var clsVar = { .tup = &varTup, .loc = 3 };
    
    RecIter* iter = ten_newDat( ten, lib->recIterInfo, &datVar );
    iter->iter  = idxIterMake( state, tpGetPtr( rec->idx ) );
    iter->state = state;
    
    vset( recVar, tvObj( rec ) );
    ten_setMember( ten, &datVar, RecIter_REC, &recVar );
    
    ten_FunParams p = {
        .name   = fmtA( state, false, "pairs#%llu", (ullong)(uintptr_t)iter ),
        .params = NULL,
        .cb     = pairIterNext
    };
    ten_newFun( ten, &p, &funVar );
    ten_newCls( ten, &funVar, &datVar, &clsVar );
    
    Closure* cls = tvGetObj( vget( clsVar ) );
    ten_pop( ten );
    
    return cls;
}


typedef struct {
    llong next;
} Seq;

typedef enum {
    Seq_VALS,
    Seq_LAST
} SeqMem;

static ten_Tup
seqNext( ten_PARAMS ) {
    State* state = (State*)ten;
    Seq*   seq   = dat;
    
    ten_Tup retTup = ten_pushA( ten, "N" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    if( seq->next < 0 )
        return retTup;
    
    ten_Var valsVar = {.tup = mems, .loc = Seq_VALS };
    Record* vals = tvGetObj( vget( valsVar ) );
    
    TVal next = recGet( (State*)ten, vals, tvInt( seq->next++ ) );
    if( tvIsUdf( next ) )
        seq->next = -1;
    else
        vset( retVar, next );
    
    return retTup;
}

Closure*
libSeq( State* state, Record* vals ) {
    LibState*  lib = state->libState;
    ten_State* ten = (ten_State*)state;
    
    ten_Tup varTup = ten_pushA( ten, "UUUU" );
    ten_Var valsVar = { .tup = &varTup, .loc = 0 };
    ten_Var datVar  = { .tup = &varTup, .loc = 1 };
    ten_Var funVar  = { .tup = &varTup, .loc = 2 };
    ten_Var clsVar  = { .tup = &varTup, .loc = 3 };
    
    Seq* seq = ten_newDat( ten, lib->seqInfo, &datVar );
    seq->next = 0;
    
    vset( valsVar, tvObj( vals ) );
    ten_setMember( ten, &datVar, Seq_VALS, &valsVar );
    
    ten_FunParams p = {
        .name   = fmtA( state, false, "seq#%llu", (ullong)(uintptr_t)seq ),
        .params = NULL,
        .cb     = seqNext
    };
    ten_newFun( ten, &p, &funVar );
    ten_newCls( ten, &funVar, &datVar, &clsVar );
    
    Closure* cls = tvGetObj( vget( clsVar ) );
    ten_pop( ten );
    
    return cls;
}

typedef struct {
    llong loc;
} StrIter;

typedef enum {
    StrIter_STR,
    StrIter_LAST
} StrIterMem;

static ten_Tup
byteIterNext( ten_PARAMS ) {
    State*   state = (State*)ten;
    StrIter* iter  = dat;
    
    ten_Tup retTup = ten_pushA( ten, "N" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    if( iter->loc < 0 )
        return retTup;
    
    ten_Var strVar = {.tup = mems, .loc = StrIter_STR };
    String* str = tvGetObj( vget( strVar ) );
    
    if( iter->loc < str->len )
        vset( retVar, tvInt( str->buf[iter->loc++] ) );
    else
        iter->loc = -1;
    
    return retTup;
}

Closure*
libBytes( State* state, String* str ) {
    LibState*  lib = state->libState;
    ten_State* ten = (ten_State*)state;
    
    ten_Tup varTup = ten_pushA( ten, "UUUU" );
    ten_Var strVar = { .tup = &varTup, .loc = 0 };
    ten_Var datVar = { .tup = &varTup, .loc = 1 };
    ten_Var funVar = { .tup = &varTup, .loc = 2 };
    ten_Var clsVar = { .tup = &varTup, .loc = 3 };
    
    StrIter* iter = ten_newDat( ten, lib->strIterInfo, &datVar );
    iter->loc = 0;
    
    vset( strVar, tvObj( str ) );
    ten_setMember( ten, &datVar, StrIter_STR, &strVar );
    
    ten_FunParams p = {
        .name   = fmtA( state, false, "bytes#%llu", (ullong)(uintptr_t)iter ),
        .params = NULL,
        .cb     = byteIterNext
    };
    ten_newFun( ten, &p, &funVar );
    ten_newCls( ten, &funVar, &datVar, &clsVar );
    
    Closure* cls = tvGetObj( vget( clsVar ) );
    ten_pop( ten );
    
    return cls;
}

static inline void
cnext( State* state, char const** str, size_t* len, SymT* next ) {
    uint n = 0;
    if( isSingleChr( (*str)[0] ) ) {
        if( *len < 1 )
            goto fail;
        n = 1;
    }
    
    if( isDoubleChr( (*str)[0] ) ) {
        if( *len < 2 )
            goto fail;
        n = 2;
    }
    else
    if( isTripleChr( (*str)[0] ) ) {
        if( *len < 3 )
            goto fail;
        n = 3;
    }
    else
    if( isQuadChr( (*str)[0] ) ) {
        if( *len < 4 )
            goto fail;
        n = 4;
    }
    
    *next = symGet( state, *str, n );
    *str += n;
    *len -= n;
    return;
    
    fail: panic( "Format is not UTF-8" );
}

static ten_Tup
charIterNext( ten_PARAMS ) {
    State*   state = (State*)ten;
    StrIter* iter  = dat;
    
    ten_Tup retTup = ten_pushA( ten, "N" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    if( iter->loc < 0 )
        return retTup;
    
    ten_Var strVar = {.tup = mems, .loc = StrIter_STR };
    String* str = tvGetObj( vget( strVar ) );
    
    if( iter->loc < str->len ) {
        char const* buf = str->buf + iter->loc;
        size_t      len = str->len - iter->loc;
        SymT        chr = 0;
        cnext( (State*)ten, &buf, &len, &chr );
        iter->loc = buf - str->buf;
        vset(retVar, tvSym( chr ) );
    }
    else {
        iter->loc = -1;
    }
    return retTup;
}

Closure*
libChars( State* state, String* str ) {
    LibState*  lib = state->libState;
    ten_State* ten = (ten_State*)state;
    
    ten_Tup varTup = ten_pushA( ten, "UUUU" );
    ten_Var strVar = { .tup = &varTup, .loc = 0 };
    ten_Var datVar = { .tup = &varTup, .loc = 1 };
    ten_Var funVar = { .tup = &varTup, .loc = 2 };
    ten_Var clsVar = { .tup = &varTup, .loc = 3 };
    
    StrIter* iter = ten_newDat( ten, lib->strIterInfo, &datVar );
    iter->loc = 0;
    
    vset( strVar, tvObj( str ) );
    ten_setMember( ten, &datVar, StrIter_STR, &strVar );
    
    ten_FunParams p = {
        .name   = fmtA( state, false, "chars#%llu", (ullong)(uintptr_t)iter ),
        .params = NULL,
        .cb     = charIterNext
    };
    ten_newFun( ten, &p, &funVar );
    ten_newCls( ten, &funVar, &datVar, &clsVar );
    
    Closure* cls = tvGetObj( vget( clsVar ) );
    ten_pop( ten );
    
    return cls;
}

typedef struct {
    char const* loc;
} SplitIter;

typedef enum {
    SplitIter_STR,
    SplitIter_SEP,
    SplitIter_LAST
} SplitIterMem;

static ten_Tup
splitIterNext( ten_PARAMS ) {
    State*     state = (State*)ten;
    SplitIter* iter  = dat;
    
    ten_Tup retTup = ten_pushA( ten, "N" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    if( iter->loc == NULL )
        return retTup;
    
    ten_Var strVar = { .tup = mems, .loc = SplitIter_STR };
    ten_Var sepVar = { .tup = mems, .loc = SplitIter_SEP };
    String* str = tvGetObj( vget( strVar ) );
    String* sep = tvGetObj( vget( sepVar ) );
    
    char const* loc = iter->loc;
    char const* nxt = loc;
    char const* end = str->buf + str->len;
    while( nxt + sep->len < end ) {
        if( !memcmp( nxt, sep->buf, sep->len ) ) {
            iter->loc = nxt + sep->len;
            vset( retVar, tvObj( strNew( state, loc, nxt - loc ) ) );
            return retTup;
        }
        nxt++;
    }
    
    iter->loc = NULL;
    return retTup;
}

Closure*
libSplit( State* state, String* str, String* sep ) {
    LibState*  lib = state->libState;
    ten_State* ten = (ten_State*)state;
    
    ten_Tup varTup = ten_pushA( ten, "UUUUU" );
    ten_Var strVar = { .tup = &varTup, .loc = 0 };
    ten_Var sepVar = { .tup = &varTup, .loc = 1 };
    ten_Var datVar = { .tup = &varTup, .loc = 2 };
    ten_Var funVar = { .tup = &varTup, .loc = 3 };
    ten_Var clsVar = { .tup = &varTup, .loc = 4 };
    
    SplitIter* iter = ten_newDat( ten, lib->splitIterInfo, &datVar );
    iter->loc = str->buf;
    
    vset( strVar, tvObj( str ) );
    vset( sepVar, tvObj( sep ) );
    ten_setMember( ten, &datVar, SplitIter_STR, &strVar );
    ten_setMember( ten, &datVar, SplitIter_SEP, &sepVar );
    
    ten_FunParams p = {
        .name   = fmtA( state, false, "split#%llu", (ullong)(uintptr_t)iter ),
        .params = NULL,
        .cb     = splitIterNext
    };
    ten_newFun( ten, &p, &funVar );
    ten_newCls( ten, &funVar, &datVar, &clsVar );
    
    Closure* cls = tvGetObj( vget( clsVar ) );
    ten_pop( ten );
    
    return cls;
}

typedef struct {
    bool finished;
} ListIter;

typedef enum {
    ListIter_CELL,
    ListIter_LAST
} ListIterMem;

static ten_Tup
listIterNext( ten_PARAMS ) {
    State*    state = (State*)ten;
    LibState* lib   = state->libState;
    
    ListIter* iter = dat;
    
    ten_Tup retTup = ten_pushA( ten, "N" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    if( iter->finished )
        return retTup;
    
    ten_Var cellVar = {.tup = mems, .loc = ListIter_CELL };
    Record* cell = tvGetObj( vget( cellVar ) );
    
    TVal car = recGet( state, cell, tvSym( lib->idents[IDENT_car] ) );
    TVal cdr = recGet( state, cell, tvSym( lib->idents[IDENT_cdr] ) );
    if( tvIsNil( cdr ) ) {
        iter->finished = true;
    }
    else
    if( !tvIsObjType( cdr, OBJ_REC ) ) {
        panic( "Iteration over malformed list" );
    }
    
    vset( cellVar, cdr );
    vset( retVar, car );
    return retTup;
}

Closure*
libItems( State* state, Record* list ) {
    LibState*  lib = state->libState;
    ten_State* ten = (ten_State*)state;
    
    ten_Tup varTup = ten_pushA( ten, "UUUU" );
    ten_Var listVar = { .tup = &varTup, .loc = 0 };
    ten_Var datVar  = { .tup = &varTup, .loc = 1 };
    ten_Var funVar  = { .tup = &varTup, .loc = 2 };
    ten_Var clsVar  = { .tup = &varTup, .loc = 3 };
    
    ListIter* iter = ten_newDat( ten, lib->listIterInfo, &datVar );
    iter->finished = false;
    
    vset( listVar, tvObj( list ) );
    ten_setMember( ten, &datVar, ListIter_CELL, &listVar );
    
    ten_FunParams p = {
        .name   = fmtA( state, false, "items#%llu", (ullong)(uintptr_t)iter ),
        .params = NULL,
        .cb     = listIterNext
    };
    ten_newFun( ten, &p, &funVar );
    ten_newCls( ten, &funVar, &datVar, &clsVar );
    
    Closure* cls = tvGetObj( vget( clsVar ) );
    ten_pop( ten );
    
    return cls;
}


typedef struct {
   DecT start;
   DecT end;
   DecT step; 
   DecT next;
} DRange;

static ten_Tup
dRangeNext( ten_PARAMS ) {
    State*    state = (State*)ten;
    LibState* lib   = state->libState;
    
    DRange* range = dat;
    
    ten_Tup retTup = ten_pushA( ten, "N" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    DecT start = range->start;
    DecT end   = range->end;
    DecT next  = range->next;
    if( (end < start && next <= end) || ( end > start && next >= end ) )
        return retTup;
    
    range->next += range->step;
    vset( retVar, tvDec( next ) );
    
    return retTup;
}

Closure*
libDrange( State* state, DecT start, DecT end, DecT step ) {
    LibState*  lib = state->libState;
    ten_State* ten = (ten_State*)state;
    
    ten_Tup varTup = ten_pushA( ten, "UUU" );
    ten_Var datVar  = { .tup = &varTup, .loc = 0 };
    ten_Var funVar  = { .tup = &varTup, .loc = 1 };
    ten_Var clsVar  = { .tup = &varTup, .loc = 2 };
    
    if( (end < start && step >= 0.0) || ( end > start && step <= 0.0 ) )
        panic( "Range does not progress" );
    
    DRange* range = ten_newDat( ten, lib->dRangeInfo, &datVar );
    range->start = start;
    range->end   = end;
    range->step  = step;
    range->next  = start;
    
    ten_FunParams p = {
        .name   = fmtA( state, false, "drange#%llu", (ullong)(uintptr_t)range ),
        .params = NULL,
        .cb     = dRangeNext
    };
    ten_newFun( ten, &p, &funVar );
    ten_newCls( ten, &funVar, &datVar, &clsVar );
    
    Closure* cls = tvGetObj( vget( clsVar ) );
    ten_pop( ten );
    
    return cls;
}

typedef struct {
    IntT start;
    IntT end;
    IntT step;
    IntT next;
} IRange;


static ten_Tup
iRangeNext( ten_PARAMS ) {
    State*    state = (State*)ten;
    LibState* lib   = state->libState;
    
    IRange* range = dat;
    
    ten_Tup retTup = ten_pushA( ten, "N" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    IntT start = range->start;
    IntT end   = range->end;
    IntT next  = range->next;
    if( (end < start && next <= end) || ( end > start && next >= end ) )
        return retTup;
    
    range->next += range->step;
    vset( retVar, tvInt( next ) );
    
    return retTup;
}

Closure*
libIrange( State* state, IntT start, IntT end, IntT step ) {
    LibState*  lib = state->libState;
    ten_State* ten = (ten_State*)state;
    
    ten_Tup varTup = ten_pushA( ten, "UUU" );
    ten_Var datVar  = { .tup = &varTup, .loc = 0 };
    ten_Var funVar  = { .tup = &varTup, .loc = 1 };
    ten_Var clsVar  = { .tup = &varTup, .loc = 2 };
    
    if( (end < start && step >= 0.0) || ( end > start && step <= 0.0 ) )
        panic( "Range does not progress" );
    
    IRange* range = ten_newDat( ten, lib->iRangeInfo, &datVar );
    range->start = start;
    range->end   = end;
    range->step  = step;
    range->next  = start;
    
    ten_FunParams p = {
        .name   = fmtA( state, false, "irange#%llu", (ullong)(uintptr_t)range ),
        .params = NULL,
        .cb     = iRangeNext
    };
    ten_newFun( ten, &p, &funVar );
    ten_newCls( ten, &funVar, &datVar, &clsVar );
    
    Closure* cls = tvGetObj( vget( clsVar ) );
    ten_pop( ten );
    
    return cls;
}

void
libShow( State* state, Record* vals ) {
    fmtA( state, false, "" );
    
    uint  i = 0;
    TVal  v = recGet( state, vals, tvInt( i++ ) );
    while( !tvIsUdf( v ) ) {
        fmtA( state, true, "%v", v );
        v = recGet( state, vals, tvInt( i++ ) );
    }
    fwrite( fmtBuf( state ), 1, fmtLen( state ), stdout );
}

void
libWarn( State* state, Record* vals ) {
    fmtA( state, false, "" );
    
    uint  i = 0;
    TVal  v = recGet( state, vals, tvInt( i++ ) );
    while( !tvIsUdf( v ) ) {
        fmtA( state, true, "%v", v );
        v = recGet( state, vals, tvInt( i++ ) );
    }
    fwrite( fmtBuf( state ), 1, fmtLen( state ), stderr );
}

#define BUF_TYPE char
#define BUF_NAME CharBuf
    #include "inc/buf.inc"
#undef BUF_NAME
#undef BUF_TYPE

String*
libInput( State* state ) {
    CharBuf buf; initCharBuf( state, &buf );
    
    char next = getc( stdin );
    while( next != '\n' && next != '\r' ) {
        *putCharBuf( state, &buf ) = next;
        next = getc( stdin );
    }
    
    String* str = strNew( state, buf.buf, buf.top );
    finlCharBuf( state, &buf );
    
    return str;
}

static inline void
unext( State* state, char const** str, size_t* len, uint32_t* next ) {
    uint n = 0;
    if( isSingleChr( (*str)[0] ) ) {
        if( *len < 1 )
            goto fail;
        n = 1;
        *next = (*str)[0];
    }
    
    if( isDoubleChr( (*str)[0] ) ) {
        if( *len < 2 )
            goto fail;
        n = 2;
        *next = (*str)[0] & 0x1F;
    }
    else
    if( isTripleChr( (*str)[0] ) ) {
        if( *len < 3 )
            goto fail;
        n = 3;
        *next = (*str)[0] & 0xF;
    }
    else
    if( isQuadChr( (*str)[0] ) ) {
        if( *len < 4 )
            goto fail;
        n = 4;
        *next = (*str)[0] & 0x7;
    }
    
    for( uint i = 1 ; i < n ; i++ ) {
        *next <<= 6;
        *next |= (*str)[i] & 0x3F;
    }
    
    *str += n;
    *len -= n;
    return;
    
    fail: panic( "Format is not UTF-8" );
}

TVal
libUcode( State* state, SymT chr ) {
    size_t len = symLen( state, chr );
    if( len > 4 )
        return tvUdf();
    
    char buf[len];
    memcpy( buf, symBuf( state, chr ), len );
    
    char const* str  = buf;
    uint32_t    code = 0;
    unext( state, &str, &len, &code );
    
    return tvInt( code );
}

TVal
libUchar( State* state, IntT code ) {
    char     buf[4];
    size_t   len = 0;
    uint32_t u   = code;
    
    if( u < SINGLE_END ) {
        buf[0] = u;
        len = 1;
    }
    else
    if( u < DOUBLE_END ) {
        buf[0] = 6 << 5 | u >> 6;
        buf[1] = 2 << 6 | ( u & 63 );
        len = 2;
    }
    else
    if( u < TRIPLE_END ) {
        buf[0] = 14 << 4 | u >> 12;
        buf[1] =  2 << 6 | ( u >> 6 & 63 );
        buf[2] =  2 << 6 | ( u & 63 );
        len = 3;
    }
    else
    if( u < QUAD_END ) {
        buf[0] = 30 << 3 | u >> 18;
        buf[1] =  2 << 6 | ( u >> 12 & 63 );
        buf[2] =  2 << 6 | ( u >>  6 & 63 );
        buf[3] =  2 << 6 | ( u & 63 );
        len = 4;
    }
    else {
        return tvUdf();
    }
    
    return tvSym( symGet( state, buf, len ) );
}

String*
libCat( State* state, Record* vals ) {
    CharBuf buf; initCharBuf( state, &buf );
    
    uint  i = 0;
    TVal  v = recGet( state, vals, tvInt( i++ ) );
    while( !tvIsUdf( v ) ) {
        char const* str = fmtA( state, false, "%v", v );
        size_t      len = fmtLen( state );
        for( uint i = 0 ; i < len ; i++ )
            *putCharBuf( state, &buf ) = str[i];
        
        v = recGet( state, vals, tvInt( i++ ) );
    }
    
    String* str = strNew( state, buf.buf, buf.top );
    finlCharBuf( state, &buf );
    return str;
}

String*
libJoin( State* state, Closure* iter, String* sep ) {
    ten_State* ten = (ten_State*)state;
    
    CharBuf buf; initCharBuf( state, &buf );
    
    ten_Tup argTup = ten_pushA( ten, "" );
    ten_Tup retTup = ten_call( ten, stateTmp( state, tvObj( iter ) ), &argTup );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    if( ten_size( ten, &retTup ) != 1 )
        panic( "Iterator returned tuple" );
    
    while( !tvIsNil( vget( retVar ) ) ) {
        fmtA( state, false, "%v", vget( retVar ) );
        
        ten_pop( ten );
        retTup = ten_call( ten, stateTmp( state, tvObj( iter ) ), &argTup );
        
        char const* str = fmtBuf( state );
        size_t      len = fmtLen( state );
        if( !tvIsUdf( vget( retVar ) ) ) {
            str = fmtA( state, true, "%v", tvObj( sep ) );
            len = fmtLen( state );
        }
        
        for( uint i = 0 ; i < len ; i++ )
            *putCharBuf( state, &buf ) = str[i];
    }
    
    ten_pop( ten );
    ten_pop( ten );
    
    String* str = strNew( state, buf.buf, buf.top );
    finlCharBuf( state, &buf );
    return str;
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

static int
ucmp( State* state, char const* str1, size_t len1, char const* str2, size_t len2 ) {
    char const* end1  = str1 + len1;
    char const* end2  = str2 + len1;
    
    while( str1 < end1 && str2 < end2 ) {
        uint32_t char1 = 0;
        unext( state, &str1, &len1, &char1 );
        
        uint32_t char2 = 0;
        unext( state, &str2, &len2, &char2 );
        
        if( char1 < char2 )
            return -1;
        if( char1 > char2 )
            return 1;
    }
    
    if( str1 == end1  && str2 != end2 )
        return -1;
    if( str2 == end2 && str1 != end1 )
        return 1;
    
    return 0;
}

TVal
libCcmp( State* state, String* str1, SymT opr, String* str2 ) {
    LibState* lib = state->libState;
    
    size_t len = str2->len < str1->len ? str2->len : str1->len;
    int r = ucmp( state, str1->buf, str1->len, str2->buf, str2->len );
    
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

String*
libBsub( State* state, String* str, IntT n ) {
    char const* buf = str->buf;
    size_t      len = str->len;
    
    if( n >= 0 ) {
        if( n > len )
            panic( "Given 'n' is larger than string length" );
        return strNew( state, buf, n );
    }
    else {
        if( -n > len )
            panic( "Given 'n' is larger than string length" );
        return strNew( state, buf + len + n, -n );
    }
}

String*
libCsub( State* state, String* str, IntT n ) {
    char const* buf = str->buf;
    size_t      len = str->len;
    
    if( n >= 0 ) {
        char const* end  = buf;
        size_t      left = len;
        uint32_t    chr  = 0;
        while( n > 0 && left > 0 ) {
            n--;
            unext( state, &end, &left, &chr );
        }
        if( n > 0 )
            panic( "Given 'n' is larger than string length" );
        
        return strNew( state, buf, end - buf );
    }
    else {
        char const* start  = buf + len;
        while( start > buf && n < 0 ) {
            uint len = 1;
            while( start > buf && isAfterChr( *start ) ) {
                start--;
                len++;
            }
            if( len == 1 && !isSingleChr( *start ) )
                goto fail;
            if( len == 2 && !isDoubleChr( *start ) )
                goto fail;
            if( len == 3 && !isTripleChr( *start ) )
                goto fail;
            if( len == 4 && !isQuadChr( *start ) )
                goto fail;
            if( len < 1 || len > 4 )
                goto fail;
            
            start--;
            n++;
        }
        
        if( n < 0 )
            panic( "Given 'n' is larger than string length" );
        return strNew( state, start, (buf + len) - start );
    }
    
    fail: panic( "Format is not UTF-8" );
    return NULL;
}

size_t
libBlen( State* state, String* str ) {
    return str->len;
}

size_t
libClen( State* state, String* str ) {
    size_t count = 0;
    
    char const* end  = str->buf;
    size_t      left = str->len;
    uint32_t    chr  = 0;
    while( left > 0 ) {
        unext( state, &end, &left, &chr );
        count++;
    }
    
    return count;
}


void
libEach( State* state, Closure* iter, Closure* what ) {
    ten_State* ten = (ten_State*)state;
    
    
    ten_Tup sArgTup = ten_pushA( ten, "" );
    ten_Tup sRetTup = ten_call( ten, stateTmp( state, tvObj( iter ) ), &sArgTup );
    while( !ten_areNil( ten, &sRetTup ) ) {
        ten_call( ten, stateTmp( state, tvObj( what ) ), &sRetTup );
        ten_pop( ten );
        ten_pop( ten );
        sRetTup = ten_call( ten, stateTmp( state, tvObj( iter ) ), &sArgTup );
    }
    ten_pop( ten );
    ten_pop( ten );
}

TVal
libFold( State* state, Closure* iter, TVal agr, Closure* how ) {
    ten_State* ten = (ten_State*)state;
    
    ten_Tup hArgTup = ten_pushA( ten, "UU" );
    ten_Var hAgrArg = { .tup = &hArgTup, .loc = 0 };
    ten_Var hValArg = { .tup = &hArgTup, .loc = 1 };
    
    vset( hAgrArg, agr );
    
    ten_Tup sArgTup = ten_pushA( ten, "" );
    ten_Tup sRetTup = ten_call( ten, stateTmp( state, tvObj( iter ) ), &sArgTup );
    ten_Var sRetVar = { .tup = &sRetTup, .loc = 0 };
    while( !ten_areNil( ten, &sRetTup ) ) {
        if( ten_size( ten, &sRetTup ) != 1 )
            panic( "Iterator returned tuple" );
        
        vset(hValArg, vget( sRetVar ) );
        
        ten_Tup hRetTup = ten_call( ten, stateTmp( state, tvObj( how ) ), &hArgTup );
        ten_Var hRetVar = { .tup = &hRetTup, .loc = 0 };
        if( ten_size( ten, &hRetTup ) != 1 )
            panic( "Aggregator returned tuple" );
        
        vset( hAgrArg, vget( hRetVar ));
        
        ten_pop( ten );
        ten_pop( ten );
        sRetTup = ten_call( ten, stateTmp( state, tvObj( iter ) ), &sArgTup );
    }
    ten_pop( ten );
    ten_pop( ten );
    
    agr = vget( hAgrArg );
    ten_pop( ten );
    
    return agr;
}


Record*
libSep( State* state, Record* rec ) {
    recSep( state, rec );
    return rec;
}

Record*
libCons( State* state, TVal car, TVal cdr ) {
    ten_State* ten = (ten_State*)state;
    LibState*  lib = state->libState;
    
    ten_Tup varTup = ten_pushA( ten, "U" );
    ten_Var recVar = { .tup = &varTup, .loc = 0 };
    
    Record* rec = recNew( state, lib->cellIdx );
    vset( recVar, tvObj( rec ) );
    
    recDef( state, rec, tvSym( lib->idents[IDENT_car] ), car );
    recDef( state, rec, tvSym( lib->idents[IDENT_cdr] ), cdr );
    
    recSep( state, rec );
    
    ten_pop( ten );
    return rec;
}

Record*
libList( State* state, Record* vals ) {
    ten_State* ten = (ten_State*)state;
    LibState*  lib = state->libState;
    
    ten_Tup varTup  = ten_pushA( ten, "U" );
    ten_Var listVar = { .tup = &varTup, .loc = 0 };
    vset( listVar, tvNil() );
    
    uint  i = 0;
    TVal  v = recGet( state, vals, tvInt( i++ ) );
    while( !tvIsUdf( v ) ) {
        Record* cell = libCons( state, v, vget( listVar ) );
        vset( listVar, tvObj( cell ) );
        v = recGet( state, vals, tvInt( i++ ) );
    }
    
    Record* list = tvGetObj( vget( listVar ) );
    ten_pop( ten );
    
    return list;
}

Record*
libExplode( State* state, Closure* iter ) {
    ten_State* ten = (ten_State*)state;
    LibState*  lib = state->libState;
    
    ten_Tup varTup  = ten_pushA( ten, "U" );
    ten_Var listVar = { .tup = &varTup, .loc = 0 };
    vset( listVar, tvNil() );
    
    ten_Tup argTup = ten_pushA( ten, "" );
    ten_Tup retTup = ten_call( ten, stateTmp( state, tvObj( iter ) ), &argTup );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    while( !ten_areNil( ten, &retTup ) ) {
        if( ten_size( ten, &retTup ) != 1 )
            panic( "Iterator returned tuple" );
        
        Record* cell = libCons( state, vget( retVar ), vget( listVar ) );
        vset( listVar, tvObj( cell ) );
        
        ten_pop( ten );
        retTup = ten_call( ten, stateTmp( state, tvObj( iter ) ), &argTup );
    }
    
    ten_pop( ten );
    ten_pop( ten );
    
    Record* list = tvGetObj( vget( listVar ) );
    ten_pop( ten );
    
    return list;
}

Fiber*
libFiber( State* state, Closure* cls, SymT* tag ) {
    ten_State* ten = (ten_State*)state;
    return fibNew( state, cls, tag );
}

Tup
libCont( State* state, Fiber* fib, Record* args ) {
    uint count = 0;
    TVal val   = recGet( state, args, tvInt( count ) );
    while( !tvIsUdf( val ) ) {
        val = recGet( state, args, tvInt( ++count ) );
    }
    
    Tup argTup = statePush( state, count );
    for( uint i = 0 ; i < count ; i++ ) {
        val = recGet( state, args, tvInt( i ) );
        tupAt( argTup, i ) = val;
    }
    Tup retTup = fibCont( state, fib, &argTup );
    
    statePop( state );
    return retTup;
}

void
libYield( State* state, Record* vals, bool pop ) {
    Fiber* fib = state->fiber;
    
    uint count = 0;
    TVal val   = recGet( state, vals, tvInt( count ) );
    while( !tvIsUdf( val ) ) {
        val = recGet( state, vals, tvInt( ++count ) );
    }
    
    Tup argTup = fibPush( state, fib, count );
    for( uint i = 0 ; i < count ; i++ ) {
        val = recGet( state, vals, tvInt( i ) );
        tupAt( argTup, i ) = val;
    }
    fibYield( state, &argTup, pop );
}

SymT
libState( State* state, Fiber* fib ) {
    LibState* lib = state->libState;
    
    switch( fib->state ) {
        case ten_FIB_RUNNING:
            return lib->idents[IDENT_running];
        break;
        case ten_FIB_WAITING:
            return lib->idents[IDENT_waiting];
        break;
        case ten_FIB_STOPPED:
            return lib->idents[IDENT_stopped];
        break;
        case ten_FIB_FINISHED:
            return lib->idents[IDENT_finished];
        break;
        case ten_FIB_FAILED:
            return lib->idents[IDENT_failed];
        break;
        default:
            tenAssertNeverReached();
            return 0;
        break;
    }
}

TVal
libErrval( State* state, Fiber* fib ) {
    return fib->errVal;
}

Record*
libTrace( State* state, Fiber* fib ) {
    ten_State* ten = (ten_State*)state;
    LibState*  lib = state->libState;
    
    if( fib->errNum == ten_ERR_NONE || fib->trace == NULL )
        return NULL;
    
    ten_Tup varTup   = ten_pushA( ten, "UUU" );
    ten_Var idxVar   = { .tup = &varTup, .loc = 0 };
    ten_Var seqVar   = { .tup = &varTup, .loc = 1 };
    ten_Var traceVar = { .tup = &varTup, .loc = 2 };
    
    Index* idx = idxNew( state );
    vset( idxVar, tvObj( idx ) );
    
    Record* seq = recNew( state, idx );
    vset( seqVar, tvObj( seq ) );
    
    
    uint loc = 0;
    ten_Trace* tIt = fib->trace;
    while( tIt ) {
        Record* trace = recNew( state, lib->traceIdx );
        vset( traceVar, tvObj(trace) );
        
        if( tIt->unit ) {
            TVal key = tvSym( lib->idents[IDENT_unit] );
            TVal val = tvSym( symGet( state, tIt->unit, strlen( tIt->unit ) ) );
            recDef( state, trace, key, val );
        }
        if( tIt->file ) {
            TVal key = tvSym( lib->idents[IDENT_file] );
            TVal val = tvSym( symGet( state, tIt->file, strlen( tIt->file ) ) );
            recDef( state, trace, key, val );
        }
        TVal key = tvSym( lib->idents[IDENT_line] );
        TVal val = tvInt( tIt->line );
        recDef( state, trace, key, val );
        
        recDef( state, seq, tvInt( loc++ ), tvObj( trace ) );
        tIt = tIt->next;
    }
    
    ten_pop( ten );
    return seq;
}

static void
countUpvals( State* state, void* dat, TVal key, TVal val ) {
    uint* count = dat;
    
    if( !tvIsSym( key ) )
        panic( "Upvalue given with non-Sym key" );
        
    *count += 1;
}

typedef struct {
    uint   count;
    char** names;
    TVal*  vals;
} UpvalBuf;

static void
fillUpvals( State* state, void* dat, TVal key, TVal val ) {
    UpvalBuf* dst = dat;
    
    SymT        sym = tvGetSym( key );
    size_t      len = symLen( state, sym );
    char const* buf = symBuf( state, sym );
    
    Part nameP;
    char* name = stateAllocRaw( state, &nameP, len + 1 );
    strcpy( name, buf );
    name[len] = '\0';
    stateCommitRaw( state, &nameP );
    
    dst->names[dst->count] = name;
    dst->vals[dst->count]  = val;
    dst->count++;
}

typedef struct {
    Defer  base;
    char** names;
} FreeNamesDefer;

static void
freeNamesDefer( State* state, Defer* d ) {
    char** names = ((FreeNamesDefer*)d)->names;
    for( uint i = 0 ; names[i] != NULL ; i++ ) {
        size_t len = strlen( names[i] );
        stateFreeRaw( state, names[i], len + 1 );
    }
}

Closure*
libScript( State* state, Record* upvals, String* script ) {
    ten_State* ten = (ten_State*)state;
    LibState*  lib = state->libState;
    
    uint count = 0;
    recForEach( state, upvals, &count, countUpvals );
    
    
    char*  names[count+1];
    TVal   vals[count];
    UpvalBuf buf = { .count = 0, .names = names, .vals = vals };
    recForEach( state, upvals, &buf, fillUpvals );
    names[count] = NULL;
    
    FreeNamesDefer defer = { .base = { .cb = freeNamesDefer }, .names = names };
    stateInstallDefer( state, (Defer*)&defer );
    
    ten_Tup varTup = ten_pushA( ten, "U" );
    ten_Var clsVar = { .tup = &varTup, .loc = 0 };
    
    ten_Source* src = ten_stringSource( ten, script->buf, "<unknown>" );
    ten_compileScript( ten, (char const**)names, src, ten_SCOPE_LOCAL, ten_COM_CLS, &clsVar );
    
    stateCommitDefer( state, (Defer*)&defer );
    
    
    Closure* cls = tvGetObj( vget( clsVar ) );
    for( uint i = 0 ; i < count ; i++ )
        cls->dat.upvals[i] = upvNew( state, buf.vals[i] );
    
    ten_pop( ten );
    
    return cls;
}

Closure*
libExpr( State* state, Record* upvals, String* script ) {
    ten_State* ten = (ten_State*)state;
    LibState*  lib = state->libState;
    
    uint count = 0;
    recForEach( state, upvals, &count, countUpvals );
    
    
    char*  names[count+1];
    TVal   vals[count];
    UpvalBuf buf = { .count = 0, .names = names, .vals = vals };
    recForEach( state, upvals, &buf, fillUpvals );
    names[count] = NULL;
    
    FreeNamesDefer defer = { .base = { .cb = freeNamesDefer }, .names = names };
    stateInstallDefer( state, (Defer*)&defer );
    
    ten_Tup varTup = ten_pushA( ten, "U" );
    ten_Var clsVar = { .tup = &varTup, .loc = 0 };
    
    ten_Source* src = ten_stringSource( ten, script->buf, "<unknown>" );
    ten_compileExpr( ten, (char const**)names, src, ten_SCOPE_LOCAL, ten_COM_CLS, &clsVar );
    
    stateCommitDefer( state, (Defer*)&defer );
    
    
    Closure* cls = tvGetObj( vget( clsVar ) );
    for( uint i = 0 ; i < count ; i++ )
        cls->dat.upvals[i] = upvNew( state, buf.vals[i] );
    
    ten_pop( ten );
    
    return cls;
}

#define expectArg( ARG, TYPE ) \
    libExpect( state, #ARG, state->libState->types[TYPE], vget( ARG ## Arg ) ) 

static ten_Tup
requireFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var modArg = { .tup = args, .loc = 0 };
    expectArg( mod, OBJ_STR );
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    vset( retVar, libRequire( state, tvGetObj( vget( modArg ) ) ) );
    return retTup;
}

static ten_Tup
importFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var modArg = { .tup = args, .loc = 0 };
    expectArg( mod, OBJ_STR );
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    vset( retVar, libImport( state, tvGetObj( vget( modArg ) ) ) );
    return retTup;
}

static ten_Tup
typeFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var valArg = { .tup = args, .loc = 0 };
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    vset( retVar, tvSym( libType( state, vget( valArg ) ) ) );
    return retTup;
}

static ten_Tup
panicFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var errArg = { .tup = args, .loc = 0 };
    panic( "%v", vget( errArg ) );
    
    return ten_pushA( ten, "" );
}

static ten_Tup
assertFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var condArg = { .tup = args, .loc = 0 };
    ten_Var whatArg = { .tup = args, .loc = 1 };
    
    TVal cond = vget( condArg );
    TVal what = vget( whatArg );
    libAssert( state, cond, what );
    
    return ten_pushA( ten, "" );
}

static ten_Tup
expectFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var whatArg = { .tup = args, .loc = 0 };
    ten_Var typeArg = { .tup = args, .loc = 1 };
    ten_Var valArg  = { .tup = args, .loc = 2 };
    
    expectArg( what, OBJ_STR );
    String* what = tvGetObj( vget( whatArg ) );
    
    expectArg( type, VAL_SYM );
    SymT type = tvGetSym( vget( typeArg ) );
    
    libExpect( state, what->buf, type, vget( valArg ) );
    
    return ten_pushA( ten, "" );
}

static ten_Tup
collectFun( ten_PARAMS ) {
    State* state = (State*)ten;
    libCollect( state );
    
    return ten_pushA( ten, "" );
}

static ten_Tup
loaderFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var typeArg  = { .tup = args, .loc = 0 };
    ten_Var loadrArg = { .tup = args, .loc = 1 };
    ten_Var optArg   = { .tup = args, .loc = 2 };
    
    expectArg( type, VAL_SYM );
    SymT type = tvGetSym( vget( typeArg ) );
    
    expectArg( loadr, OBJ_CLS );
    Closure* loadr = tvGetObj( vget( loadrArg ) );
    
    
    tenAssert( tvIsObjType( vget( optArg ), OBJ_REC ) );
    ten_Tup varTup = ten_pushA( ten, "U" );
    ten_Var transArg = { .tup = &varTup, .loc = 0 };
    
    ten_recGet( ten, &optArg, ten_int( ten, 0 ), &transArg );
    
    Closure* trans = NULL;
    if( !ten_isUdf( ten, &transArg ) ) {
        expectArg( trans, OBJ_CLS );
        trans = tvGetObj( vget( transArg ) );
    }
    
    libLoader( state, type, loadr, trans );
    
    return ten_pushA( ten, "" );
}

static ten_Tup
clockFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    return ten_pushA( ten, "D", libClock( state ) );
}

static ten_Tup
randFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    return ten_pushA( ten, "D", libRand( state ) );
}

static ten_Tup
logFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var valArg = { .tup = args, .loc = 0 };
    
    ten_Tup retTup = ten_pushA( (ten_State*)state, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    vset( retVar, libLog( state, vget( valArg ) ) );
    return retTup;
}

static ten_Tup
intFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var valArg = { .tup = args, .loc = 0 };
    
    ten_Tup retTup = ten_pushA( (ten_State*)state, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    vset( retVar, libInt( state, vget( valArg ) ) );
    return retTup;
}

static ten_Tup
decFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var valArg = { .tup = args, .loc = 0 };
    
    ten_Tup retTup = ten_pushA( (ten_State*)state, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    vset( retVar, libDec( state, vget( valArg ) ) );
    return retTup;
}

static ten_Tup
symFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var valArg = { .tup = args, .loc = 0 };
    
    ten_Tup retTup = ten_pushA( (ten_State*)state, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    vset( retVar, libSym( state, vget( valArg ) ) );
    return retTup;
}

static ten_Tup
strFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var valArg = { .tup = args, .loc = 0 };
    
    ten_Tup retTup = ten_pushA( (ten_State*)state, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    vset( retVar, libStr( state, vget( valArg ) ) );
    return retTup;
}

static ten_Tup
hexFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var strArg = { .tup = args, .loc = 0 };
    expectArg( str, OBJ_STR );
    
    ten_Tup retTup = ten_pushA( (ten_State*)state, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    vset( retVar, libHex( state, tvGetObj( vget( strArg ) ) ) );
    return retTup;
}

static ten_Tup
octFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var strArg = { .tup = args, .loc = 0 };
    expectArg( str, OBJ_STR );
    
    ten_Tup retTup = ten_pushA( (ten_State*)state, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    vset( retVar, libOct( state, tvGetObj( vget( strArg ) ) ) );
    return retTup;
}

static ten_Tup
binFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var strArg = { .tup = args, .loc = 0 };
    expectArg( str, OBJ_STR );
    
    ten_Tup retTup = ten_pushA( (ten_State*)state, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    vset( retVar, libBin( state, tvGetObj( vget( strArg ) ) ) );
    return retTup;
}

static ten_Tup
keysFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var recArg = { .tup = args, .loc = 0 };
    expectArg( rec, OBJ_REC );
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    Closure* cls = libKeys( (State*)ten, tvGetObj( vget( recArg ) ) );
    vset( retVar, tvObj( cls ) );
    return retTup;
}

static ten_Tup
valsFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var recArg = { .tup = args, .loc = 0 };
    expectArg( rec, OBJ_REC );
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    Closure* cls = libVals( (State*)ten, tvGetObj( vget( recArg ) ) );
    vset( retVar, tvObj( cls ) );
    
    return retTup;
}

static ten_Tup
pairsFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var recArg = { .tup = args, .loc = 0 };
    expectArg( rec, OBJ_REC );
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    Closure* cls = libPairs( (State*)ten, tvGetObj( vget( recArg ) ) );
    vset( retVar, tvObj( cls ) );
    
    return retTup;
}

static ten_Tup
seqFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var valsArg = { .tup = args, .loc = 0 };
    tenAssert( tvIsObjType( vget( valsArg ), OBJ_REC ) );
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    Closure* cls = libSeq( (State*)ten, tvGetObj( vget( valsArg ) ) );
    vset( retVar, tvObj( cls ) );
    
    return retTup;
}

static ten_Tup
rseqFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var valsArg = { .tup = args, .loc = 0 };
    expectArg( vals, OBJ_REC );
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    Closure* cls = libSeq( (State*)ten, tvGetObj( vget( valsArg ) ) );
    vset( retVar, tvObj( cls ) );
    
    return retTup;
}

static ten_Tup
bytesFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var strArg = { .tup = args, .loc = 0 };
    expectArg( str, OBJ_STR );
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    Closure* cls = libBytes( (State*)ten, tvGetObj( vget( strArg ) ) );
    vset( retVar, tvObj( cls ) );
    
    return retTup;
}

static ten_Tup
charsFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var strArg = { .tup = args, .loc = 0 };
    expectArg( str, OBJ_STR );
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    Closure* cls = libChars( (State*)ten, tvGetObj( vget( strArg ) ) );
    vset( retVar, tvObj( cls ) );
    return retTup;
}

static ten_Tup
splitFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var strArg = { .tup = args, .loc = 0 };
    ten_Var sepArg = { .tup = args, .loc = 1 };
    expectArg( str, OBJ_STR );
    expectArg( sep, OBJ_STR );
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    Closure* cls = libSplit( (State*)ten, tvGetObj( vget( strArg ) ), tvGetObj( vget( sepArg ) ) );
    vset( retVar, tvObj( cls ) );
    return retTup;
}


static ten_Tup
itemsFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var listArg = { .tup = args, .loc = 0 };
    expectArg( list, OBJ_REC );
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    Closure* cls = libItems( (State*)ten, tvGetObj( vget( listArg ) ) );
    vset( retVar, tvObj( cls ) );
    
    return retTup;
}

static ten_Tup
drangeFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var startArg = { .tup = args, .loc = 0 };
    ten_Var endArg   = { .tup = args, .loc = 1 };
    ten_Var optArg   = { .tup = args, .loc = 2 };
    
    expectArg( start, VAL_DEC );
    expectArg( end, VAL_DEC );
    tenAssert( tvIsObjType( vget( optArg ), OBJ_REC ) );
    
    DecT start = tvGetDec( vget( startArg ) );
    DecT end   = tvGetDec( vget( endArg ) );
    DecT step  = end >= start ? 1.0 : -1.0;
    
    Record*  opt = tvGetObj( vget( optArg ) );
    TVal opt0 = recGet( state, opt, tvInt( 0 ) );
    if( !tvIsUdf( opt0 ) ) {
        if( !tvIsDec( opt0 ) )
            panic( "DRange step has non-Dec type" );
        step = tvGetDec( opt0 );
    }
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    vset( retVar, tvObj( libDrange( state, start, end, step ) ) );

    return retTup;
}

static ten_Tup
irangeFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var startArg = { .tup = args, .loc = 0 };
    ten_Var endArg   = { .tup = args, .loc = 1 };
    ten_Var optArg   = { .tup = args, .loc = 2 };
    
    expectArg( start, VAL_INT );
    expectArg( end, VAL_INT );
    tenAssert( tvIsObjType( vget( optArg ), OBJ_REC ) );
    
    IntT start = tvGetInt( vget( startArg ) );
    IntT end   = tvGetInt( vget( endArg ) );
    IntT step  = end >= start ? 1 : -1;
    
    Record*  opt = tvGetObj( vget( optArg ) );
    TVal opt0 = recGet( state, opt, tvInt( 0 ) );
    if( !tvIsUdf( opt0 ) ) {
        if( !tvIsInt( opt0 ) )
            panic( "IRange step has non-Int type" );
        step = tvGetInt( opt0 );
    }
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    vset( retVar, tvObj( libIrange( state, start, end, step ) ) );

    return retTup;
}

static ten_Tup
showFun( ten_PARAMS ) {
    State* state = (State*)ten;
    ten_Var valsArg = { .tup = args, .loc = 0 };
    
    tenAssert( tvIsObjType( vget( valsArg ), OBJ_REC ) );
    
    libShow( state, tvGetObj( vget( valsArg ) ) );
    
    return ten_pushA( ten, "" );
}

static ten_Tup
warnFun( ten_PARAMS ) {
    State* state = (State*)ten;
    ten_Var valsArg = { .tup = args, .loc = 0 };
    
    tenAssert( tvIsObjType( vget( valsArg ), OBJ_REC ) );
    
    libWarn( state, tvGetObj( vget( valsArg ) ) );
    
    return ten_pushA( ten, "" );
}

static ten_Tup
inputFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    String* str = libInput( state );
    vset( retVar, tvObj( str ) );
    
    return retTup;
}

static ten_Tup
ucodeFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var chrArg = { .tup = args, .loc = 0 };
    expectArg( chr, VAL_SYM );
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    vset( retVar, libUcode( state, tvGetSym( vget( chrArg ) ) ) );
    return retTup;
}

static ten_Tup
ucharFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var codeArg = { .tup = args, .loc = 0 };
    expectArg( code, VAL_INT );
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    vset( retVar, libUchar( state, tvGetInt( vget( codeArg ) ) ) );
    return retTup;
}

static ten_Tup
catFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var valsArg = { .tup = args, .loc = 0 };
    tenAssert( tvIsObjType( vget( valsArg ), OBJ_REC ) );
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    String* str = libCat( state, tvGetObj( vget( valsArg ) ) );
    vset( retVar, tvObj( str ) );
    
    return retTup;
}

static ten_Tup
joinFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var iterArg = { .tup = args, .loc = 0 };
    ten_Var sepArg  = { .tup = args, .loc = 1 };
    
    expectArg( iter, OBJ_CLS );
    expectArg( sep, OBJ_STR );
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    String* str = libJoin( state, tvGetObj( vget( iterArg ) ), tvGetObj( vget( sepArg ) ) );
    vset( retVar, tvObj( str ) );
    
    return retTup;
}

static ten_Tup
bcmpFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var str1Arg = { .tup = args, .loc = 0 };
    ten_Var oprArg  = { .tup = args, .loc = 1 };
    ten_Var str2Arg = { .tup = args, .loc = 2 };
    
    expectArg( str1, OBJ_STR );
    expectArg( opr, VAL_SYM );
    expectArg( str2, OBJ_STR );
    
    String* str1 = tvGetObj( vget( str1Arg ) );
    SymT    opr  = tvGetSym( vget( oprArg ) );
    String* str2 = tvGetObj( vget( str2Arg ) );
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    vset( retVar, libBcmp( state, str1, opr, str2 ) );
    return retTup;
}

static ten_Tup
ccmpFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var str1Arg = { .tup = args, .loc = 0 };
    ten_Var oprArg  = { .tup = args, .loc = 1 };
    ten_Var str2Arg = { .tup = args, .loc = 2 };
    
    expectArg( str1, OBJ_STR );
    expectArg( opr, VAL_SYM );
    expectArg( str2, OBJ_STR );
    
    String* str1 = tvGetObj( vget( str1Arg ) );
    SymT    opr  = tvGetSym( vget( oprArg ) );
    String* str2 = tvGetObj( vget( str2Arg ) );
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    vset( retVar, libCcmp( state, str1, opr, str2 ) );
    return retTup;
}

static ten_Tup
bsubFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var strArg = { .tup = args, .loc = 0 };
    ten_Var nArg   = { .tup = args, .loc = 1 };
    
    expectArg( str, OBJ_STR );
    expectArg( n, VAL_INT );
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    String* sub = libBsub( state, tvGetObj( vget( strArg ) ), tvGetInt( vget( nArg ) ) );
    vset( retVar, tvObj( sub ) );
    
    return retTup;
}

static ten_Tup
csubFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var strArg = { .tup = args, .loc = 0 };
    ten_Var nArg   = { .tup = args, .loc = 1 };
    
    expectArg( str, OBJ_STR );
    expectArg( n, VAL_INT );
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    String* sub = libCsub( state, tvGetObj( vget( strArg ) ), tvGetInt( vget( nArg ) ) );
    vset( retVar, tvObj( sub ) );
    
    return retTup;
}


static ten_Tup
blenFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var strArg = { .tup = args, .loc = 0 };
    expectArg( str, OBJ_STR );
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    size_t len = libBlen( state, tvGetObj( vget( strArg ) ) );
    vset( retVar, tvInt( len ) );
    
    return retTup;
}

static ten_Tup
clenFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var strArg = { .tup = args, .loc = 0 };
    expectArg( str, OBJ_STR );
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    size_t len = libClen( state, tvGetObj( vget( strArg ) ) );
    vset( retVar, tvInt( len ) );
    
    return retTup;
}


static ten_Tup
eachFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var iterArg = { .tup = args, .loc = 0 };
    ten_Var whatArg = { .tup = args, .loc = 1 };
    
    expectArg( iter, OBJ_CLS );
    expectArg( what, OBJ_CLS );
    
    libEach( state, tvGetObj( vget( iterArg ) ), tvGetObj( vget( whatArg ) ) );
    
    return ten_pushA( ten, "" );
}

static ten_Tup
foldFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var iterArg = { .tup = args, .loc = 0 };
    ten_Var agrArg  = { .tup = args, .loc = 1 };
    ten_Var howArg  = { .tup = args, .loc = 2 };
    
    expectArg( iter, OBJ_CLS );
    expectArg( how, OBJ_CLS );
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    vset( retVar, libFold( state, tvGetObj( vget( iterArg ) ), vget( agrArg ), tvGetObj( vget( howArg ) ) ) );
    
    return retTup;
}

static ten_Tup
consFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var carArg = { .tup = args, .loc = 0 };
    ten_Var cdrArg = { .tup = args, .loc = 1 };
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    vset( retVar, tvObj( libCons( state, vget( carArg ), vget(cdrArg ) ) ) );
    
    return retTup;
}

static ten_Tup
sepFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var recArg = { .tup = args, .loc = 1 };
    expectArg( rec, OBJ_REC );
    
    Record* rec = tvGetObj( vget( recArg ) );
    libSep( state, rec );
    
    return ten_pushA( ten, "V", &recArg );
}

static ten_Tup
listFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var valsArg = { .tup = args, .loc = 0 };
    tenAssert( tvIsObjType( vget( valsArg ), OBJ_REC ) );
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    vset( retVar, tvObj( libList( state, tvGetObj( vget( valsArg ) ) ) ) );
    
    return retTup;
}

static ten_Tup
explodeFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var iterArg = { .tup = args, .loc = 0 };
    expectArg( iter, OBJ_CLS );
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    vset( retVar, tvObj( libExplode( state, tvGetObj( vget( iterArg ) ) ) ) );
    
    return retTup;
}

static ten_Tup
fiberFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var clsArg = { .tup = args, .loc = 0 };
    ten_Var optArg = { .tup = args, .loc = 1 };
    
    expectArg( cls, OBJ_CLS );
    tenAssert( tvIsObjType( vget( optArg ), OBJ_REC ) );
    
    Closure* cls = tvGetObj( vget( clsArg ) );
    Record*  opt = tvGetObj( vget( optArg ) );
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    TVal opt0 = recGet( state, opt, tvInt( 0 ) );
    if( !tvIsUdf( opt0 ) ) {
        if( !tvIsSym( opt0 ) )
            panic( "Fiber tag has non-Sym type" );
    
        SymT tag = tvGetSym( opt0 );
        
        vset( retVar, tvObj( libFiber( state, cls, &tag ) ) );
    }
    else {
        vset( retVar, tvObj( libFiber( state, cls, NULL ) ) );
    }
    
    return retTup;
}

static ten_Tup
contFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var fibArg  = { .tup = args, .loc = 0 };
    ten_Var argsArg = { .tup = args, .loc = 1 };
    
    expectArg( fib, OBJ_FIB );
    expectArg( args, OBJ_REC );
    
    Fiber*  fib  = tvGetObj( vget( fibArg ) );
    Record* rec = tvGetObj( vget( argsArg ) );
    
    Tup ret = libCont( state, fib, rec );
    
    ten_Tup retTup;
    memcpy( &retTup, &ret, sizeof(Tup) );
    return retTup;
}

static ten_Tup
yieldFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var valsArg = { .tup = args, .loc = 0 };
    tenAssert( tvIsObjType( vget( valsArg ), OBJ_REC ) );
    
    Record* vals = tvGetObj( vget( valsArg ) );
    
    libYield( state, vals, true );
    
    return ten_pushA( ten, "" );
}

static ten_Tup
stateFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var fibArg = { .tup = args, .loc = 0 };
    expectArg( fib, OBJ_FIB );
    
    Fiber* fib = tvGetObj( vget( fibArg ) );
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    vset( retVar, tvSym( libState( state, fib ) ) );
    return retTup;
}

static ten_Tup
errvalFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var fibArg = { .tup = args, .loc = 0 };
    expectArg( fib, OBJ_FIB );
    
    Fiber* fib = tvGetObj( vget( fibArg ) );
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    vset( retVar, libErrval( state, fib ) );
    return retTup;
}

static ten_Tup
traceFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var fibArg = { .tup = args, .loc = 0 };
    expectArg( fib, OBJ_FIB );
    
    Fiber* fib = tvGetObj( vget( fibArg ) );
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    
    Record* trace = libTrace( state, fib );
    if( trace )
        vset( retVar, tvObj( trace ) );
    else
        vset( retVar, tvUdf() );
    return retTup;
}

static ten_Tup
scriptFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var upvalsArg = { .tup = args, .loc = 0 };
    ten_Var codeArg   = { .tup = args, .loc = 1 };
    expectArg( upvals, OBJ_REC );
    expectArg( code, OBJ_STR );
    
    Record* upvals = tvGetObj( vget( upvalsArg ) );
    String* code   = tvGetObj( vget( codeArg ) );
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    vset( retVar, tvObj( libScript( state, upvals, code ) ) );
    
    return retTup;
}

static ten_Tup
exprFun( ten_PARAMS ) {
    State* state = (State*)ten;
    
    ten_Var upvalsArg = { .tup = args, .loc = 0 };
    ten_Var codeArg   = { .tup = args, .loc = 1 };
    expectArg( upvals, OBJ_REC );
    expectArg( code, OBJ_STR );
    
    Record* upvals = tvGetObj( vget( upvalsArg ) );
    String* code   = tvGetObj( vget( codeArg ) );
    
    ten_Tup retTup = ten_pushA( ten, "U" );
    ten_Var retVar = { .tup = &retTup, .loc = 0 };
    vset( retVar, tvObj( libExpr( state, upvals, code ) ) );
    
    return retTup;
}

void
libInit( State* state ) {
    ten_State* s = (ten_State*)state;
    
    Part libP;
    LibState* lib = stateAllocRaw( state, &libP, sizeof(LibState) );
    lib->val1 = tvUdf();
    lib->val2 = tvUdf();
    lib->cellIdx     = NULL;
    lib->traceIdx    = NULL;
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
    
    ten_Tup varTup = ten_pushA( (ten_State*)state, "UUUU" );
    ten_Var idxVar = { .tup = &varTup, .loc = 0 };
    ten_Var funVar = { .tup = &varTup, .loc = 1 };
    ten_Var clsVar = { .tup = &varTup, .loc = 2 };
    ten_Var symVar = { .tup = &varTup, .loc = 3 };
    
    
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
    IDENT( clock );
    IDENT( rand );
    
    IDENT( log );
    IDENT( int );
    IDENT( dec );
    IDENT( sym );
    IDENT( str );
    
    IDENT( hex );
    IDENT( oct );
    IDENT( bin );
    
    IDENT( keys );
    IDENT( vals );
    IDENT( pairs );
    IDENT( seq );
    IDENT( rseq );
    IDENT( bytes );
    IDENT( chars );
    IDENT( split );
    IDENT( items );
    IDENT( drange );
    IDENT( irange );
    
    IDENT( show );
    IDENT( warn );
    IDENT( input );
    
    IDENT( ucode );
    IDENT( uchar );
    
    IDENT( cat );
    IDENT( join );
    IDENT( bcmp );
    IDENT( ccmp );
    IDENT( bsub );
    IDENT( csub );
    IDENT( blen );
    IDENT( clen );
    
    IDENT( each );
    IDENT( fold );
    
    IDENT( sep );
    
    IDENT( explode );
    IDENT( cons );
    IDENT( list );
    
    IDENT( fiber );
    IDENT( cont );
    IDENT( yield );
    IDENT( state );
    IDENT( errval );
    IDENT( trace );
    IDENT( script );
    IDENT( expr );
    
    IDENT( tag );
    IDENT( car );
    IDENT( cdr );
    
    IDENT( unit );
    IDENT( file );
    IDENT( line );
    
    IDENT( running );
    IDENT( waiting );
    IDENT( stopped );
    IDENT( finished );
    IDENT( failed );
    

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
            vset( idxVar, tvObj( idx ) );                           \
        }                                                           \
                                                                    \
        Function* fun = funNewNat( state, (P), idx, N ## Fun );     \
        fun->u.nat.name = lib->idents[IDENT_ ## N];                 \
        vset( funVar, tvObj( fun ) );                               \
                                                                    \
        Closure* cls = clsNewNat( state, fun, NULL );               \
        vset( clsVar, tvObj( cls ) );                               \
                                                                    \
        vset( symVar, tvSym( lib->idents[IDENT_ ## N] ) );          \
        ten_def( s, &symVar, &clsVar );                             \
    } while( 0 )
    
    FUN( require, 1, false );
    FUN( import, 1, false );
    FUN( type, 1, false );
    FUN( panic, 1, false );
    FUN( assert, 2, false );
    FUN( expect, 3, false );
    FUN( collect, 0, false );
    FUN( loader, 2, true );
    FUN( clock, 0, false );
    FUN( rand, 0, false );
    
    FUN( log, 1, false );
    FUN( int, 1, false );
    FUN( dec, 1, false );
    FUN( sym, 1, false );
    FUN( str, 1, false );
    
    FUN( hex, 1, false );
    FUN( oct, 1, false );
    FUN( bin, 1, false );
    
    FUN( keys, 1, false );
    FUN( vals, 1, false );
    FUN( pairs, 1, false );
    FUN( seq, 0, true );
    FUN( rseq, 1, false );
    FUN( bytes, 1, false );
    FUN( chars, 1, false );
    FUN( split, 2, false );
    FUN( items, 1, false );
    FUN( drange, 2, true );
    FUN( irange, 2, true );
    
    FUN( show, 0, true );
    FUN( warn, 0, true );
    FUN( input, 0, false );
    
    FUN( ucode, 1, false );
    FUN( uchar, 1, false );
    
    FUN( cat, 0, true );
    FUN( join, 2, false );
    FUN( bcmp, 3, false );
    FUN( ccmp, 3, false );
    FUN( bsub, 2, false );
    FUN( csub, 2, false );
    FUN( blen, 1, false );
    FUN( clen, 1, false );
    
    FUN( each, 2, false );
    FUN( fold, 3, false );
    
    FUN( sep, 1, false );
    
    FUN( cons, 2, false );
    FUN( list, 0, true );
    FUN( explode, 1, false );
    
    FUN( fiber, 1, true );
    FUN( cont, 2, false );
    FUN( yield, 0, true );
    FUN( state, 1, false );
    FUN( errval, 1, false );
    FUN( trace, 1, false );
    
    FUN( script, 2, false );
    FUN( expr, 2, false );
    
    ten_def( s, ten_sym( s, "N" ), ten_sym( s, "\n" ) );
    ten_def( s, ten_sym( s, "R" ), ten_sym( s, "\r" ) );
    ten_def( s, ten_sym( s, "L" ), ten_sym( s, "\r\n" ) );
    ten_def( s, ten_sym( s, "T" ), ten_sym( s, "\t" ) );
    ten_def( s, ten_sym( s, "NULL" ), ten_ptr( s, NULL ) );
    
    
    lib->recIterInfo = ten_addDatInfo(
        s,
        &(ten_DatConfig){
            .tag   = "RecIter",
            .size  = sizeof(RecIter),
            .mems  = RecIter_LAST,
            .destr = recIterDestr
        }
    );
    lib->seqInfo = ten_addDatInfo(
        s,
        &(ten_DatConfig){
            .tag   = "Seq",
            .size  = sizeof(Seq),
            .mems  = Seq_LAST,
            .destr = NULL
        }
    );
    lib->strIterInfo = ten_addDatInfo(
        s,
        &(ten_DatConfig){
            .tag   = "StrIter",
            .size  = sizeof(StrIter),
            .mems  = StrIter_LAST,
            .destr = NULL
        }
    );
    lib->splitIterInfo = ten_addDatInfo(
        s,
        &(ten_DatConfig){
            .tag   = "SplitIter",
            .size  = sizeof(SplitIter),
            .mems  = SplitIter_LAST,
            .destr = NULL
        }
    );
    lib->listIterInfo = ten_addDatInfo(
        s,
        &(ten_DatConfig){
            .tag   = "ListIter",
            .size  = sizeof(ListIter),
            .mems  = ListIter_LAST,
            .destr = NULL
        }
    );
    lib->dRangeInfo = ten_addDatInfo(
        s,
        &(ten_DatConfig){
            .tag   = "DRange",
            .size  = sizeof(DRange),
            .mems  = 0,
            .destr = NULL
        }
    );
    lib->iRangeInfo = ten_addDatInfo(
        s,
        &(ten_DatConfig){
            .tag   = "IRange",
            .size  = sizeof(IRange),
            .mems  = 0,
            .destr = NULL
        }
    );
    
    statePop( state ); // varTup
    
    lib->cellIdx  = idxNew( state );
    lib->traceIdx = idxNew( state );
    
    Index* importIdx = idxNew( state );
    lib->val1 = tvObj( importIdx );
    
    lib->loaders     = recNew( state, importIdx );
    lib->translators = recNew( state, importIdx );
    
    Index* moduleIdx = idxNew( state );
    lib->val1 = tvObj( moduleIdx );
    
    lib->modules = recNew( state, moduleIdx );
    
    
    state->libState = lib;
}
