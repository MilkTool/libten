/***********************************************************************
This component implements Ten's Symbol type, which is an interned
alternative to String which can be compared direcly with the `=`
operator in constant time.  Shorter symbols (<= 4 bytes) can also
be encoded direclty within the `SymT` or `TVal` payload, which is
really efficient and makes symbols work really well as single or
multibyte textual characters.
***********************************************************************/

#ifndef ten_sym_h
#define ten_sym_h
#include "ten_types.h"
#include <stddef.h>
#include <stdbool.h>

void
symInit( State* state );

SymT
symGet( State* state, char const* buf, size_t len );

char const*
symBuf( State* state, SymT sym );

size_t
symLen( State* state, SymT sym );

void
symStartCycle( State* state );

void
symMark( State* state, SymT sym );

void
symFinishCycle( State* state );

#endif
