#include "ten_fmt.h"
#include "ten_sym.h"
#include "ten_ptr.h"
#include "ten_idx.h"
#include "ten_rec.h"
#include "ten_cls.h"
#include "ten_fun.h"
#include "ten_dat.h"
#include "ten_str.h"
#include "ten_state.h"
#include "ten_assert.h"
#include "ten_macros.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define BUF_TYPE char
#define BUF_NAME CharBuf
    #include "inc/buf.inc"
#undef BUF_TYPE
#undef BUF_NAME

struct FmtState {
    Finalizer finl;
    
    CharBuf buf;
};

static void
fmtFinl( State* state, Finalizer* finl ) {
    FmtState* fmtState = structFromFinl( FmtState, finl );
    finlCharBuf( state, &fmtState->buf );
    stateFreeRaw( state, fmtState, sizeof(FmtState) );
}

void
fmtInit( State* state ) {
    Part fmtP;
    FmtState* fmt = stateAllocRaw( state, &fmtP, sizeof(FmtState) );
    initCharBuf( state, &fmt->buf );
    fmt->finl.cb = fmtFinl;
    
    stateInstallFinalizer( state, &fmt->finl );
    stateCommitRaw( state, &fmtP );
    state->fmtState = fmt;
    
    *putCharBuf( state, &fmt->buf ) = '\0';
}

void
fmtTest( State* state ) {
    char const* t1 = fmtA( state, false, "Test %i: %v", 1, tvInt( 1 ) );
    tenAssert( !strcmp( t1, "Test 1: 1" ) );
    
    SymT s2 = symGet( state, "2", 1 );
    char const* t2 = fmtA( state, false, "Test %s: %v", "2", tvSym( s2 ) );
    tenAssert( !strcmp( t2, "Test 2: 2" ) );
    
    String* s3 = strNew( state, "3", 1 );
    char const* t3 = fmtA( state, false, "Test 3: %q", tvObj( s3 ) );
    tenAssert( !strcmp( t3, "Test 3: \"3\"" ) );
    
    fmtA( state, false, "Hello," );
    fmtA( state, true, " " );
    fmtA( state, true, "World!" );
    tenAssert( !strcmp( fmtBuf( state ), "Hello, World!" ) );
}

char const*
fmtA( State* state, bool append, char const* fmt, ... ) {
    va_list ap;
    va_start( ap, fmt );
    char const* buf = fmtV( state, append, fmt, ap );
    va_end( ap );
    
    return buf;
}

static void
fmtStdV( State* state, char const* fmt, va_list ap ) {
    FmtState* fmtState = state->fmtState;
    
    // Figure out how much room it'll take to format the segment.
    va_list ac;
    va_copy( ac, ap );
    int len = vsnprintf( NULL, 0, fmt, ac );
    va_end( ac );
    tenAssert( len >= 0 );
    
    ensureCharBuf( state, &fmtState->buf, len + 1 );
    len = vsnprintf( fmtState->buf.buf + fmtState->buf.top, fmtState->buf.cap, fmt, ap );
    tenAssert( len >= 0 );
    fmtState->buf.top += len;
}

static void
fmtStdA( State* state, char const* fmt, ... ) {
    va_list ap;
    va_start( ap, fmt );
    fmtStdV( state, fmt, ap );
    va_end( ap );
}

static void
fmtRaw( State* state, char const* str ) {
    FmtState* fmtState = state->fmtState;
    
    size_t len = strlen( str );
    ensureCharBuf( state, &fmtState->buf, len + 1 );
    char* dst = &fmtState->buf.buf[fmtState->buf.top];
    strcpy( dst, str );
    fmtState->buf.top += len;
}
static void
fmtSym( State* state, SymT sym, bool q );

