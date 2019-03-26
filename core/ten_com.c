#include "ten_com.h"
#include "ten_gen.h"
#include "ten_sym.h"
#include "ten_ptr.h"
#include "ten_str.h"
#include "ten_idx.h"
#include "ten_fmt.h"
#include "ten_state.h"
#include "ten_macros.h"
#include "ten_opcodes.h"
#include "ten_assert.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define BUF_TYPE char
#define BUF_NAME CharBuf
    #include "inc/buf.inc"
#undef BUF_TYPE
#undef BUF_NAME

typedef enum {
    TOK_DELIM = 'D',
    TOK_IDENT = 'I',
    TOK_CONST = 'C',
    TOK_END   = 'E'
    // Other tokens are given as multibyte char literals
    // with their respective character sequence, or in
    // the case of keywords as the first two chars of the
    // word.
} TokType;

typedef struct {
    TokType type;
    TVal    value;
    uint    line;
} Token;

struct ComState {
    Finalizer finl;
    Scanner   scan;
    
    ComParams p;
    
    struct {
        // The next character, read from the input source.
        int nChar;
        
        uint line;
        
        // Temporary buffer where we put characters
        // of the token that's currently being parsed
        // in case they need to be converted to a value.
        CharBuf chars;
    } lex;
    
    // The next token, lexed from input characters.
    Token tok;
    
    // The current code generator.
    Gen* gen;
    
    // Temporary pointers and values to keep GC objects.
    void* obj1;
    void* obj2;
    TVal  val1;
    TVal  val2;
    
    // Symbol for special 'this' identifier.
    SymT this;
};

////// Errors /////
static void
errLex( State* state, char const* fmt, ... ) {
    genFree( state, state->comState->gen );
    va_list ap;
    va_start( ap, fmt );
    statePushTrace( state, state->comState->p.file, state->comState->lex.line );
    stateErrFmtV( state, ten_ERR_SYNTAX, fmt, ap );
    va_end( ap );
}

static void
errPar( State* state, char const* fmt, ... ) {
    genFree( state, state->comState->gen );
    va_list ap;
    va_start( ap, fmt );
    statePushTrace( state, state->comState->p.file, state->comState->tok.line );
    stateErrFmtV( state, ten_ERR_SYNTAX, fmt, ap );
    va_end( ap );
}


static void
errLimit( State* state, char const* fmt, ... ) {
    genFree( state, state->comState->gen );
    va_list ap;
    va_start( ap, fmt );
    statePushTrace( state, state->comState->p.file, state->comState->lex.line );
    stateErrFmtV( state, ten_ERR_LIMIT, fmt, ap );
    va_end( ap );
}

static void
errUser( State* state, char const* fmt, ... ) {
    genFree( state, state->comState->gen );
    va_list ap;
    va_start( ap, fmt );
    stateErrFmtV( state, ten_ERR_USER, fmt, ap );
    va_end( ap );
}

static void
errCom( State* state, char const* fmt, ... ) {
    genFree( state, state->comState->gen );
    va_list ap;
    va_start( ap, fmt );
    statePushTrace( state, state->comState->p.file, state->comState->tok.line );
    stateErrFmtV( state, ten_ERR_COMPILE, fmt, ap );
    va_end( ap );
}

///// Lexing /////

static void
advance( State* state ) {
    ComState* com = state->comState;
    if( com->lex.nChar == '\n' )
        com->lex.line++;
    com->lex.nChar = com->p.src->next( com->p.src );
}

static bool
maybeChar( State* state, bool buf, int c ) {
    ComState* com = state->comState;
    if( com->lex.nChar != c )
        return false;
    if( buf )
        *putCharBuf( state, &com->lex.chars ) = com->lex.nChar;
    advance( state );
    return true;
}

static bool
takeChar( State* state, bool buf, int c ) {
    ComState* com = state->comState;
    
    int nChar = com->lex.nChar;
    if( buf )
        *putCharBuf( state, &com->lex.chars ) = nChar;
    advance( state );
    if( nChar != c )
        return false;
    return true;
}

static bool
maybeType( State* state, bool buf, int (*ctype)( int c ) ) {
    ComState* com = state->comState;
    if( !ctype( com->lex.nChar ) )
        return false;
    if( buf )
        *putCharBuf( state, &com->lex.chars ) = com->lex.nChar;
    advance( state );
    return true;
}

static bool
takeType( State* state, bool buf, int (*ctype)( int c ) ) {
    ComState* com = state->comState;
    
    int nChar = com->lex.nChar;
    if( buf )
        *putCharBuf( state, &com->lex.chars ) = nChar;
    advance( state );
    if( !ctype( nChar ) )
        return false;
    return true;
}

#define takeAll( WHAT ) while( WHAT );

static void
resetChars( State* state ) {
    state->comState->lex.chars.top = 0;
}

static bool
lexWord( State* state ) {
    resetChars( state );    
    
    ComState* com  = state->comState;
    uint      line = com->lex.line;
    if( !maybeType( state, true, isalpha ) )
        return false;
    
    takeAll( maybeChar( state, true, '_' ) || maybeType( state, true, isalnum ) );
    *putCharBuf( state, &com->lex.chars ) = '\0';
    
    
    TokType  type;
    TVal     value;
    if( !strcmp( com->lex.chars.buf, "def" ) ) {
        type  = 'de';
        value = tvUdf();
    }
    else
    if( !strcmp( com->lex.chars.buf, "set" ) ) {
        type  = 'se';
        value = tvUdf();
    }
    else
    if( !strcmp( com->lex.chars.buf, "sig" ) ) {
        type  = 'si';
        value = tvUdf();
    }
    else
    if( !strcmp( com->lex.chars.buf, "true" ) ) {
        type  = TOK_CONST;
        value = tvLog( true );
    }
    else
    if( !strcmp( com->lex.chars.buf, "false" ) ) {
        type  = TOK_CONST;
        value = tvLog( false );
    }
    else
    if( !strcmp( com->lex.chars.buf, "nil" ) ) {
        type  = TOK_CONST;
        value = tvNil();
    }
    else
    if( !strcmp( com->lex.chars.buf, "udf" ) ) {
        type  = TOK_CONST;
        value = tvUdf();
    }
    else
    if( !strcmp( com->lex.chars.buf, "if" ) ) {
        type  = 'if';
        value = tvUdf();
    }
    else
    if( !strcmp( com->lex.chars.buf, "else" ) ) {
        type  = 'el';
        value = tvUdf();
    }
    else
    if( !strcmp( com->lex.chars.buf, "do" ) ) {
        type  = 'do';
        value = tvUdf();
    }
    else
    if( !strcmp( com->lex.chars.buf, "for" ) ) {
        type  = 'fo';
        value = tvUdf();
    }
    else
    if( !strcmp( com->lex.chars.buf, "when" ) ) {
        type  = 'wh';
        value = tvUdf();
    }
    else
    if( !strcmp( com->lex.chars.buf, "in" ) ) {
        type  = 'in';
        value = tvUdf();
    }
    else {
        type = TOK_IDENT;
        
        SymT sym = symGet( state, com->lex.chars.buf, com->lex.chars.top );
        value = tvSym( sym );
    }
    
    com->tok.type  = type;
    com->tok.value = value;
    com->tok.line  = line;
    return true;
}

