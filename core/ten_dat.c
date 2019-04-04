#include "ten_dat.h"
#include "ten_sym.h"
#include "ten_ptr.h"
#include "ten_state.h"
#include "ten_assert.h"

void
datInit( State* state ) {
    state->datState = NULL;
}

#ifdef ten_TEST
#include "ten_sym.h"
static DatInfo testInfo;

void
datTest( State* state ) {
    testInfo.type  = symGet( state, "Dat:Test", 4 );
    testInfo.size  = 50;
    testInfo.nMems = 10;
    testInfo.destr = NULL;
    
    for( uint i = 0 ; i < 100 ; i++ )
        tenAssert( datNew( state, &testInfo ) );
}
#endif

Data*
datNew( State* state, DatInfo* info ) {
    Part datP;
    Data* dat = stateAllocObj( state, &datP, sizeof(Data)+info->size, OBJ_DAT );
    
    Part memsP;
    TVal* mems = stateAllocRaw( state, &memsP, sizeof(TVal)*info->nMems );
    for( uint i = 0 ; i < info->nMems ; i++ )
        mems[i] = tvUdf();
    
    dat->info = info;
    dat->mems = mems;
    stateCommitObj( state, &datP );
    stateCommitRaw( state, &memsP );
    
    return dat;
}

void
datTraverse( State* state, Data* dat ) {
    if( state->gcFull )
        symMark( state, dat->info->type );
    
    for( uint i = 0 ; i < dat->info->nMems ; i++ )
        tvMark( dat->mems[i] );
}

void
datDestruct( State* state, Data* dat ) {
    stateFreeRaw( state, dat->mems, sizeof(TVal)*dat->info->nMems );
    if( dat->info->destr )
        dat->info->destr( (ten_State*)state, dat->data );
}
