#ifndef ten_upv_h
#define ten_upv_h
#include "ten_types.h"
#include "ten_sym.h"
#include "ten_ptr.h"

struct Upvalue {
    TVal val;
};

#define upvSize( STATE, UPV )  (sizeof(Upvalue))
#define upvTrav( STATE, UPV )  (upvTraverse( STATE, UPV))
#define upvDest( STATE, UPV )

void
upvInit( State* state );

#ifdef ten_TEST
    void
    upvTest( State* state );
#endif

Upvalue*
upvNew( State* state, TVal val );

void
upvTraverse( State* state, Upvalue* upv );

#endif
