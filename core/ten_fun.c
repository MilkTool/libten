#include "ten_fun.h"
#include "ten_state.h"
#include "ten_ntab.h"
#include "ten_stab.h"

Function*
funNewVir( State* state, int nParams, DocInfo* doc ) {
    Part funP;
    Function* fun = stateAllocObj( state, &funP, sizeof(Function), OBJ_FUN );
    
    fun->type    = FUN_VIR;
    fun->nParams = nParams;
    fun->doc     = doc;
    
    memset( &fun->u.vir, sizeof(VirFun) );
    
    stateCommitObj( state, &funP );
    return fun;
}

Function*
funNewNat( State* state, int nParams, DocInfo* doc, ten_FunCb cb ) {
    Part funP;
    Function* fun = stateAllocObj( state, &funP, sizeof(Function), OBJ_FUN );
    
    fun->type    = FUN_NAT;
    fun->nParams = nParams;
    fun->doc     = doc;
    
    memset( &fun->u.nat, sizeof(NatFun) );
    
    stateCommitObj( state, &funP );
    return fun;
}

void
funTraverse( State* state, Function* fun ) {
    if( fun->doc )
        stateMark( state, fun->doc->docs );
    
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
            if( state->gcFull ) {
                DbgInfo* dbg = vir->dbg;
                symMark( state, dbg->func );
                symMark( state, dbg->file );
                for( uint i = 0 ; i < dbg->nLines ; i++ )
                    if( dbg->lines[i] )
                        stateMark( state, dbg->lines[i] );
            }
        }
    }
}

void
funDestruct( State* state, Function* fun ) {
    if( fun->doc )
        stateFreeRaw( state, fun->doc, sizeof(DocInfo) );
    
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

