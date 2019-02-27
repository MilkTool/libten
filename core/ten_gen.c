#include "ten_gen.h"
#include "ten_fun.h"
#include "ten_sym.h"
#include "ten_ptr.h"
#include "ten_idx.h"
#include "ten_fun.h"
#include "ten_env.h"
#include "ten_state.h"
#include "ten_stab.h"
#include "ten_macros.h"
#include "ten_assert.h"

// Dynamic buffers.
#define BUF_TYPE GenConst
#define BUF_NAME ConstBuf
    #include "inc/buf.inc"
#undef BUF_NAME
#undef BUF_TYPE

#define BUF_TYPE instr
#define BUF_NAME CodeBuf
    #include "inc/buf.inc"
#undef BUF_NAME
#undef BUF_TYPE

#define BUF_TYPE LineInfo
#define BUF_NAME LineBuf
    #include "inc/buf.inc"
#undef BUF_NAME
#undef BUF_TYPE


struct Gen {
    Finalizer finl;
    Scanner   scan;
    
    bool global;
    Gen* parent;
    
    STab*  glbs;
    STab*  upvs;
    STab*  lcls;
    STab*  lbls;
    
    ConstBuf consts;
    CodeBuf  code;
    
    uint nParams;
    bool vParams;
    
    uint curTemps;
    uint maxTemps;
    
    bool        debug;
    SymT        func;
    SymT        file;
    LineInfo*   line;
    LineBuf     lines;
    
    void* misc1;
    void* misc2;
    
    void* obj1;
    void* obj2;
};

void
genInit( State* state ) {
    state->genState = NULL;
}

static void
genFinl( State* state, Finalizer* finl ) {
    Gen* gen = structFromFinl( Gen, finl );
    
    stateRemoveScanner( state, &gen->scan );
    
    finlConstBuf( state, &gen->consts );
    finlCodeBuf( state, &gen->code );
    if( gen->debug )
        finlLineBuf( state, &gen->lines );
    stateFreeRaw( state, gen, sizeof(Gen) );
    
}

static void
genScan( State* state, Scanner* scan ) {
    Gen* gen = structFromScan( Gen, scan );
    
    if( gen->debug && state->gcFull ) {
        symMark( state, gen->func );
        symMark( state, gen->file );
    }
    
    if( gen->obj1 )
        stateMark( state, gen->obj1 );
    if( gen->obj2 )
        stateMark( state, gen->obj2 );
    
    for( uint i = 0 ; i < gen->consts.top ; i++ )
        tvMark( gen->consts.buf[i].val );
}

static void
freeVar( State* state, void* udat, void* edat ) {
    stateFreeRaw( state, edat, sizeof(GenVar) );
}

static void
freeLbl( State* state, void* udat, void* edat ) {
    stateFreeRaw( state, edat, sizeof(GenLbl) );
}

Gen*
genMake( State* state, Gen* parent, bool global, bool debug ) {
    Part genP;
    Gen* gen = stateAllocRaw( state, &genP, sizeof(Gen) );
    
    gen->global = global;
    gen->parent = parent;
    gen->glbs   = stabMake( state, true, gen, freeVar );
    gen->upvs   = stabMake( state, true, gen, freeVar );
    gen->lcls   = stabMake( state, true, gen, freeVar );
    gen->lbls   = stabMake( state, false, gen, freeLbl );
    initConstBuf( state, &gen->consts );
    initCodeBuf( state, &gen->code );
    
    gen->curTemps = 0;
    gen->maxTemps = 0;
    
    gen->nParams = 0;
    gen->vParams = false;
    
    gen->debug = debug;
    if( debug ) {
        gen->func  = symGet( state, "<anon>", 6 );
        gen->file  = symGet( state, "<input>", 7 );
        initLineBuf( state, &gen->lines );
        genSetLine( state, gen, 0 );
    }
    
    gen->misc1 = NULL;
    gen->misc2 = NULL;
    gen->obj1  = NULL;
    gen->obj2  = NULL;
    
    gen->scan.cb = genScan;
    stateInstallScanner( state, &gen->scan );
    gen->finl.cb = genFinl;
    stateInstallFinalizer( state, &gen->finl );
    return gen;
}

static void
setLabel( State* state, void* udat, void* edat ) {
    Gen*    gen = udat;
    GenLbl* lbl = edat;
    
    instr*  code   = gen->misc1;
    instr** labels = gen->misc2;
    labels[lbl->which] = code + lbl->where;
}