static void
fmtType( State* state, TVal val ) {
    if( tvIsObj( val ) ) {
        void*  obj = tvGetObj( val );
        ObjTag tag = datGetTag( obj );
        switch( tag ) {
            case OBJ_STR:
                fmtRaw( state, "Str" );
            break;
            case OBJ_IDX:
                fmtRaw( state, "Idx" );
            break;
            case OBJ_REC: {
                fmtRaw( state, "Rec" );
                
                Record* rec = obj;
                TVal tagK = tvSym( symGet( state, "tag", 3 ) );
                TVal tagV = recGet( state, rec, tagK );
                if( !tvIsSym( tagV ) ) {
                    fmtRaw( state, ":" );
                    fmtSym( state, tvGetSym( tagV ), false );
                }
            } break;
            case OBJ_FUN: {
                fmtRaw( state, "Fun:" );
                
                Function* fun = obj;
                fmtStdA( state, "%u", (uint)fun->nParams );
                if( fun->vargIdx )
                    fmtRaw( state, "+" );
            } break;
            case OBJ_CLS: {
                fmtRaw( state, "Cls:" );
                
                Closure*  cls = obj;
                Function* fun = cls->fun;
                fmtStdA( state, "%u", (uint)fun->nParams );
                if( fun->vargIdx )
                    fmtRaw( state, "+" );
            } break;
            case OBJ_FIB:
                fmtRaw( state, "Fib" );
            break;
            case OBJ_UPV:
                fmtRaw( state, "Upv" );
            break;
            case OBJ_DAT: {
                Data* dat = obj;
                fmtSym( state, dat->info->type, false );
            } break;
            default:
                tenAssertNeverReached();
            break;
        }
    }
    else
    if( tvIsSym( val ) ) {
        fmtRaw( state, "Sym" );
    }
    else
    if( tvIsPtr( val ) ) {
        PtrInfo* info = ptrInfo( state, tvGetPtr( val ) );
        fmtSym( state, info->type, false );
    }
    else
    if( tvIsUdf( val ) ) {
        fmtRaw( state, "Udf" );
    }
    else
    if( tvIsNil( val ) ) {
        fmtRaw( state, "Nil" );
    }
    else
    if( tvIsLog( val ) ) {
        fmtRaw( state, "Log" );
    }
    else
    if( tvIsInt( val ) ) {
        fmtRaw( state, "Int" );
    }
    else
    if( tvIsDec( val ) ) {
        fmtRaw( state, "Dec" );
    }
    else
    if( tvIsTup( val ) ) {
        fmtRaw( state, "Tup" );
    }
    else
    if( tvIsRef( val ) ) {
        fmtRaw( state, "Ref" );
    }
    else {
        tenAssertNeverReached();
    }
}

static void
fmtSym( State* state, SymT sym, bool q ) {
    FmtState* fmtState = state->fmtState;
    
    size_t      len = symLen( state, sym );
    char const* buf = symBuf( state, sym );
    
    bool alt = false;
    if( q ) {
        for( uint i = 0 ; i < len ; i++ )
            alt = alt || (buf[i] == '\'') || (buf[i] == '\n');
        if( alt )
            fmtRaw( state, "'|" );
        else
            fmtRaw( state, "'" );
    }
    
    ensureCharBuf( state, &fmtState->buf, len + 1 );
    char* dst = &fmtState->buf.buf[fmtState->buf.top];
    memcpy( dst, buf, len );
    fmtState->buf.top += len;
    
    if( q ) {
        if( alt )
            fmtRaw( state, "|'" );
        else
            fmtRaw( state, "'" );
    }
}

static void
fmtStr( State* state, String* str, bool q ) {
    FmtState* fmtState = state->fmtState;
    
    size_t      len = strLen( state, str );
    char const* buf = strBuf( state, str );
    
    bool alt = false;
    if( q ) {
        for( uint i = 0 ; i < len ; i++ )
            alt = alt || (buf[i] == '"') || (buf[i] == '\n');
        if( alt )
            fmtRaw( state, "\"|" );
        else
            fmtRaw( state, "\"" );
    }
    
    ensureCharBuf( state, &fmtState->buf, len + 1 );
    char* dst = &fmtState->buf.buf[fmtState->buf.top];
    memcpy( dst, buf, len );
    fmtState->buf.top += len;
    
    if( q ) {
        if( alt )
            fmtRaw( state, "|\"" );
        else
            fmtRaw( state, "\"" );
    }
}

static void
fmtVal( State* state, TVal val, bool q );

static TVal
nextKey( State* state, IdxIter* iter, uint seqEnd ) {
    TVal  key = tvUdf();
    uint  loc;
    while( idxIterNext( state, iter, &key, &loc ) && tvIsInt( key ) && tvGetInt( key ) < seqEnd )
        ;
    return key;
}

static bool
isIdent( State* state, TVal val ) {
    if( !tvIsSym( val ) )
        return false;
    
    SymT        sym = tvGetSym( val );
    size_t      len = symLen( state, sym );
    char const* buf = symBuf( state, sym );
    
    bool is = len > 0 && ( isalpha( buf[0] ) || buf[0] == '_' );
    for( size_t i = 1 ; i < len ; i++ )
        is = is && ( isalpha( buf[0] ) || isdigit( buf[0] ) || buf[0] == '_' );
    
    return is;
}

