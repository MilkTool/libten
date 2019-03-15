#ifndef ten_sym_h
#define ten_sym_h
#include "ten_types.h"
#include <stddef.h>
#include <stdbool.h>

void
symInit( State* state );

#ifdef ten_TEST
    void
    symTest( State* state );
#endif

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
