#include "ten_upv.h"
#include "ten_state.h"
#include "ten_assert.h"

void
upvInit( State* state ) {
    state->upvState = NULL;
}

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
