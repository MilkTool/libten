#include "ten_cls.h"
#include "ten_fun.h"
#include "ten_state.h"
#include "ten_assert.h"

void
clsInit( State* state ) {
    state->clsState = NULL;
}

Closure*
clsNew( State* state, Function* fun, Data* dat ) {
    tenAssert( fun->type == FUN_NAT || dat == NULL );
    
    Part clsP;
    Closure* cls = stateAllocObj( state, &clsP, sizeof(Closure), OBJ_CLS );
    cls->fun = fun;
    if( fun->type == FUN_NAT ) {
        cls->dat.dat = dat;
        stateCommitObj( state, &clsP );
        return cls;
    }
    
    Part upvalsP;
    Upvalue** upvals = stateAllocRaw(
        state,
        &upvalsP,
        sizeof(Upvalue*)*fun->u.vir.nUpvals
    );
    for( uint i = 0 ; i < fun->u.vir.nUpvals )
        upvals[i] = NULL;
    cls->dat.upvals = upvals;
    
    stateCommitRaw( state, &upvalsP );
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
        stateMark( state, cls->dat.upvals[i] );
}

