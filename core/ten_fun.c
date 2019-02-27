#include "ten_fun.h"
#include "ten_state.h"
#include "ten_ntab.h"
#include "ten_stab.h"

void
funInit( State* state ) {
    state->funState = NULL;
}

Function*
funNewVir( State* state, uint nParams, Index* vargIdx ) {
    Part funP;
    Function* fun = stateAllocObj( state, &funP, sizeof(Function), OBJ_FUN );
    
    fun->type    = FUN_VIR;
    fun->nParams = nParams;
    fun->vargIdx = vargIdx;
    
    memset( &fun->u.vir, sizeof(VirFun) );
    
    stateCommitObj( state, &funP );
    return fun;
}

Function*
funNewNat( State* state, uint nParams, Index* vargIdx, ten_FunCb cb ) {
    Part funP;
    Function* fun = stateAllocObj( state, &funP, sizeof(Function), OBJ_FUN );
    
    fun->type    = FUN_NAT;
    fun->nParams = nParams;
    fun->vargIdx = vargIdx;
    
    memset( &fun->u.nat, sizeof(NatFun) );
    
    stateCommitObj( state, &funP );
    return fun;
}

void
funTraverse( State* state, Function* fun ) {
    if( fun->vargIdx )
        stateTraverse( state, fun->vargIdx );
    
    if( fun->type == FUN_NAT ) {
        if( state->gcFull ) {
            NatFun* nat = &fun->u.nat;
            symMark( state, nat->name );
            for( uint i = 0 ; i < fun->nParams ; i++ )
                symMark( state, nat->params[i] );
        }
    }
    else {
        VirFun* vir = &fun->u.vir;
        for( uint i = 0 ; i < vir->nConsts ; i++ )
            tvMark( vir->consts[i] );
        
        if( vir->dbg ) {
            DbgInfo* dbg = vir->dbg;
            if( state->gcFull ) {
                symMark( state, dbg->func );
                symMark( state, dbg->file );
            }
            for( uint i = 0 ; i < dbg->nLines ; i++ )
                if( dbg->lines[i].bcb )
                    stateMark( state, dbg->lines[i].bcb );
        }
    }
}

void
funDestruct( State* state, Function* fun ) {
    
    if( fun->type == FUN_NAT ) {
        NatFun* nat = &fun->u.nat;
        stateFreeRaw( state, nat->params, sizeof(SymT)*fun->nParams );
    }
    else {
        VirFun* vir = &fun->u.vir;
        stateFreeRaw( state, vir->consts, sizeof(TVal)*vir->nConsts );
        stateFreeRaw( state, vir->labels, sizeof(TVal)*vir->nLabels );
        stateFreeRaw( state, vir->code,   sizeof(instr)*vir->len );
        if( vir->dbg ) {
            DbgInfo* dbg = vir->dbg;
            ntabFree( state, dbg->upvals );
            stabFree( state, dbg->locals );
            stateFreeRaw( state, dbg->lines, sizeof(LineInfo)*dbg->nLines );
        }
    }
}

