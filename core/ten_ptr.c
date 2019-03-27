#include "ten_ptr.h"
#include "ten_state.h"
#include "ten_assert.h"
#include "ten_tables.h"
#include "ten_macros.h"
#include <limits.h>

typedef struct PtrNode {
    struct PtrNode*  next;
    struct PtrNode** link;
    
    void*    addr;
    PtrInfo* info;
    bool     mark;
    uint     loc;
} PtrNode;

struct PtrState {
    Finalizer finl;
    
    uint count;
    uint next;
    
    struct {
        uint      row;
        uint      cap;
        PtrNode** buf;
    } map;
    
    struct {
        uint      cap;
        PtrNode** buf;
    } nodes;
    
    PtrNode* recycled;
};

static void
growMap( State* state );

static void
ptrFinl( State* state, Finalizer* finl ) {
    PtrState* ptrState = (PtrState*)finl;
    
    for( uint i = 0 ; i < ptrState->next ; i++ ) {
        if( ptrState->nodes.buf[i] )
            stateFreeRaw( state, ptrState->nodes.buf[i], sizeof(PtrNode) );
    }
    PtrNode* nIt = ptrState->recycled;
    while( nIt ) {
        PtrNode* n = nIt;
        nIt = nIt->next;
        stateFreeRaw( state, n, sizeof(PtrNode) );
    }
    
    stateFreeRaw(
        state,
        ptrState->map.buf,
        sizeof(PtrNode*)*ptrState->map.cap
    );
    stateFreeRaw(
        state,
        ptrState->nodes.buf,
        sizeof(PtrNode*)*ptrState->nodes.cap
    );
    stateFreeRaw( state, ptrState, sizeof(PtrState) );
    state->ptrState = NULL;
}

void
ptrInit( State* state ) {
    uint mcap = slowGrowthMapCapTable[0];
    
    Part stateP;
    PtrState* ptrState = stateAllocRaw( state, &stateP, sizeof(PtrState) );
    
    Part mapP;
    PtrNode** map = stateAllocRaw( state, &mapP, sizeof(PtrNode*)*mcap );
    for( uint i = 0 ; i < mcap ; i++ )
        map[i] = NULL;
    
    uint ncap = 7;
    Part nodesP;
    PtrNode** nodes = stateAllocRaw( state, &nodesP, sizeof(PtrNode*)*ncap );
    for( uint i = 0 ; i < ncap ; i++ )
        nodes[i] = NULL;
    
    ptrState->count     = 0;
    ptrState->next      = 0;
    ptrState->map.row   = 0;
    ptrState->map.cap   = mcap;
    ptrState->map.buf   = map;
    ptrState->nodes.cap = ncap;
    ptrState->nodes.buf = nodes;
    ptrState->recycled  = NULL;
    ptrState->finl.cb   = ptrFinl;
    
    stateInstallFinalizer( state, &ptrState->finl );
    stateCommitRaw( state, &stateP );
    stateCommitRaw( state, &mapP );
    stateCommitRaw( state, &nodesP );
    
    state->ptrState = ptrState;
}


#ifdef ten_TEST
#include "ten_sym.h"
static PtrInfo testInfo;

void
ptrTest( State* state ) {
    testInfo.type  = symGet( state, "Ptr:Test", 4 );
    testInfo.destr = NULL;
    
    for( uint i = 0 ; i < 100 ; i++ )
        ptrGet( state, &testInfo, NULL );
}
#endif

