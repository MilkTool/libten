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
        symMark( state, it->type );
        it = it->next;
    }
}

static void
datFinl( State* state, Finalizer* finl ) {
    DatState* dat = structFromFinl( DatState, finl );
    
    stateRemoveScanner( state, &dat->scan );
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

void
datInitInfo( State* state, ten_DatConfig* config, DatInfo* info ) {
    char const* type;
    if( config->tag )
        type = fmtA( state, false, "Dat:%s", config->tag );
    else
        type = "Dat";
    
    info->type  = symGet( state, type, strlen( type ) );
    info->size  = config->size;
    info->nMems = config->mems;
    info->destr = config->destr;
    info->magic = DAT_MAGIC;
    info->next  = state->datState->infos;
    state->datState->infos = info;
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
    for( uint i = 0 ; i < dat->info->nMems ; i++ )
        tvMark( dat->mems[i] );
}

void
datDestruct( State* state, Data* dat ) {
    stateFreeRaw( state, dat->mems, sizeof(TVal)*dat->info->nMems );
    if( dat->info->destr )
        dat->info->destr( (ten_State*)state, dat->data );
}