static bool
lexNum( State* state ) {
    resetChars( state );
    
    ComState* com  = state->comState;
    uint      line = com->lex.line;
    if( !maybeType( state, true, isdigit ) )
        return false;
    
    takeAll( maybeChar( state, false, '_' ) || maybeType( state, true, isdigit ) );
    if( !maybeChar( state, true, '.' ) ) {
        *putCharBuf( state, &com->lex.chars ) = '\0';
        IntT in = strtoll( com->lex.chars.buf, NULL, 10 );
        com->tok.type  = TOK_CONST;
        com->tok.value = tvInt( in );
        com->tok.line  = line;
        return true;
    }
    
    takeAll( maybeChar( state, false, '_' ) || maybeType( state, true, isdigit ) );
    if( !maybeChar( state, true, '.' ) )
        errLex( state, "Extra decimal point" );
    *putCharBuf( state, &com->lex.chars ) = '\0';
    DecT dec = strtod( com->lex.chars.buf, NULL );
    com->tok.type  = TOK_CONST;
    com->tok.value = tvDec( dec );
    com->tok.line  = line;
    return true;
}


#define notEOF()     (state->comState->lex.nChar >= 0)
#define hasEOF()     (state->comState->lex.nChar <  0)
#define notNewline() (state->comState->lex.nChar != '\n' && state->comState->lex.nChar != '\r')
#define untilOne( B, Q ) \
    takeAll( notEOF() && notNewline() && !takeChar( state, B, Q ) )
#define untilPair( B, F, S ) \
    takeAll( notEOF() && (!takeChar( state, B, F ) || !maybeChar( state, B, S )) )

    
static bool
lexSym( State* state ) {
    resetChars( state );
    
    ComState* com  = state->comState;
    uint      line = com->lex.line;
    
    if( !maybeChar( state, false, '\'' ) )
        return false;
    
    int trim = 0;
    if( maybeChar( state, false, '|' ) ) {
        untilPair( true, '|', '\'' );
        trim = 2;
    
        if( hasEOF() )
            errLex( state, "Unexpected EOF" );
    }
    else {
        untilOne( true, '\'' );
        trim = 1;
    }
    
    SymT sym = symGet( state, com->lex.chars.buf, com->lex.chars.top - trim );
    com->tok.type  = TOK_CONST;
    com->tok.value = tvSym( sym );
    com->tok.line  = line;
    return true;
}

static bool
lexStr( State* state ) {
    resetChars( state );
    
    ComState* com  = state->comState;
    uint      line = com->lex.line;
    
    if( !maybeChar( state, false, '"' ) )
        return false;
    
    int trim = 0;
    if( maybeChar( state, false, '|' ) ) {
        untilPair( true, '|', '"' );
        trim = 2;
    
        if( hasEOF() )
            errLex( state, "Unexpected EOF" );
    }
    else {
        untilOne( true, '"' );
        trim = 1;
    }
    
    String* str = strNew( state, com->lex.chars.buf, com->lex.chars.top - trim );
    com->tok.type  = TOK_CONST;
    com->tok.value = tvObj( str );
    com->tok.line  = line;
    return true;
}


static void
parExpr( State* state, bool tail );

static bool
lexComment( State* state ) {
    if( !maybeChar( state, false, '`' ) )
        return false;
        
    if( maybeChar( state, false, '|' ) ) {
        untilPair( false, '|', '`' );
        if( hasEOF() )
            errLex( state, "Unexpected EOF" );
    }
    else {
        untilOne( false, '`' );
    }
    return true;
}

static bool
lexOper( State* state ) {
    
    ComState* com  = state->comState;
    uint      line = com->lex.line;
    
    TokType type;
    if( maybeChar( state, false, '@' ) )
        type = '@';
    else
    if( maybeChar( state, false, '.' ) ) {
        if( maybeChar( state, false, '.' ) ) {
            if( maybeChar( state, false, '.' ) )
                type = '..';
            else
                errLex( state, "Incomplete ellipsis" );
        }
        else {
            type = '.';
        }
    }
    else
    if( maybeChar( state, false, '+' ) )
        type = '+';
    else
    if( maybeChar( state, false, '-' ) )
        type = '-';
    else
    if( maybeChar( state, false, '!' ) ) {
        if( maybeChar( state, false, '?' ) )
            type = '!?';
        else
        if( maybeChar( state, false, '=' ) )
            type = '!=';
        else
            type = '!';
    }
    else
    if( maybeChar( state, false, '~' ) ) {
        if( maybeChar( state, false, '=' ) )
            type = '~=';
        else
            type = '~';
    }
    else
    if( maybeChar( state, false, '^' ) )
        type = '^';
    else
    if( maybeChar( state, false, '*' ) )
        type = '*';
    else
    if( maybeChar( state, false, '/' ) )
        type = '/';
    else
    if( maybeChar( state, false, '%' ) )
        type = '%';
    else
    if( maybeChar( state, false, '<' ) ) {
        if( maybeChar( state, false, '<' ) )
            type = '<<';
        else
        if( maybeChar( state, false, '=' ) )
            type = '<=';
        else
            type = '<';
    }
    else
    if( maybeChar( state, false, '>' ) ) {
        if( maybeChar( state, false, '>' ) )
            type = '>>';
        else
        if( maybeChar( state, false, '=' ) )
            type = '>=';
        else
            type = '>';
    }
    else
    if( maybeChar( state, false, '\\' ) )
        type = '\\';
    else
    if( maybeChar( state, false, '|' ) ) {
        if( maybeChar( state, false, '?' ) )
            type = '|?';
        else
            type = '|';
    }
    else
    if( maybeChar( state, false, '&' ) ) {
        if( maybeChar( state, false, '?' ) )
            type = '&?';
        else
            type = '&';
    }
    else
    if( maybeChar( state, false, '=' ) )
        type = '=';
    else
        return false;
    
    com->tok.type  = type;
    com->tok.value = tvUdf();
    com->tok.line  = line;
    return true;
}

static bool
lexOther( State* state ) {
    
    ComState* com  = state->comState;
    uint      line = com->lex.line;
    
    TokType type;
    if( maybeChar( state, false, ':' ) )
        type = ':';
    else
    if( maybeChar( state, false, '(' ) )
        type = '(';
    else
    if( maybeChar( state, false, ')' ) )
        type = ')';
    else
    if( maybeChar( state, false, '[' ) )
        type = '[';
    else
    if( maybeChar( state, false, ']' ) )
        type = ']';
    else
    if( maybeChar( state, false, '{' ) )
        type = '{';
    else
    if( maybeChar( state, false, '}' ) )
        type = '}';
    else
    if( maybeChar( state, false, ',' ) || maybeChar( state, false, '\n' ) )
        type = TOK_DELIM;
    else
    if( maybeChar( state, false, -1 ) )
        type = TOK_END;
    else
        return false;
    
    com->tok.type  = type;
    com->tok.value = tvUdf();
    com->tok.line  = line;
    return true;
}