PtrT
ptrGet( State* state, PtrInfo* info, void* addr ) {
    PtrState* ptrState = state->ptrState;
    
    uint h = (ullong)addr;
    uint s = h % ptrState->map.cap;
    
    // Search existing nodes for the address.
    PtrNode* node = ptrState->map.buf[s];
    while( node ) {
        if( node->info == info && node->addr == addr )
            return node->loc;
        node = node->next;
    }
    
    // If a node for the address doesn't exist, then
    // either allocate a new one or recycle one from
    // the `recycled` list.
    if( ptrState->recycled ) {
        node = ptrState->recycled;
        remNode( node );
    }
    else {
        Part nodeP;
        node = stateAllocRaw( state, &nodeP, sizeof(PtrNode) );
        node->addr = NULL;
        node->info = NULL;
        
        tenAssert( ptrState->next < UINT_MAX );
        node->loc = ptrState->next++;
        if( node->loc >= ptrState->nodes.cap ) {
            Part nodesP = {
                .ptr = ptrState->nodes.buf,
                .sz  = ptrState->nodes.cap*sizeof(PtrNode*)
            };
            
            uint cap = ptrState->nodes.cap*2;
            PtrNode** nodes = stateResizeRaw( state, &nodesP, sizeof(PtrNode*)*cap );
            for( uint i = ptrState->nodes.cap ; i < cap ; i++ )
                nodes[i] = NULL;
            
            ptrState->nodes.cap = cap;
            ptrState->nodes.buf = nodes;
            stateCommitRaw( state, &nodesP );
        }
        stateCommitRaw( state, &nodeP );
    }
    
    addNode( &ptrState->map.buf[s], node );
    
    node->addr = addr;
    node->info = info;
    node->mark = false;
    ptrState->nodes.buf[node->loc] = node;
    ptrState->count++;
    
    if( ptrState->count*3 >= ptrState->map.cap )
        growMap( state );
    
    return node->loc;
}

void*
ptrAddr( State* state, PtrT ptr ) {
    PtrState* ptrState = state->ptrState;
    
    tenAssert( ptr < ptrState->next );
    return ptrState->nodes.buf[ptr]->addr;
}

PtrInfo*
ptrInfo( State* state, PtrT ptr ) {
    PtrState* ptrState = state->ptrState;
    
    tenAssert( ptr < ptrState->next );
    return ptrState->nodes.buf[ptr]->info;
}


void
ptrStartCycle( State* state ) {
    // NADA
}

void
ptrMark( State* state, PtrT ptr ) {
    PtrState* ptrState = state->ptrState;
    
    tenAssert( ptr < ptrState->next );
    PtrNode* node = ptrState->nodes.buf[ptr];
    node->mark = true;
}

void
ptrFinishCycle( State* state ) {
    PtrState* ptrState = state->ptrState;
    
    for( uint i = 0 ; i < ptrState->next ; i++ ) {
        PtrNode* node = ptrState->nodes.buf[i];
        if( !node )
            continue;
        if( node->mark ) {
            node->mark = false;
            continue;
        }
        if( node->info && node->info->destr )
            node->info->destr( (ten_State*)state, node->addr );
        
        remNode( node );
        ptrState->nodes.buf[i] = NULL;
        
        node->addr = NULL;
        node->info = NULL;
        addNode( &ptrState->recycled, node );
        
        tenAssert( ptrState->count > 0 );
        ptrState->count--;
    }
}

static void
growMap( State* state ) {
    PtrState* ptrState = state->ptrState;
    
    uint mcap;
    if( ptrState->map.row < slowGrowthMapCapTableSize )
        mcap = slowGrowthMapCapTable[ptrState->map.row++];
    else
        mcap = ptrState->map.cap * 2;
    
    Part mapP;
    PtrNode** map = stateAllocRaw( state, &mapP, sizeof(PtrNode*)*mcap );
    for( uint i = 0 ; i < mcap ; i++ )
        map[i] = NULL;
    
    for( uint i = 0 ; i< ptrState->map.cap ; i++ ) {
        PtrNode* nIt = ptrState->map.buf[i];
        while( nIt ) {
            PtrNode* node = nIt;
            nIt = nIt->next;
            
            uint s = (ullong)node->addr % mcap;
            addNode( &map[s], node );
        }
    }
    
    stateFreeRaw( state, ptrState->map.buf, sizeof(PtrNode*)*ptrState->map.cap );
    stateCommitRaw( state, &mapP );
    ptrState->map.cap = mcap;
    ptrState->map.buf = map;
}
