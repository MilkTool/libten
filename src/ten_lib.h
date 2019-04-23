#ifndef ten_lib_h
#define ten_lib_h
#include "ten_types.h"
#include <stdbool.h>

void
libInit( State* state );

TVal
libRequire( State* state, String* mod );

TVal
libImport( State* state, String* mod );

SymT
libType( State* state, TVal val );

void
libPanic( State* state, TVal err );

void
libAssert( State* state, TVal cond, TVal what );

void
libExpect( State* state, char const* what, SymT type, TVal val );

void
libCollect( State* state );

void
libLoader( State* state, SymT type, Closure* loadr, Closure* trans );

DecT
libClock( State* state );

DecT
libRand( State* state );

TVal
libLog( State* state, TVal val );

TVal
libInt( State* state, TVal val );

TVal
libDec( State* state, TVal val );

TVal
libSym( State* state, TVal val );

TVal
libStr( State* state, TVal val );

TVal
libHex( State* state, String* str );

TVal
libOct( State* state, String* str );

TVal
libBin( State* state, String* str );

Closure*
libKeys( State* state, Record* rec );

Closure*
libVals( State* state, Record* rec );

Closure*
libPairs( State* state, Record* rec );

Closure*
libSeq( State* state, Record* vals );

Closure*
libBytes( State* state, String* str );

Closure*
libChars( State* state, String* str );

Closure*
libSplit( State* state, String* str, String* sep );

Closure*
libItems( State* state, Record* list );

Closure*
libDrange( State* state, DecT start, DecT end, DecT step );

Closure*
libIrange( State* state, IntT start, IntT end, IntT step );

void
libShow( State* state, Record* vals );

void
libWarn( State* state, Record* vals );

String*
libInput( State* state );

TVal
libUcode( State* state, SymT chr );

TVal
libUchar( State* state, IntT code );

String*
libCat( State* state, Record* rec );

String*
libJoin( State* state, Closure* seq, String* sep );

TVal
libBcmp( State* state, String* str1, SymT opr, String* str2 );

TVal
libCcmp( State* state, String* str1, SymT opr, String* str2 );

String*
libBsub( State* state, String* str, IntT n );

String*
libCsub( State* state, String* str, IntT n );

void
libEach( State* state, Closure* seq, Closure* what );

TVal
libFold( State* state, Closure* seq, TVal agr, Closure* how );


Record*
libSep( State* state, Record* rec );

Record*
libCons( State* state, TVal car, TVal cdr );

Record*
libList( State* state, Record* rec );

Record*
libExplode( State* state, Closure* seq );

Fiber*
libFiber( State* state, Closure* cls, SymT* tag );

Tup
libCont( State* state, Fiber* fib, Record* args );

void
libYield( State* state, Record* args, bool pop );

SymT
libState( State* state, Fiber* fib );

TVal
libErrval( State* state, Fiber* fib );

Record*
libTrace( State* state, Fiber* fib );

Closure*
libScript( State* state, Record* upvals, String* code );

Closure*
libExpr( State* state, Record* upvals, String* code );

#endif
