#ifndef ten_upv_h
#define ten_upv_h
#include "ten_types.h"
#include "ten_sym.h"
#include "ten_ptr.h"

struct Upvalue {
    TVal* val;
    TVal  buf;
};

#define upvSize( STATE, UPV )  (sizeof(Upvalue))
#define upvTrav( STATE, UPV )  tvMark( *(UPV)->val )
#define upvDest( STATE, UPV )  {}
#define upvClose( STATE, UPV )      \
    do {                            \
        (UPV)->buf = *(UPV)->val;   \
        (UPV)->val = &(UPV)->buf;   \
    } while( 0 )

void
upvInit( State* state );

Upvalue*
upvNew( State* state, TVal* val );

#endif