static void
setUpvalRef( State* state, void* udat, void* edat ) {
    Gen*    gen = udat;
    GenVar* upv = edat;
    
    instr*  refs = gen->misc1;
    
    Gen*    pgen = gen->parent;
    GenVar* pvar = (GenVar*)genGetVar( state, pgen, upv->name );
    if( pvar->type == VAR_LOCAL ) {
        pvar->type = VAR_CLOSED;
        refs[upv->which] = inMake( OPC_REF_LOCAL, pvar->which );
    }
    else
    if( pvar->type == VAR_CLOSED ) {
        refs[upv->which] = inMake( OPC_REF_CLOSED, pvar->which );
    }
    else
    if( pvar->type == VAR_UPVAL ) {
        refs[upv->which] = inMake( OPC_REF_UPVAL, pvar->which );
    }
    else
    if( pvar->type == VAR_GLOBAL ) {
        refs[upv->which] = inMake( OPC_REF_GLOBAL, pvar->which );
    }
}

Function*
genFinish( State* state, Gen* gen, bool constr ) {
    Index* vargIdx = NULL;
    if( gen->vParams ) {
        tenAssert( gen->nParams > 0 );
        vargIdx = idxNew( state );
    }
    gen->obj1 = vargIdx;
    
    Function* fun  = funNewVir( state, gen->nParams, vargIdx );
    VirFun*   vfun = &fun->u.vir;
    gen->obj2 = fun;
    
    uint   len  = gen->code.top;
    instr* code = packCodeBuf( state, &gen->code );
    vfun->len  = len;
    vfun->code = code;
    
    Part constsP;
    uint nConsts = gen->consts.top;
    TVal* consts = stateAllocRaw( state, &constsP, sizeof(TVal)*nConsts );
    for( uint i = 0 ; i < nConsts ; i++ )
        consts[i] = gen->consts.buf[i].val;
    finlConstBuf( state, &gen->consts );
    vfun->nConsts = nConsts;
    vfun->consts  = consts;
    
    Part labelsP;
    uint nLabels = stabNumSlots( state, gen->lbls );
    instr** labels = stateAllocRaw( state, &labelsP, sizeof(instr*)*nLabels );
    gen->misc1 = code;
    gen->misc2 = labels;
    stabForEach( state, gen->lbls, setLabel );
    vfun->nLabels = nLabels;
    vfun->labels  = labels;
    
    vfun->nUpvals = stabNumSlots( state, gen->upvs );
    vfun->nLocals = stabNumSlots( state, gen->lcls );
    vfun->nTemps  = gen->maxTemps;
    
    // If `constr == true` then we add code to construct
    // a closure from the function into the parent.
    if( constr ) {
        Gen* pgen = gen->parent;
        tenAssert( pgen );
        
        // The function goes into the parent's constant
        // pool and is stacked before the upvalue bindings.
        GenConst const* fc = genAddConst( state, pgen, tvObj( fun ) );
        genPutInstr( state, gen, inMake( OPC_GET_CONST, fc->which ) );
        
        // Following the function goes a list of references,
        // one for each upvalue of the child function.
        instr refs[vfun->nUpvals];
        gen->misc1 = refs;
        stabForEach( state, gen->upvs, setUpvalRef );
    }
    
    stabFree( state, gen->glbs );
    if( gen->debug ) {
        Part dbgP;
        DbgInfo* dbg = stateAllocRaw( state, &dbgP, sizeof(DbgInfo) );
        dbg->lcls   = gen->lcls;
        dbg->upvs   = gen->upvs;
        dbg->lbls   = gen->lbls;
        dbg->func   = gen->func;
        dbg->file   = gen->file;
        dbg->nLines = gen->lines.top;
        dbg->lines   = packLineBuf( state, &gen->lines );
        vfun->dbg = dbg;
        stateCommitRaw( state, &dbgP );
    }
    else {
        stabFree( state, gen->lcls );
        stabFree( state, gen->upvs );
        stabFree( state, gen->lbls );
    }
    
    stateRemoveScanner( state, &gen->scan );
    stateRemoveFinalizer( state, &gen->finl );
    return fun;
}

void
genSetFile( State* state, Gen* gen, SymT file ) {
    if( !gen->debug )
        return;
    
    gen->file = file;
}

void
genSetFunc( State* state, Gen* gen, SymT func ) {
    if( !gen->debug )
        return;
    
    gen->func = func;
}

