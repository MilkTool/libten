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

struct GenState {
    Finalizer finl;
    Scanner   scan;
    SymT      this;
};


struct Gen {
    Finalizer finl;
    Scanner   scan;
    
    bool global;
    Gen* parent;
    
    STab*  glbs;
    STab*  upvs;
    STab*  lcls;
    STab*  lbls;
    STab*  cons;
    
    CodeBuf  code;
    
    uint nParams;
    bool vParams;
    
    uint curTemps;
    uint maxTemps;
    
    int level;
    
    bool        debug;
    SymT        func;
    SymT        file;
    uint        start;
    LineInfo*   line;
    LineBuf     lines;
    
    void* misc1;
    void* misc2;
    
    void* obj1;
    void* obj2;
};

void
genStateScan( State* state, Scanner* scan ) {
    GenState* genState = structFromScan( GenState, scan );
    if( state->gcFull )
        symMark( state, genState->this );
}

void
genStateFinl( State* state, Finalizer* finl ) {
    GenState* genState = structFromFinl( GenState, finl );
    stateRemoveScanner( state, &genState->scan );
    stateFreeRaw( state, genState, sizeof(GenState) );
}

void
genInit( State* state ) {
    Part genP;
    GenState* genState = stateAllocRaw( state, &genP, sizeof(GenState) );
    genState->this = symGet( state, "this", 4 );
    genState->finl.cb = genStateFinl;
    genState->scan.cb = genStateScan;
    stateInstallFinalizer( state, &genState->finl );
    stateInstallScanner( state, &genState->scan );
    stateCommitRaw( state, &genP );
    
    state->genState = genState;
}

#ifdef ten_TEST
void
genTest( State* state ) {
    Gen* gen;
    
    gen = genMake( state, NULL, NULL, true, true );
    genSetFile( state, gen, symGet( state, "f1", 2 ) );
    genSetFunc( state, gen, symGet( state, "t1", 2 ) );
    genSetLine( state, gen, 2 );
    
    tenAssert( gen->file  == symGet( state, "f1", 2 ) );
    tenAssert( gen->func  == symGet( state, "t1", 2 ) );
    tenAssert( gen->start == 1 );
    tenAssert( gen->line->line == 2 );
    
    GenVar* v1 = genAddVar( state, gen, symGet( state, "v1", 2 ) );
    tenAssert( v1->type == VAR_GLOBAL );
    
    GenConst* c1 = genAddConst( state, gen, tvInt( 1 ) );
    GenConst* c2 = genAddConst( state, gen, tvInt( 2 ) );
    GenConst* c3 = genAddConst( state, gen, tvInt( 3 ) );
    
    genPutInstr( state, gen, inMake( OPC_GET_CONST, c1->which ) );
    genPutInstr( state, gen, inMake( OPC_GET_CONST, c2->which ) );
    genPutInstr( state, gen, inMake( OPC_ADD, 0 ) );
    tenAssert( gen->maxTemps == 2 );
    
    {
        genOpenScope( state, gen );
        GenVar* v1 = genAddVar( state, gen, symGet( state, "v1", 2 ) );
        tenAssert( v1->type == VAR_LOCAL );
        tenAssert( v1 == genGetVar( state, gen, symGet( state, "v1", 2 ) ) );
        
        GenVar* v2 = genGetVar( state, gen, symGet( state, "v2", 2 ) );
        tenAssert( v2 == genGetVar( state, gen, symGet( state, "v2", 2 ) ) );
        tenAssert( v2->type == VAR_GLOBAL );
        
        genSetLine( state, gen, 5 );
        {
            SymT fun = symGet( state, "t2", 2 );
            Gen* sub = genMake( state, gen, &fun , false, false );
            tenAssert( sub->file == symGet( state, "f1", 2 ) );
            tenAssert( sub->func == symGet( state, "t2", 2 ) );
            tenAssert( sub->start == 5 );
            tenAssert( sub->debug == true );
            tenAssert( sub->global == false );
            tenAssert( sub->parent == gen );
            
            GenVar* v1 = genGetVar( state, sub, symGet( state, "v1", 2 ) );
            tenAssert( v1->type == VAR_UPVAL );
            
            genFinish( state, sub, true );
        }
        tenAssert( v1->type == VAR_CLOSED );
        
        genCloseScope( state, gen );
    }
    
    
    genFinish( state, gen, false );
}
#endif

static void
genFinl( State* state, Finalizer* finl ) {
    Gen* gen = structFromFinl( Gen, finl );
    
    stateRemoveScanner( state, &gen->scan );
    
    finlCodeBuf( state, &gen->code );
    if( gen->debug )
        finlLineBuf( state, &gen->lines );
    stateFreeRaw( state, gen, sizeof(Gen) );
    
}


static void
markConst( State* state, void* udat, void* edat ) {
    Gen*      gen = udat;
    GenConst* con = edat;
    tvMark( con->val );
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
    
    stabForEach( state, gen->cons, markConst );
}

