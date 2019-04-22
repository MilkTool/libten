#include "ten_cls.h"
#include "ten_fun.h"
#include "ten_state.h"
#include "ten_macros.h"
#include "ten_assert.h"

void
clsInit( State* state ) {
    state->clsState = NULL;
}

Closure*
clsNewNat( State* state, Function* fun, Data* dat ) {
    tenAssert( fun->type == FUN_NAT );
    
    Part clsP;
    Closure* cls = stateAllocObj( state, &clsP, sizeof(Closure), OBJ_CLS );
    cls->fun = fun;
    cls->dat.dat = dat;
    stateCommitObj( state, &clsP );
    return cls;
}

Closure*
clsNewVir( State* state, Function* fun, Upvalue** upvals ) {
    tenAssert( fun->type == FUN_VIR );
    
    Part clsP;
    Closure* cls = stateAllocObj( state, &clsP, sizeof(Closure), OBJ_CLS );
    cls->fun = fun;
    
    
    if( !upvals ) {
        Part upvalsP;
        upvals = stateAllocRaw(
            state,
            &upvalsP,
            sizeof(Upvalue*)*fun->u.vir.nUpvals
        );
        for( uint i = 0 ; i < fun->u.vir.nUpvals ; i++ )
            upvals[i] = NULL;
        stateCommitRaw( state, &upvalsP );
    }
    cls->dat.upvals = upvals;
    stateCommitObj( state, &clsP );
    return cls;
}


void
clsTraverse( State* state, Closure* cls ) {
    stateMark( state, cls->fun );
    
    if( cls->fun->type == FUN_NAT ) {
        if( cls->dat.dat )
            stateMark( state, cls->dat.dat );
        return;
    }
    
    for( uint i = 0 ; i < cls->fun->u.vir.nUpvals ; i++ )
        if( cls->dat.upvals[i] )
            stateMark( state, cls->dat.upvals[i] );
}

void
clsDestruct( State* state, Closure* cls ) {
    if( cls->fun->type == FUN_VIR )
        stateFreeRaw( state, cls->dat.upvals, cls->fun->u.vir.nUpvals*sizeof(Upvalue*) );
}