void
genSetLine( State* state, Gen* gen, uint linenum ) {
    if( !gen->debug )
        return;
    
    while( linenum <= gen->lines.top ) {
        LineInfo* line = putLineBuf( state, &gen->lines );
        line->line  = gen->lines.top - 1;
        line->start = gen->code.top;
        line->end   = line->start;
        line->bcb   = NULL;
    }
    gen->line = &gen->lines.buf[linenum];
}

GenConst const*
genAddConst( State* state, Gen* gen, TVal val ) {
    GenConst* c = putConstBuf( state, &gen->consts );
    c->which = gen->consts.top - 1;
    c->val   = val;
    return c;
}

static GenVar*
addGlobal( State* state, Gen* gen, SymT name ) {
    GenVar* var = stabGetDat( state, gen->glbs, name, gen->code.top );
    if( var )
        return var;
    
    Part varP;
    var = stateAllocRaw( state, &varP, sizeof(GenVar) );
    stabAdd( state, gen->glbs, name, var );
    var->which = envAddGlobal( state, name );
    var->name  = name;
    var->type  = VAR_GLOBAL;
    stateCommitRaw( state, &varP );
    return var;
}

static GenVar*
addLocal( State* state, Gen* gen, SymT name ) {
    if( gen->global )
        return addGlobal( state, gen, name );
    
    GenVar* var = stabGetDat( state, gen->lcls, name, gen->code.top );
    if( var ) {
        if( var->type == VAR_CLOSED )
            var->type = VAR_LOCAL;
        return var;
    }
    
    Part varP;
    var = stateAllocRaw( state, &varP, sizeof(GenVar) );
    var->which = stabAdd( state, gen->lcls, name, var );
    var->name  = name;
    var->type  = VAR_LOCAL;
    stateCommitRaw( state, &varP );
    return var;
}

static GenVar*
addUpval( State* state, Gen* gen, SymT name ) {
    if( gen->global )
        return addGlobal( state, gen, name );
    
    GenVar* var = stabGetDat( state, gen->upvs, name, gen->code.top );
    if( var )
        return var;
    
    Part varP;
    var = stateAllocRaw( state, &varP, sizeof(GenVar) );
    var->which = stabAdd( state, gen->upvs, name, var );
    var->name  = name;
    var->type  = VAR_UPVAL;
    stateCommitRaw( state, &varP );
    
    return var;
}


GenVar const*
genAddParam( State* state, Gen* gen, SymT name, bool vParam ) {
    tenAssert( !gen->vParams );
    tenAssert( !gen->global );
    if( vParam )
        gen->vParams = true;
    else
        gen->nParams++;
    
    return addLocal( state, gen, name );
}

GenVar const*
genAddVar( State* state, Gen* gen, SymT name ) {
    return addLocal( state, gen, name );
}

GenVar const*
genGetVar( State* state, Gen* gen, SymT name ) {
    GenVar* var;
    
    var = stabGetDat( state, gen->lcls, name, gen->code.top );
    if( var )
        return var;
    
    if( gen->global )
        var = stabGetDat( state, gen->glbs, name, gen->code.top );
    else
        var = stabGetDat( state, gen->upvs, name, gen->code.top );
    if( var )
        return var;
    
    return addUpval( state, gen, name  );
}

GenLbl const*
genAddLbl( State* state, Gen* gen, SymT name ) {
    Part lblP;
    GenLbl* lbl = stateAllocRaw( state, &lblP, sizeof(GenLbl) );
    uint    loc = stabAdd( state, gen->lbls, name, lbl );
    lbl->which = loc;
    lbl->where = gen->code.top;
    lbl->name  = name;
    return lbl;
}

GenLbl const*
genGetLbl( State* state, Gen* gen, SymT name ) {
    return stabGetDat( state, gen->lbls, name, gen->code.top );
}

void
genMovLbl( State* state, Gen* gen, GenLbl const* lbl, uint where ) {
    tenAssert( where <= gen->code.top );
    ((GenLbl*)lbl)->where = where;
}

void
genOpenScope( State* state, Gen* gen ) {
    stabOpenScope( state, gen->lcls, gen->code.top );
    stabOpenScope( state, gen->lbls, gen->code.top );
}

void
genCloseScope( State* state, Gen* gen ) {
    stabCloseScope( state, gen->lcls, gen->code.top );
    stabCloseScope( state, gen->lbls, gen->code.top );
}

void
genPutInstr( State* state, Gen* gen, instr in ) {
    *putCodeBuf( state, &gen->code ) = in;
}

uint
genGetPlace( State* state, Gen* gen ) {
    return gen->code.top;
}