static void
freeVar( State* state, void* udat, void* edat ) {
    stateFreeRaw( state, edat, sizeof(GenVar) );
}

static void
freeLbl( State* state, void* udat, void* edat ) {
    stateFreeRaw( state, edat, sizeof(GenLbl) );
}

static void
freeCons( State* state, void* udat, void* edat ) {
    stateFreeRaw( state, edat, sizeof(GenConst) );
}

static GenVar*
addLocal( State* state, Gen* gen, SymT ident );

Gen*
genMake( State* state, Gen* parent, SymT* func, bool global, bool debug ) {
    Part genP;
    Gen* gen = stateAllocRaw( state, &genP, sizeof(Gen) );
    
    gen->global = global;
    gen->parent = parent;
    gen->glbs   = stabMake( state, true, gen, freeVar );
    gen->upvs   = stabMake( state, true, gen, freeVar );
    gen->lcls   = stabMake( state, true, gen, freeVar );
    gen->lbls   = stabMake( state, false, gen, freeLbl );
    gen->cons   = stabMake( state, false, gen, freeCons );
    initCodeBuf( state, &gen->code );
    
    gen->curTemps = 0;
    gen->maxTemps = 0;
    
    gen->level = 0;
    
    gen->nParams = 0;
    gen->vParams = false;
    
    gen->debug = parent? parent->debug : debug;
    if( gen->debug ) {
        gen->func  = func ? *func : symGet( state, "<anon>", 6 );
        gen->file  = parent ? parent->file : symGet( state, "<input>", 7 );
        gen->start = parent ? parent->line->line : 1;
        initLineBuf( state, &gen->lines );
        genSetLine( state, gen, gen->start );
    }
    
    gen->misc1 = NULL;
    gen->misc2 = NULL;
    gen->obj1  = NULL;
    gen->obj2  = NULL;
    
    gen->scan.cb = genScan;
    stateInstallScanner( state, &gen->scan );
    gen->finl.cb = genFinl;
    stateInstallFinalizer( state, &gen->finl );
    
    GenVar* this = addLocal( state, gen, state->genState->this );
    tenAssert( this->which == 0 );
    
    stateCommitRaw( state, &genP );
    
    return gen;
}

void
genFree( State* state, Gen* gen ) {
    stateRemoveFinalizer( state, &gen->finl );
    stabFree( state, gen->glbs );
    stabFree( state, gen->upvs );
    stabFree( state, gen->lcls );
    stabFree( state, gen->lbls );
    stabFree( state, gen->cons );
    genFinl( state, &gen->finl );
}

