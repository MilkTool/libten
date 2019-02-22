#include "ten_ntab.h"
#include "ten_state.h"
#include "ten_tables.h"
#include "ten_assert.h"
#include "ten_macros.h"
#include "ten_sym.h"
#include <limits.h>

typedef struct NameNode {
    struct NameNode*  next;
    struct NameNode** link;
    
    SymT name;
    uint loc;
} NameNode;

typedef struct NTab {
    Scanner   scan;
    
    uint count;
    uint next;
    
    struct {
        uint        row;
        uint        cap;
        NameNode**  buf;
    } map;
    
} NTab;

static void
ntabScan( State* state, Scanner* scan ) {
    if( !state->gcFull )
        return;
    
    NTab* ntab = (NTab*)scan;
    for( uint i = 0 ; i < ntab->map.cap ; i++ ) {
        NameNode* node = ntab->map.buf[i];
        while( node ) {
            symMark( state, node->name );
            node = node->next;
        }
    }
}


NTab*
ntabMake( State* state ) {
    uint mcap = slowGrowthMapCapTable[0];
    
    Part ntabP;
    NTab* ntab = stateAllocRaw( state, &ntabP, sizeof(NTab) );
    
    Part mapP;
    NameNode** map = stateAllocRaw( state, &mapP, sizeof(NameNode*)*mcap );
    for( uint i = 0 ; i < mcap ; i++ )
        map[i] = NULL;
    
    ntab->count   = 0;
    ntab->next    = 0;
    ntab->map.row = 1;
    ntab->map.cap = mcap;
    ntab->map.buf = map;
    ntab->scan.cb = ntabScan;
    stateInstallScanner( state, &ntab->scan );
    
    return ntab;
}

static void
growMap( State* state, NTab* ntab );

void
ntabFree( State* state, NTab* ntab ) {
    stateRemoveScanner( state, &ntab->scan );
    
    stateFreeRaw( state, ntab->map.buf, sizeof(NameNode*)*ntab->map.cap );
    stateFreeRaw( state, ntab, sizeof(NTab));
}

uint
ntabAdd( State* state, NTab* ntab, SymT name ) {
    uint s = name & ntab->map.cap;
    NameNode* node = ntab->map.buf[s];
    while( node ) {
        if( node->name == name )
            return node->loc;
        node = node->next;
    }
    
    // If name node doesn't exist then allocate a new one.
    Part nodeP;
    node = stateAllocRaw( state, &nodeP, sizeof(NameNode) );
    tenAssert( ntab->next < UINT_MAX );
    node->name = name;
    node->loc  = ntab->next++;
    addNode( &ntab->map.buf[s], node );
    stateCommitRaw( state, &nodeP );
    ntab->count++;
    
    if( ntab->count*3 >= ntab->map.cap )
        growMap( state, ntab );
    
    return node->loc;
}

uint
ntabGet( State* state, NTab* ntab, SymT name ) {
    uint s = name & ntab->map.cap;
    NameNode* node = ntab->map.buf[s];
    while( node ) {
        if( node->name == name )
            return node->loc;
        node = node->next;
    }
    
    return UINT_MAX;
}

static void
growMap( State* state, NTab* ntab ) {
    uint mcap;
    if( ntab->map.row < slowGrowthMapCapTableSize )
        mcap = slowGrowthMapCapTable[ntab->map.row++];
    else
        mcap = ntab->map.cap*2;
    
    Part mapP;
    NameNode** map = stateAllocRaw( state, &mapP, sizeof(NameNode*)*mcap );
    for( uint i = 0 ; i < mcap ; i++ )
        map[i] = NULL;
    
    for( uint i = 0 ; i < ntab->map.cap ; i++ ) {
        NameNode* node = ntab->map.buf[i];
        if( !node )
            continue;
        
        uint s = node->name % mcap;
        remNode( node );
        addNode( &map[s], node );
    }
    
    stateFreeRaw( state, ntab->map.buf, sizeof(NameNode*)*ntab->map.cap );
    stateCommitRaw( state, &mapP );
    ntab->map.cap = mcap;
    ntab->map.buf = map;
}
