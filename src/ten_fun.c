#include "ten_fun.h"
#include "ten_sym.h"
#include "ten_ptr.h"
#include "ten_state.h"
#include "ten_ntab.h"
#include "ten_stab.h"
#include "ten_assert.h"
#include <string.h>

void
funInit( State* state ) {
    state->funState = NULL;
}

#ifdef ten_TEST
void
funTest( State* state ) {
    for( uint i = 0 ; i < 100 ; i++ ) {
        tenAssert( funNewVir( state, 0, NULL ) );
        tenAssert( funNewNat( state, 0, NULL, NULL ) );
    }
}
#endif

#ifdef ten_DEBUG
#include <stdio.h>

#define OP( N, SE ) #N,
char const* opcnames[] = {
    #include "inc/ops.inc"
};
#undef OP

void
funDump( State* state, Function* fun ) {
    tenAssert( fun->type == FUN_VIR );
    
    VirFun* vir = &fun->u.vir;
    printf( "constants:\n" );
    for( uint i = 0 ; i < vir->nConsts; i++ )
        printf( "  %u: %s\n", i, fmtA( state, false, "%q", vir->consts[i] ) );
    printf( "code:\n" );
    for( uint i = 0 ; i < vir->len ; i++ ) {
        char const* opc = opcnames[inGetOpc( vir->code[i] )];
        uint        opr = inGetOpr( vir->code[i] );
        printf( "  %s: %u\n", opc, opr );
    }
}
#endif

Function*
funNewVir( State* state, uint nParams, Index* vargIdx ) {
    Part funP;
    Function* fun = stateAllocObj( state, &funP, sizeof(Function), OBJ_FUN );
    
    fun->type    = FUN_VIR;
    fun->nParams = nParams;
    fun->vargIdx = vargIdx;
    
    memset( &fun->u.vir, 0, sizeof(VirFun) );
    
    stateCommitObj( state, &funP );
    return fun;
}

Function*
funNewNat( State* state, uint nParams, Index* vargIdx, ten_FunCb cb ) {
    Part funP;
    Function* fun = stateAllocObj( state, &funP, sizeof(Function), OBJ_FUN );
    
    fun->type       = FUN_NAT;
    fun->nParams    = nParams;
    fun->vargIdx    = vargIdx;
    
    memset( &fun->u.nat, 0, sizeof(NatFun) );
    fun->u.nat.cb   = cb;
    fun->u.nat.name = symGet( state, "<anon>", 6 );
    
    stateCommitObj( state, &funP );
    return fun;
}

void
funTraverse( State* state, Function* fun ) {
    if( fun->vargIdx )
        stateMark( state, fun->vargIdx );
    
    if( fun->type == FUN_NAT ) {
        if( state->gcFull ) {
            NatFun* nat = &fun->u.nat;
            symMark( state, nat->name );
            if( fun->u.nat.params ) {
                for( uint i = 0 ; i < fun->nParams ; i++ )
                    symMark( state, nat->params[i] );
            }
        }
    }
    else {
        VirFun* vir = &fun->u.vir;
        for( uint i = 0 ; i < vir->nConsts ; i++ )
            tvMark( vir->consts[i] );
        
        
        DbgInfo* dbg = vir->dbg;
        if( vir->dbg && state->gcFull ) {
            symMark( state, dbg->func );
            symMark( state, dbg->file );
        }
    }
}

void
funDestruct( State* state, Function* fun ) {
    
    if( fun->type == FUN_NAT ) {
        NatFun* nat = &fun->u.nat;
        if( nat->params )
            stateFreeRaw( state, nat->params, sizeof(SymT)*fun->nParams );
    }
    else {
        VirFun* vir = &fun->u.vir;
        stateFreeRaw( state, vir->consts, sizeof(TVal)*vir->nConsts );
        stateFreeRaw( state, vir->labels, sizeof(instr*)*vir->nLabels );
        stateFreeRaw( state, vir->code,   sizeof(instr)*vir->len );
        if( vir->dbg ) {
            DbgInfo* dbg = vir->dbg;
            stateFreeRaw( state, dbg->lines, sizeof(LineInfo)*dbg->nLines );
            stateFreeRaw( state, dbg, sizeof(DbgInfo) );
        }
    }
}