///// Parsing /////

static void
lex( State* state ) {
    ComState* com = state->comState;
    
    takeAll( maybeType( state, false, isblank ) );
    
    if( lexComment( state ) ) {
        lex( state );
        genSetLine( state, com->gen, com->tok.line );
        return;
    }
    
    bool has =
        lexWord( state )  ||
        lexNum( state )   ||
        lexSym( state )   ||
        lexStr( state )   ||
        lexOper( state )  ||
        lexOther( state );
    if( !has )
        errLex( state, "Unexpected character %c", (char)com->lex.nChar );
        
    genSetLine( state, com->gen, com->tok.line );
}

static void
genInstr( State* state, instr opc, instr opr ) {
    ComState* com = state->comState;
    genPutInstr( state, com->gen, inMake( opc, opr ) );
}

static void
genConst( State* state, TVal val ) {
    ComState* com = state->comState;
    
    com->val1 = val;
    if( tvIsUdf( val ) ) {
        genInstr( state, OPC_LOAD_UDF, 0 );
        return;
    }
    else
    if( tvIsNil( val ) ) {
        genInstr( state , OPC_LOAD_NIL, 0 );
        return;
    }
    else
    if( tvIsLog( val ) ) {
        genInstr( state, OPC_LOAD_LOG, tvGetLog( val ) );
        return;
    }
    else
    if( tvIsInt( val ) ) {
        IntT i = tvGetInt( val );
        if( i <= IN_OPR_MAX ) {
            genInstr( state, OPC_LOAD_INT, i );
            return;
        }
    }
    
    GenConst* c = genAddConst( state, com->gen, val );
    if( c->which > IN_OPR_MAX )
        errLimit( state, "constant count" );
    
    switch( c->which ) {
        case 0:
            genInstr( state, OPC_GET_CONST0, 0 );
        break;
        case 1:
            genInstr( state, OPC_GET_CONST1, 0 );
        break;
        case 2:
            genInstr( state, OPC_GET_CONST2, 0 );
        break;
        case 3:
            genInstr( state, OPC_GET_CONST3, 0 );
        break;
        case 4:
            genInstr( state, OPC_GET_CONST4, 0 );
        break;
        case 5:
            genInstr( state, OPC_GET_CONST5, 0 );
        break;
        case 6:
            genInstr( state, OPC_GET_CONST6, 0 );
        break;
        case 7:
            genInstr( state, OPC_GET_CONST7, 0 );
        break;
        default:
            genInstr( state, OPC_GET_CONST, c->which );
        break;
    }
}

static void
genIndex( State* state ) {
    ComState* com = state->comState;
    
    Index* idx = idxNew( state );
    com->obj1 = idx;
    
    genConst( state, tvObj( idx ) );
}

static void
genGet( State* state, GenVar* var ) {
    ComState* com = state->comState;
    
    if( var->type == VAR_GLOBAL ) {
        if( var->which > IN_OPR_MAX )
            errLimit( state, "global variable count" );
        genPutInstr( state, com->gen, inMake( OPC_GET_GLOBAL, var->which ) );
        return;
    }
    
    if( var->type == VAR_UPVAL ) {
        if( var->which > IN_OPR_MAX )
            errLimit( state, "upvalue variable count" );
        
        switch( var->which ) {
            case 0:
                genPutInstr( state, com->gen, inMake( OPC_GET_UPVAL0, 0 ) );
            break;
            case 1:
                genPutInstr( state, com->gen, inMake( OPC_GET_UPVAL1, 0 ) );
            break;
            case 2:
                genPutInstr( state, com->gen, inMake( OPC_GET_UPVAL2, 0 ) );
            break;
            case 3:
                genPutInstr( state, com->gen, inMake( OPC_GET_UPVAL3, 0 ) );
            break;
            case 4:
                genPutInstr( state, com->gen, inMake( OPC_GET_UPVAL4, 0 ) );
            break;
            case 5:
                genPutInstr( state, com->gen, inMake( OPC_GET_UPVAL5, 0 ) );
            break;
            case 6:
                genPutInstr( state, com->gen, inMake( OPC_GET_UPVAL6, 0 ) );
            break;
            case 7:
                genPutInstr( state, com->gen, inMake( OPC_GET_UPVAL7, 0 ) );
            break;
            default:
                genPutInstr( state, com->gen, inMake( OPC_GET_UPVAL, var->which ) );
            break;
        }
        return;
    }
    if( var->type == VAR_LOCAL ) {
        if( var->which > IN_OPR_MAX )
            errLimit( state, "local variable count" );
        
        switch( var->which ) {
            case 0:
                genPutInstr( state, com->gen, inMake( OPC_GET_LOCAL0, 0 ) );
            break;
            case 1:
                genPutInstr( state, com->gen, inMake( OPC_GET_LOCAL1, 0 ) );
            break;
            case 2:
                genPutInstr( state, com->gen, inMake( OPC_GET_LOCAL2, 0 ) );
            break;
            case 3:
                genPutInstr( state, com->gen, inMake( OPC_GET_LOCAL3, 0 ) );
            break;
            case 4:
                genPutInstr( state, com->gen, inMake( OPC_GET_LOCAL4, 0 ) );
            break;
            case 5:
                genPutInstr( state, com->gen, inMake( OPC_GET_LOCAL5, 0 ) );
            break;
            case 6:
                genPutInstr( state, com->gen, inMake( OPC_GET_LOCAL6, 0 ) );
            break;
            case 7:
                genPutInstr( state, com->gen, inMake( OPC_GET_LOCAL7, 0 ) );
            break;
            default:
                genPutInstr( state, com->gen, inMake( OPC_GET_LOCAL, var->which ) );
            break;
        }
        return;
    }
    if( var->type == VAR_CLOSED ) {
        if( var->which > IN_OPR_MAX )
            errLimit( state, "local variable count" );
        
        switch( var->which ) {
            case 0:
                genPutInstr( state, com->gen, inMake( OPC_GET_CLOSED0, 0 ) );
            break;
            case 1:
                genPutInstr( state, com->gen, inMake( OPC_GET_CLOSED1, 0 ) );
            break;
            case 2:
                genPutInstr( state, com->gen, inMake( OPC_GET_CLOSED2, 0 ) );
            break;
            case 3:
                genPutInstr( state, com->gen, inMake( OPC_GET_CLOSED3, 0 ) );
            break;
            case 4:
                genPutInstr( state, com->gen, inMake( OPC_GET_CLOSED4, 0 ) );
            break;
            case 5:
                genPutInstr( state, com->gen, inMake( OPC_GET_CLOSED5, 0 ) );
            break;
            case 6:
                genPutInstr( state, com->gen, inMake( OPC_GET_CLOSED6, 0 ) );
            break;
            case 7:
                genPutInstr( state, com->gen, inMake( OPC_GET_CLOSED7, 0 ) );
            break;
            default:
                genPutInstr( state, com->gen, inMake( OPC_GET_CLOSED, var->which ) );
            break;
        }
        return;
    }
    
    tenAssertNeverReached();
}

