#include "ten_upv.h"
#include "ten_state.h"
#include "ten_assert.h"

void
upvInit( State* state ) {
    state->upvState = NULL;
}

#ifdef ten_TEST
void
upvTest( State* state ) {
    for( uint i = 0 ; i < 100 ; i++ )
        tenAssert( upvNew( state, tvNil() ) );
}
#endif

Upvalue*
upvNew( State* state, TVal val ) {
    Part upvP;
    Upvalue* upv = stateAllocObj( state, &upvP, sizeof(Upvalue), OBJ_UPV );
    upv->val = val;
    stateCommitObj( state, &upvP );
    return upv;
}

void
upvTraverse( State* state, Upvalue* upv ) {
    tvMark( upv->val );
}
