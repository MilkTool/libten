/***********************************************************************
This implements Ten's Upvalue data type; which is invisible to the
language frontent.  It's implemented as a Ten object to allow for the
same garbage collection mechansims; but will never be seen by the
user.  Upvalues are created when a closure is created and references
variables from the parent scope; such variables are elevated above the
runtime stack into an upvalue; and replaced by a reference to the
upvalue.  Subsequent references to the variable have an extra indirection
through the upvalue; but this allows the variable to exist beyond the
lifetime of a particular stack frame.
***********************************************************************/

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

Upvalue*
upvNew( State* state, TVal val );

void
upvTraverse( State* state, Upvalue* upv );

#endif