static void
genRef( State* state, GenVar* var ) {
    ComState* com = state->comState;
    
    if( var->type == VAR_GLOBAL ) {
        genPutInstr( state, com->gen, inMake( OPC_REF_GLOBAL, var->which ) );
        return;
    }
    
    if( var->type == VAR_UPVAL ) {
        genPutInstr( state, com->gen, inMake( OPC_REF_UPVAL, var->which ) );
        return;
    }
    if( var->type == VAR_LOCAL ) {
        genPutInstr( state, com->gen, inMake( OPC_REF_LOCAL, var->which ) );
        return;
    }
    if( var->type == VAR_CLOSED ) {
        genPutInstr( state, com->gen, inMake( OPC_REF_CLOSED, var->which ) );
        return;
    }
    
    tenAssertNeverReached();
}

static GenVar*
genVar( State* state, SymT ident, bool def ) {
    ComState* com = state->comState;
    if( ident == com->this )
        errCom( state, "Special identifier 'this' cannot be used as variable name" );
    
    GenVar* var;
    if( def )
        var = genAddVar( state, com->gen, ident );
    else
        var = genGetVar( state, com->gen, ident );
    
    if( var->which > IN_OPR_MAX ) {
        if( var->type == VAR_GLOBAL )
            errLimit( state, "global variable count" );
        else
        if( var->type == VAR_UPVAL )
            errLimit( state, "upvalue variable count" );
        else
            errLimit( state, "local variable count" );
    }
    return var;
}

static GenLbl*
genLbl( State* state, SymT ident ) {
    ComState* com = state->comState;
    
    GenLbl* lbl = genAddLbl( state, com->gen, ident );
    if( lbl->which > IN_OPR_MAX )
        errLimit( state, "label count" );
    return lbl;
}



static bool
parConst( State* state ) {
    ComState* com = state->comState;
    if( com->tok.type != TOK_CONST )
        return false;
    
    genConst( state, com->tok.value );
    lex( state );
    return true;
}

static bool
parGet( State* state ) {
    ComState* com = state->comState;
    if( com->tok.type != TOK_IDENT )
        return false;
    
    SymT name = tvGetSym( com->tok.value );
    GenVar* var = genGetVar( state, com->gen, name );
    genGet( state, var );
    lex( state );
    return true;
}

static bool
parDelim( State* state ) {
    ComState* com = state->comState;
    if( com->tok.type != TOK_DELIM )
        return false;
    while( com->tok.type == TOK_DELIM )
        lex( state );
    return true;
}


typedef bool (*ParseCb)( State* state, void* udat );

static bool
parSequence(
    State*      state,
    TokType     open,
    TokType     close,
    char const* type,
    char const* term,
    void*       udat,
    ParseCb     entry
) {
    ComState* com = state->comState;
    if( com->tok.type != open )
        return false;
    
    // Skip opening token.
    lex( state );
    
    // Skip opening delimiters.
    parDelim( state );
    
    // Parse sub-units.
    while( com->tok.type != close ) {
        if( !entry( state, udat ) )
            errPar( state, "Invalid %s entry", type );
        
        if( com->tok.type != close && !parDelim( state ) )
            errPar( state, "Missing '%s'", term );
    }
    
    // Skip closing token.
    lex( state );
    
    return true;
}

typedef struct {
    uint size;
    bool rexp;
} TupDat;

static bool
parTupleEntry( State* state, void* udat ) {
    ComState* com = state->comState;
    TupDat*   dat = udat;
    
    if( dat->rexp )
        errPar( state, "Extra entries after record expansion" );
    
    dat->size++;
    parExpr( state, false );
    if( com->tok.type == '..' ) {
        dat->rexp = true;
        lex( state );
    }
    
    return true;
}

static bool
parTuple( State* state ) {
    ComState* com = state->comState;
    
    TupDat dat = { .size = 0, .rexp = false };
    bool r = parSequence(
        state,
        '(', ')',
        "tuple",
        ")",
        &dat, parTupleEntry
    );
    if( !r )
        return false;
    
    
    if( dat.size > IN_OPR_MAX )
        errLimit( state, "tuple entry count" );
    if( !dat.rexp && dat.size != 1 )
        genInstr( state, OPC_MAKE_TUP, dat.size );
    else
    if( dat.rexp )
        genInstr( state, OPC_MAKE_VTUP, dat.size );
    return true;
}

static bool
parPrim( State* state, bool tail );

static bool
parValueKey( State* state ) {
    ComState* com = state->comState;
    if( com->tok.type != '@' )
        return false;
    lex( state );
    if( !parPrim( state, false ) )
        errPar( state, "Expected primary expression after '@'" );
    
    return true;
}

static bool
parIdentKey( State* state ) {
    ComState* com = state->comState;
    if( com->tok.type != '.' )
        return false;
    lex( state );
    if( com->tok.type == TOK_IDENT )
        errPar( state, "Expected identifier after '.'" );
    
    genConst( state, com->tok.value );
    return true;
}

static bool
parKey( State* state ) {
    return parValueKey( state ) || parIdentKey( state );
}


typedef struct {
    uint size;
    uint ikey;
    bool rexp;
} RecDat;

static bool
parRecordEntry( State* state, void* udat ) {
    RecDat*   dat = udat;
    ComState* com = state->comState;
    
    if( dat->rexp )
        errPar( state, "Extra entries after record expansion" );
    
    dat->size++;
    if( !parKey( state ) ) {
        genConst( state, tvInt( dat->ikey++ ) );
        parExpr( state, false );
        if( com->tok.type == '..' ) {
            dat->rexp = true;
            lex( state );
        }
        return true;
    }
    
    if( com->tok.type != ':' )
        errPar( state, "Expected ':' after record key" );
    lex( state );
    parDelim( state );
    
    parExpr( state, false );
    return true;
}

static bool
parRecord( State* state ) {
    ComState* com = state->comState;
    
    RecDat dat = { .size = 0, .ikey = 0, .rexp = false };
    bool r = parSequence(
        state,
        '{', '}',
        "record constructor", "}",
        &dat, parRecordEntry
    );
    if( !r )
        return false;
    
    if( dat.size > IN_OPR_MAX )
        errLimit( state, "record constructor entry count" );
    if( !dat.rexp )
        genInstr( state, OPC_MAKE_REC, dat.size );
    else
        genInstr( state, OPC_MAKE_VREC, dat.size );
    return true;
}

typedef struct {
    uint size;
    uint vpar;
} ParamsDat;

