#include "ten_dat.h"
#include "ten_sym.h"
#include "ten_ptr.h"
#include "ten_fmt.h"
#include "ten_macros.h"
#include "ten_state.h"
#include "ten_assert.h"

#include <string.h>


struct DatState {
    Finalizer finl;
    Scanner   scan;
    DatInfo*  infos;
};

static void
datScan( State* state, Scanner* scan ) {
    DatState* dat = structFromScan( DatState, scan );
    
    if( !state->gcFull )
        return;
    
    DatInfo* it = dat->infos;
    while( it ) {
        tvMark( it->typeVal );
        it = it->next;
    }
}

static void
datFinl( State* state, Finalizer* finl ) {
    DatState* dat = structFromFinl( DatState, finl );
    
    stateRemoveScanner( state, &dat->scan );
    
    DatInfo* dIt = dat->infos;
    while( dIt ) {
        DatInfo* d = dIt;
        dIt = dIt->next;
        
        stateFreeRaw( state, d, sizeof(DatInfo) );
    }
    stateFreeRaw( state, dat, sizeof(DatState) );
}

void
datInit( State* state ) {
    Part datP;
    DatState* dat = stateAllocRaw( state, &datP, sizeof(DatState) );
    dat->scan.cb = datScan;
    dat->finl.cb = datFinl;
    dat->infos   = NULL;
    
    stateInstallScanner( state, &dat->scan );
    stateInstallFinalizer( state, &dat->finl );
    
    stateCommitRaw( state, &datP );
    state->datState = dat;
}

DatInfo*
datAddInfo( State* state, ten_DatConfig* config ) {
    char const* type;
    if( config->tag )
        type = fmtA( state, false, "Dat:%s", config->tag );
    else
        type = "Dat";
    
    Part infoP;
    DatInfo* info = stateAllocRaw( state, &infoP, sizeof(DatInfo ) );
    
    info->typeVal = tvSym( symGet( state, type, strlen( type ) ) );
    info->typePtr = &info->typeVal;
    info->typeTup = (Tup){ .base = &info->typePtr, .offset = 0, .size = 1 };
    info->typeVar = (ten_Var){ .tup = (ten_Tup*)&info->typeTup, .loc = 0 };
    info->size  = config->size;
    info->nMems = config->mems;
    info->destr = config->destr;
    info->next  = state->datState->infos;
    state->datState->infos = info;
    
    stateCommitRaw( state, &infoP );
    return info;
}

#ifdef ten_TEST
#include "ten_sym.h"

void
datTest( State* state ) {
    ten_DatConfig cfg = {
        .tag   = "Test",
        .size  = 50,
        .mems  = 10,
        .destr = NULL
    };
    
    DatInfo* testInfo = datAddInfo( state, &cfg );
    for( uint i = 0 ; i < 100 ; i++ )
        tenAssert( datNew( state, testInfo ) );
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
    for( uint i = 0 ; i < dat->info->nMems ; i++ )
        tvMark( dat->mems[i] );
}

void
datDestruct( State* state, Data* dat ) {
    stateFreeRaw( state, dat->mems, sizeof(TVal)*dat->info->nMems );
    if( dat->info->destr )
        dat->info->destr( (ten_State*)state, dat->data );
}
