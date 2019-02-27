#include "ten_fmt.h"
#include "ten_sym.h"
#include "ten_state.h"
#include "ten_assert.h"
#include "ten_macros.h"
#include <string.h>

#define BUF_TYPE char
#define BUF_NAME CharBuf
    #include "inc/buf.inc"
#undef BUF_TYPE
#undef BUF_NAME

struct FmtState {
    Finalizer finl;
    Scanner   scan;
    
    CharBuf buf;
};

void
fmtInit( State* state ) {
    Part fmtP;
    FmtState* fmt = stateAllocRaw( state, &fmtP, sizeof(FmtState) );
    initCharBuf( state, &fmt->buf );
    stateCommitRaw( state, &fmtP );
    state->fmtState = fmt;
    
    *putCharBuf( state, &fmt->buf ) = '\0';
}

void
fmtA( State* state, bool append, char const* fmt, ... ) {
    va_list ap;
    va_start( ap, fmt );
    fmtV( state, append, fmt, ap );
    va_end( ap );
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
    len = vsnprintf( fmtState->buf.buf, fmtState->buf.cap, fmy, ap );
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
    strcpy( fmtState->buf, str );
    fmtState->buf.top += len;
}



void
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
        while( c[i] && ( c[i] != '%' || c[i+1] != 'v' || c[i+1] != 't' ) )
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
                fmtStrVal( state, va_arg( ap, TVal ) );
            break;
            case 't':
                fmtValType( state, va_arg( ap, TVal ) );
            break;
            case 'V':
                fmtTenVal( state, va_arg( ap, TVal ) );
            break;
            case 'T':
                fmtTagType( state, va_arg( ap, int ) );
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
}

size_t
fmtLen( State* state );

char const*
fmtBuf( State* state );