static bool
parParam( State* state, void* udat ) {
    ParamsDat* dat = udat;
    ComState*  com = state->comState;
    if( com->tok.type != TOK_IDENT )
        return false;
    
    if( dat->vpar )
        errPar( state, "Extra entries after variadic parameter" );
    
    dat->size++;
    SymT name = tvGetSym( com->tok.value );
    lex( state );
    if( com->tok.type == '..' ) {
        dat->vpar = true;
        lex( state );
    }
    genAddParam( state, com->gen, name, dat->vpar );
    return true;
}

static bool
parClosure( State* state, SymT* name ) {
    ComState* com = state->comState;
    if( com->tok.type != ']' )
        return false;
    Gen* pgen = com->gen;
    Gen* cgen = genMake( state, pgen, name, false, false );
    com->gen = cgen;
    
    ParamsDat dat = { .size = 0, .vpar = false };
    parSequence(
        state,
        '[', ']',
        "parameter list", "]",
        &dat, parParam
    );
    if( dat.size > IN_OPR_MAX )
        errLimit( state, "local variable count" );
    
    // Skip any delimiters after the parameter list.
    parDelim( state );
    
    // Parse result expression.
    parExpr( state, true );
    
    // Return the expression's result.
    genInstr( state, OPC_RETURN, 0 );
    
    // Generate the both the function, and the instructions
    // to construct a closure from it in the parent generator.
    genFinish( state, cgen, true );
    
    com->gen = pgen;
    return true;
}

static bool
parDoClause( State* state, void* udat ) {
    parExpr( state, false );
    genInstr( state, OPC_POP, 0 );
    return true;
}

static bool
parDoExpr( State* state, bool tail ) {
    ComState* com = state->comState;
    if( com->tok.type != 'do' )
        return false;
    
    genOpenScope( state, com->gen );
    
    bool r = parSequence(
        state,
        'do', 'fo',
        "do expression", "for",
        NULL, parDoClause
    );
    
    // Skip delimiters after 'for' keyword.
    parDelim( state );
    
    // Parse result expression.
    parExpr( state, tail );
    
    genCloseScope( state, com->gen );
    
    return true;
}

typedef struct {
    bool    tail;
    uint    size;
    GenLbl* exitLbl;
} IfDat;

static bool
parIfClause( State* state, void* udat ) {
    IfDat*    dat = udat;
    ComState* com = state->comState;
    
    dat->size++;
    
    // This is the label we jump to if the predicate
    // for the current clause fails.  For now we just
    // allocate it, its position will be set later.
    char const* altStr = fmtA( state, false, "$%u", (uint)dat->size );
    SymT        altSym = symGet( state, altStr, fmtLen( state ) );
    GenLbl*     altLbl = genLbl( state, altSym );
    
    // Predicate expression.
    parExpr( state, false );
    
    // Emit an ALT_JUMP, which uses the top stack value
    // as a preposition, jumping to the given label if
    // this value is nil or false.  Whether a jump occurs
    // or not this will pop the top from the stack.
    genInstr( state, OPC_ALT_JUMP, altLbl->which );
    
    // Expect a colon to separate the predicate and consequent.
    if( com->tok.type != ':' )
        errPar( state, "Expected ':' after predicate" );
    lex( state );
    
    // Skip delimiters after colon.
    parDelim( state );
    
    // Consequent expression.
    parExpr( state, dat->tail );
    
    // If consequent was evaluated then jump to the
    // end of the conditional.
    genInstr( state, OPC_JUMP, dat->exitLbl->which );
    
    // Next alternative will follow this one, so
    // set altLbl to refer to the next instruction.
    uint place = genGetPlace( state, com->gen );
    genMovLbl( state, com->gen, altLbl, place );
    
    return true;
}

static bool
parIfExpr( State* state, bool tail ) {
   ComState* com = state->comState;
    if( com->tok.type != 'if' )
        return false;
    
    genOpenLblScope( state, com->gen );
    
    // Exit label, this will be set to the end of the
    // conditional code.
    SymT    exitSym = symGet( state, "$e", 2 );
    GenLbl* exitLbl = genLbl( state, exitSym );
    
    IfDat dat = { .tail = tail, .size = 0, .exitLbl = exitLbl };
    parSequence(
        state,
        'if', 'el',
        "if expression", "else",
        &dat, parIfClause
    );
    
    // Skip any delimiters after the 'else' keyword.
    parDelim( state );
    
    // Parse the else expression.
    parExpr( state, tail );
    
    // Set the exitLbl to refer to the end of the expression.
    uint place = genGetPlace( state, com->gen );
    genMovLbl( state, com->gen, exitLbl, place );
    
    genCloseLblScope( state, com->gen );
    return true;
}

typedef struct {
    bool    tail;
    GenLbl* exitLbl;
} WhenDat;


static bool
parSigParam( State* state, void* udat ) {
    ParamsDat* dat = udat;
    ComState*  com = state->comState;
    if( com->tok.type != TOK_IDENT )
        return false;
    
    if( dat->vpar )
        errPar( state, "Extra entries after variadic parameter" );
    
    dat->size++;
    SymT ident = tvGetSym( com->tok.value );
    GenVar* var = genVar( state, ident, true );
    genRef( state, var );
    
    lex( state );
    if( com->tok.type == '..' ) {
        dat->vpar = true;
        lex( state );
    }
    
    return true;
}

static bool
parWhenClause( State* state, void* udat ) {
    WhenDat*  dat = udat;
    ComState* com = state->comState;
    
    if( com->tok.type != TOK_IDENT )
        return false;
    
    // The handler's label is used to jump to this location
    // when the corresponding signal is invoked.
    genLbl( state, tvGetSym( com->tok.value ) );
    
    // Each signal handler has its own scope.
    genOpenScope( state, com->gen );
    
    
    lex( state );
    if( com->tok.type != '(' )
        errPar( state, "Expected signal parameter list" );
    
    ParamsDat pdat = { .size = 0, .vpar = false };
    parSequence(
        state,
        '(', ')',
        "signal parameters",
        ")",
        &pdat, parSigParam
    );
    if( pdat.size > IN_OPC_MAX )
        errLimit( state, "signal parameter count" );
    
    if( pdat.vpar ) {
        genIndex( state );
        genInstr( state, OPC_DEF_VSIG, pdat.size - 1 );
    }
    else {
        genInstr( state, OPC_DEF_SIG, pdat.size );
    }
    
    if( com->tok.type != ':' )
        errPar( state, "Expected ':' after signal parameters" );
    lex( state );
    
    parExpr( state, dat->tail );
    genInstr( state, OPC_JUMP, dat->exitLbl->which );
    
    genCloseScope( state, com->gen );
    return true;
}