static void
setConst( State* state, void* udat, void* edat ) {
    Gen*      gen = udat;
    GenConst* con = edat;
    
    TVal* consts = gen->misc1;
    consts[con->which] = con->val;
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
genUpvalRef( State* state, void* udat, void* edat ) {
    Gen*    gen = udat;
    GenVar* upv = edat;
    
    Gen*    pgen = gen->parent;
    GenVar* pvar = (GenVar*)genGetVar( state, pgen, upv->name );
    if( pvar->type == VAR_LOCAL ) {
        pvar->type = VAR_CLOSED;
        genPutInstr( state, pgen, inMake( OPC_REF_LOCAL, pvar->which ) );
    }
    else
    if( pvar->type == VAR_CLOSED ) {
        genPutInstr( state, pgen, inMake( OPC_REF_CLOSED, pvar->which ) );
    }
    else
    if( pvar->type == VAR_UPVAL ) {
        genPutInstr( state, pgen, inMake( OPC_REF_UPVAL, pvar->which ) );
    }
    else
    if( pvar->type == VAR_GLOBAL ) {
        genPutInstr( state, pgen, inMake( OPC_REF_GLOBAL, pvar->which ) );
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
    fun->nParams = gen->nParams;
    
    VirFun*   vfun = &fun->u.vir;
    gen->obj2 = fun;
    
    uint   len  = gen->code.top;
    instr* code = packCodeBuf( state, &gen->code );
    vfun->len  = len;
    vfun->code = code;
    
    Part constsP;
    uint nConsts = stabNumSlots( state, gen->cons );
    TVal* consts = stateAllocRaw( state, &constsP, sizeof(TVal)*nConsts );
    gen->misc1 = consts;
    stabForEach( state, gen->cons, setConst );
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
    vfun->nLocals = stabNumSlots( state, gen->lcls ) - gen->nParams - 1;
    vfun->nTemps  = gen->maxTemps;
    
    // If `constr == true` then we add code to construct
    // a closure from the function into the parent.
    if( constr ) {
        Gen* pgen = gen->parent;
        tenAssert( pgen );
        
        // The function goes into the parent's constant
        // pool and is stacked before the upvalue bindings.
        GenConst* fc = genAddConst( state, pgen, tvObj( fun ) );
        genPutInstr( state, pgen, inMake( OPC_GET_CONST, fc->which ) );
        
        // Following the function goes a list of references,
        // one for each upvalue of the child function.
        stabForEach( state, gen->upvs, genUpvalRef );
        
        // And the closure constructor instruction.
        genPutInstr( state, pgen, inMake( OPC_MAKE_CLS, vfun->nUpvals ) );
    }
    
    stabFree( state, gen->glbs );
    stabFree( state, gen->cons );
    if( gen->debug ) {
        Part dbgP;
        DbgInfo* dbg = stateAllocRaw( state, &dbgP, sizeof(DbgInfo) );
        dbg->lcls   = gen->lcls;
        dbg->upvs   = gen->upvs;
        dbg->lbls   = gen->lbls;
        dbg->func   = gen->func;
        dbg->file   = gen->file;
        dbg->start  = gen->start;
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
    
    stateCommitRaw( state, &constsP );
    stateCommitRaw( state, &labelsP );
    
    stateRemoveScanner( state, &gen->scan );
    stateRemoveFinalizer( state, &gen->finl );
    stateFreeRaw( state, gen, sizeof(Gen) );
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
    
    tenAssert( linenum >= gen->start );
    while( gen->start + gen->lines.top <= linenum ) {
        LineInfo* line = putLineBuf( state, &gen->lines );
        line->line  = gen->start + gen->lines.top - 1;
        line->start = gen->code.top;
        line->end   = gen->code.top;
    }
    gen->line = gen->lines.buf + gen->lines.top - 1;
}

GenConst*
genAddConst( State* state, Gen* gen, TVal val ) {
    Part cP;
    GenConst* c = stateAllocRaw( state, &cP, sizeof(GenConst) );
    
    char buf[sizeof(ullong) + sizeof(uchar)];
    *(ullong*)&buf[0]             = tvGetVal( val );
    *(uchar*)&buf[sizeof(ullong)] = tvGetTag( val );
    
    SymT s = symGet( state, buf, sizeof(buf) );
    
    c->which = stabAdd( state, gen->cons, s, c );
    c->val   = val;
    stateCommitRaw( state, &cP );
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


GenVar*
genAddParam( State* state, Gen* gen, SymT name, bool vParam ) {
    tenAssert( !gen->vParams );
    tenAssert( !gen->global );
    if( vParam )
        gen->vParams = true;
    else
        gen->nParams++;
    
    return addLocal( state, gen, name );
}

GenVar*
genAddVar( State* state, Gen* gen, SymT name ) {
    if( gen->global && gen->level == 0 )
        return addGlobal( state, gen, name );
    else
        return addLocal( state, gen, name );
}

GenVar*
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
    
    if( gen->global )
        return addGlobal( state, gen, name );
    else
        return addUpval( state, gen, name );
}

GenLbl*
genAddLbl( State* state, Gen* gen, SymT name ) {
    Part lblP;
    GenLbl* lbl = stateAllocRaw( state, &lblP, sizeof(GenLbl) );
    uint    loc = stabAdd( state, gen->lbls, name, lbl );
    lbl->which = loc;
    lbl->where = gen->code.top;
    lbl->name  = name;
    stateCommitRaw( state, &lblP );
    return lbl;
}

GenLbl*
genGetLbl( State* state, Gen* gen, SymT name ) {
    return stabGetDat( state, gen->lbls, name, gen->code.top );
}

void
genMovLbl( State* state, Gen* gen, GenLbl* lbl, uint where ) {
    tenAssert( where <= gen->code.top );
    lbl->where = where;
}

void
genOpenScope( State* state, Gen* gen ) {
    gen->level++;
    stabOpenScope( state, gen->lcls, gen->code.top );
    stabOpenScope( state, gen->lbls, gen->code.top );
}

void
genCloseScope( State* state, Gen* gen ) {
    tenAssert( gen->level > 0 );
    gen->level--;
    stabCloseScope( state, gen->lcls, gen->code.top );
    stabCloseScope( state, gen->lbls, gen->code.top );
}

void
genOpenLblScope( State* state, Gen* gen ) {
    stabOpenScope( state, gen->lbls, gen->code.top );
}

void
genCloseLblScope( State* state, Gen* gen ) {
    stabCloseScope( state, gen->lbls, gen->code.top );
}


// An array of the stack effects for each instruction.
#define SE( MUL, OFF ) MUL, OFF
#define OP( NAME, SE ) { SE },
typedef struct {
    long mul;
    long off;
} StackEffect;

static StackEffect effects[] = {
    #include "inc/ops.inc"
};

void
genPutInstr( State* state, Gen* gen, instr in ) {
    *putCodeBuf( state, &gen->code ) = in;
    if( gen->debug )
        gen->line->end = gen->code.top;
    
    OpCode opc = inGetOpc( in );
    ushort opr = inGetOpr( in );
    StackEffect* se = &effects[opc];
    gen->curTemps += se->mul*opr + se->off;
    if( gen->curTemps > gen->maxTemps )
        gen->maxTemps = gen->curTemps;
}

uint
genGetPlace( State* state, Gen* gen ) {
    return gen->code.top;
}