static void
fmtRec( State* state, Record* rec ) {
    
    fmtRaw( state, "{ " );
    
    // First print all the sequenced values.
    uint loc = 0;
    TVal key = tvInt( loc );
    TVal val = recGet( state, rec, key );
    while( !tvIsUdf( val ) ) {
        fmtVal( state, val, true );
        
        key = tvInt( ++loc );
        val = recGet( state, rec, key );
        if( !tvIsUdf( val ) )
            fmtRaw( state, ", " );
    }
    
    // Now do the normal fields.
    IdxIter* iter = idxIterMake( state, recIndex( state, rec ) );
    key = nextKey( state, iter, loc );
    val = tvIsUdf( key )? tvUdf() : recGet( state, rec, key );
    
    // If `loc != 0` then we know that some sequence items
    // were added to the format buffer, and if `val != udf`
    // then we know that at least one more field will be
    // added, so we add a comma to delimit the two.
    if( loc != 0 && !tvIsUdf( val ) )
        fmtRaw( state, ", " );
    
     
    while( !tvIsUdf( val ) ) {
        if( isIdent( state, key ) ) {
            fmtRaw( state, "." );
            fmtVal( state, key, false );
        }
        else {
            fmtRaw( state, "@" );
            fmtVal( state, key, true );
        }
        fmtRaw( state, ": " );
        fmtVal( state, val, true );
        
        key = nextKey( state, iter, loc );
        val = tvIsUdf( key )? tvUdf() : recGet( state, rec, key );
        if( !tvIsUdf( val ) )
            fmtRaw( state, ", " );
    }
    
    fmtRaw( state, "}" );
}

static void
fmtObj( State* state, void* obj, bool q ) {
    switch( datGetTag( obj ) ) {
        case OBJ_STR:
            fmtStr( state, obj, q );
        break;
        case OBJ_REC:
            fmtRec( state, obj );
        break;
        default:
            fmtRaw( state, "<" );
            fmtType( state, tvObj( obj ) );
            fmtRaw( state, ">" );
        break;
    }
}

static void
fmtVal( State* state, TVal val, bool q ) {
    if( tvIsObj( val ) )
        fmtObj( state, tvGetObj( val ), q );
    else
    if( tvIsSym( val ) )
        fmtSym( state, tvGetSym( val ), q );
    else
    if( tvIsUdf( val ) )
        fmtRaw( state, "udf" );
    else
    if( tvIsNil( val ) )
        fmtRaw( state, "nil" );
    else
    if( tvIsLog( val ) ) {
        if( tvGetLog( val ) )
            fmtRaw( state, "true" );
        else
            fmtRaw( state, "false" );
    }
    else
    if( tvIsInt( val ) )
        fmtStdA( state, "%lli", (llong)tvGetInt( val ) );
    else
    if( tvIsDec( val ) )
        fmtStdA( state, "%f", (double)tvGetDec( val ) );
    else {
        fmtRaw( state, "<" );
        fmtType( state, val );
        fmtRaw( state, ">" );
    }
}

char const*
fmtV( State* state, bool append, char const* fmt, va_list ap ) {
    FmtState* fmtState = state->fmtState;
    
    // Remove the '\0' terminator from the end of the buffer
    // if we're appending, otherwise reset the buffer.
    if( append )
        fmtState->buf.top--;
    else
        fmtState->buf.top = 0;
        
    // Copy the format string so we can insert '\0'
    // take efficient substrings of the format.
    Part fCpyP;
    char*  fCpy = stateAllocRaw( state, &fCpyP, strlen( fmt ) + 1 );
    strcpy( fCpy, fmt );
    
    // Loop over the format string.  For segments that can be
    // formatted correctly with vsprintf() format them with
    // a call to the same, for `%v` and `%t` patterns format
    // them manually.
    char* c = fCpy;
    while( *c ) {
        
        // Find the next custom pattern.
        size_t i = 0;
        while( c[i] && ( c[i] != '%' || ( c[i+1] != 'v' && c[i+1] != 'q' && c[i+1] != 't' ) ) )
            i++;
        if( c[i] == '\0' ) {
            fmtStdV( state, c, ap );
            break;
        }
        
        // Replace the '%' of the next custom pattern
        // with '\0' to terminate the vsprintf() compatible
        // portion of the string.
        c[i] = '\0';
        fmtStdV( state, c, ap );
        
        // Handle the custom patterns.
        switch( c[i+1] ) {
            case 'v':
                fmtVal( state, va_arg( ap, TVal ), false );
            break;
            case 'q':
                fmtVal( state, va_arg( ap, TVal ), true );
            break;
            case 't':
                fmtType( state, va_arg( ap, TVal ) );
            break;
            default:
                tenAssertNeverReached();
            break;
        }
        
        // Now start over with the next segment.
        c = c + i + 2;
    }
    
    // Terminate with '\0'.
    *putCharBuf( state, &fmtState->buf ) = '\0';
    
    // Release the temporary copy.
    stateCancelRaw( state, &fCpyP );
    
    return fmtState->buf.buf;
}

size_t
fmtLen( State* state ) {
    return state->fmtState->buf.top - 1;
}

char const*
fmtBuf( State* state ) {
    return state->fmtState->buf.buf;
}