static bool
parWhenExpr( State* state, bool tail ) {
   ComState* com = state->comState;
    if( com->tok.type != 'if' )
        return false;
    
    genOpenLblScope( state, com->gen );
    
    // Exit label, this will be set to the end of the
    // expression, after the 'in' clause.
    SymT    exitSym = symGet( state, "$e", 2 );
    GenLbl* exitLbl = genLbl( state, exitSym );
    
    // This label will be set to the start of the 'in' clause.
    SymT    inSym = symGet( state, "$i", 2 );
    GenLbl* inLbl = genLbl( state, inSym );
    
    
    // Jump over all the signal handler clauses to the 'in'
    // clause, we only jump back to a signal handler if the
    // respective signal is invoked.
    genInstr( state, OPC_JUMP, inLbl->which );
    
    
    WhenDat dat = { .tail = tail, .exitLbl = exitLbl };
    parSequence(
        state,
        'wh', 'in',
        "when expression", "in",
        &dat, parWhenClause
    );
    
    // Skip any delimiters after the 'else' keyword.
    parDelim( state );
    
    // Move the inLbl to point to the next instruction
    // which is where the 'in' clause will start.
    uint inPlace = genGetPlace( state, com->gen );
    genMovLbl( state, com->gen, inLbl, inPlace );
    
    
    // Parse the in expression.
    parExpr( state, tail );
    
    // Set the exit label to refer to the end of the expression.
    uint endPlace = genGetPlace( state, com->gen );
    genMovLbl( state, com->gen, exitLbl, endPlace );
    
    genCloseLblScope( state, com->gen );
    return true;
}

static bool
parPrim( State* state, bool tail ) {
    return parConst( state )         ||
           parGet( state )           ||
           parTuple( state )         ||
           parRecord( state )        ||
           parClosure( state, NULL ) ||
           parDoExpr( state, tail )  ||
           parIfExpr( state, tail )  ||
           parWhenExpr( state, tail );
}

static void
finPath( State* state ) {
    ComState* com = state->comState;
    if( com->tok.type == '.' ) {
        lex( state );
        if( com->tok.type != TOK_IDENT )
            errPar( state, "Expected identifier after '.'" );
        genConst( state, com->tok.value );
        genInstr( state, OPC_GET_FIELD, 0 );
        
        lex( state );
        finPath( state );
    }
    else
    if( com->tok.type == '@' ) {
        lex( state );
        if( !parPrim( state, false ) )
            errPar( state, "Unexpected token" );
        genInstr( state, OPC_GET_FIELD, 0 );
        finPath( state );
    }
}

static bool
parPath( State* state ) {
    if( !parPrim( state, false ) )
        return false;
    finPath( state );
    return true;
}

typedef struct {
    bool tail;
} OperDat;

static bool
parCall( State* state, void* udat ) {
    OperDat* dat = udat;
    
    if( !parPath( state ) )
        errPar( state, "Expected path or primary expression" );
    
    while( parPath( state ) ) {
        genInstr( state, OPC_CALL, 0 );
    }
    
    // If this is a tailcall then add a RETURN
    // instruction after the last CALL instruction,
    // this tells the VM to optimize it as a tail call.
    if( dat->tail && state->comState->tok.type == TOK_DELIM )
        genInstr( state, OPC_RETURN, 0 );
    
    return true;
}


typedef struct {
    TokType tok;
    OpCode  oper;
} OperCode;

typedef void (*SubCb)( State* state );

static OpCode
matchOpCode( State* state, OperCode** opers ) {
    for( uint i = 0 ; opers[i] != NULL ; i++ ) {
        if( opers[i]->tok == state->comState->tok.type ) {
            lex( state );
            return opers[i]->oper;
        }
    }
    return OPC_LAST;
}

static void
parBinaryOper(
    State*      state,
    OperCode**  opers,
    void*       udat,
    ParseCb     sub
) {
    sub( state, udat );
    
    OpCode opc = matchOpCode( state, opers );
    while( opc != OPC_LAST ) {
        parDelim( state );
        
        sub( state, udat );
        genInstr( state, opc, 0 );
        opc = matchOpCode( state, opers );
    }
}

static void
parUnaryOper(
    State*      state,
    OperCode**  opers,
    void*       udat,
    ParseCb     sub
) {
    OpCode opc = matchOpCode( state, opers );
    if( opc != OPC_LAST )
        parDelim( state );
    
    sub( state, udat );
    if( opc != OPC_LAST ) {
        genInstr( state, opc, 0 );
    }
}

static bool
parUnary( State* state, void* udat );

static bool
parExponent( State* state, void* udat ) {
    OperCode powOper = { '^', OPC_POW };
    
    OperCode* opers[] = { &powOper, NULL };
    parBinaryOper( state, opers, udat, parCall );
    return true;
}

static bool
parUnary( State* state, void* udat ) {
    OperCode notOper = { '~', OPC_NOT };
    OperCode fixOper = { '!', OPC_FIX };
    OperCode negOper = { '-', OPC_NEG };
    
    OperCode* opers[] = { &notOper, &fixOper, &negOper, NULL };
    parUnaryOper( state, opers, udat, parExponent );
    return true;
}

static bool
parProduct( State* state, void* udat ) {
    OperCode mulOper = { '*', OPC_MUL };
    OperCode divOper = { '/', OPC_DIV };
    OperCode modOper = { '%', OPC_MOD };
    
    OperCode* opers[] = { &mulOper, &divOper, &modOper, NULL };
    parBinaryOper( state, opers, udat, parUnary );
    return true;
}

static bool
parSummation( State* state, void* udat ) {
    OperCode addOper = { '+', OPC_ADD };
    OperCode subOper = { '-', OPC_SUB };
    
    OperCode* opers[] = { &subOper, &addOper, NULL };
    parBinaryOper( state, opers, udat, parProduct );
    return true;
}

static bool
parShift( State* state, void* udat ) {
    OperCode lslOper = { '<<', OPC_LSL };
    OperCode lsrOper = { '>>', OPC_LSR };
    
    OperCode* opers[] = { &lslOper, &lsrOper, NULL };
    parBinaryOper( state, opers, udat, parSummation );
    return true;
}

static bool
parLogical( State* state, void* udat ) {
    OperCode andOper = { '&', OPC_MUL };
    OperCode xorOper = { '\\', OPC_DIV };
    OperCode orOper  = { '|', OPC_MOD };
    
    OperCode* opers[] = { &andOper, &xorOper, &orOper, NULL };
    parBinaryOper( state, opers, udat, parShift );
    return true;
}

static bool
parCompare( State* state, void* udat ) {
    OperCode iltOper  = { '<', OPC_ILT };
    OperCode imtOper  = { '>', OPC_IMT };
    OperCode ileOper  = { '<=', OPC_ILE };
    OperCode imeOper  = { '>=', OPC_IME };
    OperCode ietOper  = { '=', OPC_IET };
    OperCode netOper  = { '~=', OPC_NET };
    OperCode ietuOper = { '!=', OPC_IETU };
    
    OperCode* opers[] = {
        &iltOper,
        &imtOper,
        &ileOper,
        &imeOper,
        &ietOper,
        &netOper,
        &ietuOper,
        NULL
    };
    parBinaryOper( state, opers, udat, parLogical );
    return true;
}

static void
parConditional( State* state, bool tail ) {
    ComState* com = state->comState;
    
    OperCode aCondOper = { '&?', OPC_AND_JUMP };
    OperCode oCondOper = { '|?', OPC_OR_JUMP };
    OperCode uCondOper = { '!?', OPC_UDF_JUMP };
    
    OperCode* opers[] = {
        &aCondOper,
        &oCondOper,
        &uCondOper,
        NULL
    };
    
    OperDat dat = { .tail = tail };
    parCompare(  state, &dat );
    
    OpCode opc = matchOpCode( state, opers );
    
    if( opc != OPC_LAST ) {
        SymT    exitSym = symGet( state, "$e", 2 );
        GenLbl* exitLbl = genLbl( state, exitSym );
        
        while( opc != OPC_LAST ) {
            genInstr( state, opc, exitLbl->which );
            parCompare( state, &dat );
            
            opc = matchOpCode( state, opers );
        }
        uint place = genGetPlace( state, com->gen );
        genMovLbl( state, com->gen, exitLbl, place );
    }
}

typedef struct {
    uint size;
    bool def;
    bool vtup;
} DstTupDat;


typedef struct {
    uint size;
    uint ikey;
    bool def;
    bool vrec;
} DstRecDat;

static bool
parVarTupEntry( State* state, void* udat ) {
    ComState* com = state->comState;
    
    DstTupDat* dat = udat;
    if( dat->vtup )
        errPar( state, "Extra variables after '...'" );
    
    dat->size++;
    if( com->tok.type != TOK_IDENT )
        errPar( state, "Expected identifier" );
    
    SymT ident = tvGetSym( com->tok.value );
    GenVar* var = genVar( state, ident, dat->def );
    
    genRef( state, var );
    
    lex( state );
    if( com->tok.type == '..' ) {
        dat->vtup = true;
        lex( state );
    }
    return true;
}

static instr
parVarTup( State* state, bool def ) {
    DstTupDat dat = { .size = 0, .def = def, .vtup = false };
    parSequence(
        state,
        '(', ')',
        "variable tuple",
        ")",
        &dat, parVarTupEntry
    );
    if( dat.size > IN_OPC_MAX )
        errLimit( state, "variable tuple entry count" );
    
    if( dat.vtup ) {
        genIndex( state );
        if( def )
            return inMake( OPC_DEF_VTUP, dat.size - 1 );
        else
            return inMake( OPC_SET_VTUP, dat.size - 1 );
    }
    else {
        if( def )
            return inMake( OPC_DEF_TUP, dat.size );
        else
            return inMake( OPC_SET_TUP, dat.size );
    }
}

static bool
parVarRecEntry( State* state, void* udat ) {
    ComState* com = state->comState;
    
    DstRecDat* dat = udat;
    if( dat->vrec )
        errPar( state, "Extra variables after '...'" );
    
    dat->size++;
    if( com->tok.type != TOK_IDENT )
        errPar( state, "Expected identifier" );
    
    SymT ident = tvGetSym( com->tok.value );
    GenVar* var = genVar( state, ident, dat->def );
    
    genRef( state, var );
    
    lex( state );
    if( com->tok.type == '..' ) {
        dat->vrec = true;
        lex( state );
    }
    else
    if( com->tok.type == ':' ) {
        lex( state );
        parKey( state );
    }
    else {
        genConst( state, tvInt( dat->ikey++ ) );
    }
    return true;
}

static instr
parVarRec( State* state, bool def ) {
    DstRecDat dat = { .size = 0, .ikey = 0, .def = def, .vrec = false };
    parSequence(
        state,
        '(', ')',
        "variable tuple",
        ")",
        &dat, parVarRecEntry
    );
    if( dat.size > IN_OPC_MAX )
        errLimit( state, "variable record entry count" );
    
    if( dat.vrec ) {
        genIndex( state );
        if( def )
            return inMake( OPC_DEF_VREC, dat.size - 1 );
        else
            return inMake( OPC_SET_VREC, dat.size - 1 );
    }
    else {
        if( def )
            return inMake( OPC_DEF_REC, dat.size );
        else
            return inMake( OPC_SET_REC, dat.size );
    }
}



static bool
parKeyTupEntry( State* state, void* udat ) {
    ComState* com = state->comState;
    
    DstTupDat* dat = udat;
    if( dat->vtup )
        errPar( state, "Extra keys after '...'" );
    
    dat->size++;
    if( !parKey( state ) )
        errPar( state, "Expected record key" );
    
    if( com->tok.type == '..' ) {
        dat->vtup = true;
        lex( state );
    }
    return true;
}

static instr
parKeyTup( State* state, bool def ) {
    DstTupDat dat = { .size = 0, .def = def, .vtup = false };
    parSequence(
        state,
        '(', ')',
        "key tuple",
        ")",
        &dat, parKeyTupEntry
    );
    if( dat.size > IN_OPC_MAX )
        errLimit( state, "key tuple entry count" );
    
    if( dat.vtup ) {
        genIndex( state );
        if( def )
            return inMake( OPC_REC_DEF_VTUP, dat.size - 1 );
        else
            return inMake( OPC_REC_SET_VTUP, dat.size - 1 );
    }
    else {
        if( def )
            return inMake( OPC_REC_DEF_TUP, dat.size );
        else
            return inMake( OPC_REC_SET_TUP, dat.size );
    }
}

static bool
parKeyRecEntry( State* state, void* udat ) {
    ComState* com = state->comState;
    
    DstRecDat* dat = udat;
    if( dat->vrec )
        errPar( state, "Extra keys after '...'" );
    
    dat->size++;
    if( !parKey( state ) )
        errPar( state, "Expected record key" );
    
    if( com->tok.type == '..' ) {
        dat->vrec = true;
        lex( state );
    }
    else
    if( com->tok.type == ':' ) {
        lex( state );
        parKey( state );
    }
    else {
        genConst( state, tvInt( dat->ikey++ ) );
    }
    return true;
}

static instr
parKeyRec( State* state, bool def ) {
    DstRecDat dat = { .size = 0, .ikey = 0, .def = def, .vrec = false };
    parSequence(
        state,
        '(', ')',
        "variable tuple",
        ")",
        &dat, parKeyRecEntry
    );
    if( dat.size > IN_OPC_MAX )
        errLimit( state, "key record entry count" );
    
    if( dat.vrec ) {
        genIndex( state );
        if( def )
            return inMake( OPC_REC_DEF_VREC, dat.size - 1 );
        else
            return inMake( OPC_REC_SET_VREC, dat.size - 1 );
    }
    else {
        if( def )
            return inMake( OPC_REC_DEF_REC, dat.size );
        else
            return inMake( OPC_REC_SET_REC, dat.size );
    }
}

static instr
finFieldDst( State* state, bool def ) {
    ComState* com = state->comState;
    
    if( !parKey( state ) )
        errPar( state, "Expected field pattern or ':'" );
    while( com->tok.type == '@' || com->tok.type == '.' ) {
        genInstr( state, OPC_GET_FIELD, 0 );
        if( !parKey( state ) )
            errPar( state, "Expected field pattern or ':'" );
    }
    if( com->tok.type == '(' ) {
        parKey( state );
        return parKeyTup( state, def );
    }
    else
    if( com->tok.type == '{' ) {
        parKey( state );
        return parKeyRec( state, def );
    }
    else {
        if( def )
            return inMake( OPC_REC_DEF_ONE, 0 );
        else
            return inMake( OPC_REC_SET_ONE, 0 );
    }
}

static bool
parAssign( State* state ) {
    ComState* com = state->comState;
    
    bool def;
    if( com->tok.type == 'de' )
        def = true;
    else
    if( com->tok.type == 'se' )
        def = false;
    else
        return false;
    
    lex( state );
    
    // Parse the destination pattern, the respective
    // function for parsing each type of pattern will
    // return the instruction to use for the actual
    // assignment.
    instr in = 0;
    if( com->tok.type == '(' ) {
        in = parVarTup( state, def );
    }
    else
    if( com->tok.type == '{' ) {
        in = parVarRec( state, def );
    }
    else
    if( com->tok.type == TOK_IDENT ) {
        SymT ident = tvGetSym( com->tok.value );
        lex( state );
        if( com->tok.type == ':' ) {
            GenVar* var = genVar( state, ident, def );
            genRef( state, var );
            if( def )
                in = inMake( OPC_DEF_ONE, 0 );
            else
                in = inMake( OPC_SET_ONE, 0 );
        }
        else {
            GenVar* var = genVar( state, ident, false );
            genGet( state, var );
            in = finFieldDst( state, def );
        }
    }
    else
    if( parPrim( state, false ) ) {
        if( com->tok.type == ':' )
            errPar( state, "Expected variable or field pattern before ':'" );
        in = finFieldDst( state, def );
    }
    else {
        errPar( state, "Expected variable or field pattern" );
    }
    
    if( com->tok.type != ':' )
        errPar( state, "Expected ':' after assignment pattern" );
    
    // Parse the source expression.
    parExpr( state, false );
    
    
    // Add the assignment instruction.
    genPutInstr( state, com->gen, in );
    
    // Pop the `udf` returned by the assignment instruction.
    genInstr( state, OPC_POP, 0 );
    
    return true;
}

static bool
parSignal( State* state ) {
    ComState* com = state->comState;
    if( com->tok.type != 'si' )
        return false;
    lex( state );
    
    if( com->tok.type != TOK_IDENT )
        errPar( state, "Expected identifier after 'sig'" );
    
    GenLbl* lbl = genLbl( state, tvGetSym( com->tok.value ) );
    if( !lbl )
        errCom( state, "No signal handler for '%v' in scope", com->tok.type );
    
    lex( state );
    if( com->tok.type != ':' )
        errPar( state, "Expected ':' after signal name" );
    lex( state );
    
    parExpr( state, false );
    genInstr( state, OPC_JUMP, lbl->which );
    return true;
}

static void
parExpr( State* state, bool tail ) {
    if( parAssign( state ) )
        return;
    if( parSignal( state ) )
        return;
    parConditional( state, tail );
}


///// Interface /////

static void
comFinl( State* state, Finalizer* finl ) {
    ComState* com = structFromFinl( ComState, finl );
    
    stateRemoveScanner( state, &com->scan );
    finlCharBuf( state, &com->lex.chars );
    stateFreeRaw( state, com, sizeof(ComState) );
}

static void
comScan( State* state, Scanner* scan ) {
    ComState* com = structFromScan( ComState, scan );
    
    if( com->obj1 )
        stateMark( state, com->obj1 );
    if( com->obj2 )
        stateMark( state, com->obj2 );
    
    tvMark( com->val1 );
    tvMark( com->val2 );
    if( state->gcFull )
        symMark( state, com->this );
}

void
comInit( State* state ) {
    Part comP;
    ComState* com = stateAllocRaw( state, &comP, sizeof(ComState) );
    memset( com, 0, sizeof(ComState) );
    
    com->finl.cb = comFinl;
    com->scan.cb = comScan;
    com->obj1    = NULL;
    com->obj2    = NULL;
    com->val1    = tvUdf();
    com->val2    = tvUdf();
    com->this    = symGet( state, "this", 4 );
    
    initCharBuf( state, &com->lex.chars );
    
    stateInstallFinalizer( state, &com->finl );
    stateInstallScanner( state, &com->scan );
    
    stateCommitRaw( state, &comP );
    
    state->comState = com;
}

#ifdef ten_TEST
typedef struct {
    ten_Source  src;
    char const* str;
    size_t      loc;
} Source;

int
srcNext( Source* src ) {
    if( src->str[src->loc] == '\0' )
        return -1;
    return src->str[src->loc++];
}

void
srcInit( Source* src, char const* code ) {
    src->src.next  = (void*)srcNext;
    src->str = code;
    src->loc = 0;
}

void
comTest( State* state ) {
    Source src1; srcInit( &src1, "a + b, a - b" );
    ComParams p1 = {
        .file   = "test.ten",
        .params = NULL,
        .debug  = true,
        .global = true,
        .script = true,
        .src    = &src1.src
    };
    comCompile( state, &p1 );
    
    
    Source src2; srcInit( &src2, "a + b" );
    ComParams p2 = {
        .file   = "test.ten",
        .params = (char const*[]){ "a", "b", NULL },
        .debug  = true,
        .global = false,
        .script = false,
        .src    = &src2.src
    };
    comCompile( state, &p2 );
}
#endif

Function*
comCompile( State* state, ComParams* params ) {
    ComState* com = state->comState;
    
    com->p = *params;
    com->obj1    = NULL;
    com->obj2    = NULL;
    com->val1    = tvUdf();
    com->val2    = tvUdf();
    
    com->gen = genMake( state, NULL, NULL, params->global, params->debug );
    
    com->lex.nChar = params->src->next( params->src );
    com->lex.line  = 1;
    lex( state );
    
    bool vpar = false;
    if( params->params ) {
        for( uint i = 0 ; params->params[i] != NULL ; i++ ) {
            if( vpar )
                errUser( state, "Extra entries after variadic parameter" );
            
            char const* str = params->params[i];
            size_t      len = strlen( str );
            
            SymT sym  = symGet( state, str, len );
            com->val1 = tvSym( sym );
            if( len > 3 && !strcmp( str + len - 3, "..." ) ) {
                vpar = true;
            }
            genAddParam( state, com->gen, sym, vpar );
        }
    }
    
    if( params->script ) {
        parDelim( state );
        while( com->tok.type != TOK_END ) {
            parExpr( state, false );
            if( com->tok.type != TOK_END && !parDelim( state ) )
                errPar( state, "Expected delimiter or EOF" );
        }
    }
    else {
        parExpr( state, false );
    }
    return genFinish( state, com->gen, false );
}
