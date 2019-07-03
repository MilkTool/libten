#include "ten_gen.h"
#include "ten_fun.h"
#include "ten_sym.h"
#include "ten_ptr.h"
#include "ten_idx.h"
#include "ten_fun.h"
#include "ten_upv.h"
#include "ten_str.h"
#include "ten_env.h"
#include "ten_state.h"
#include "ten_stab.h"
#include "ten_macros.h"
#include "ten_assert.h"

#include <string.h>

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
    
    uint      nUpvals;
    Upvalue** upvals;
    
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

static void
genFinl( State* state, Finalizer* finl ) {
    Gen* gen = structFromFinl( Gen, finl );
    
    stateRemoveScanner( state, &gen->scan );
    
    finlCodeBuf( state, &gen->code );
    
    for( uint i = 0 ; i < gen->lines.top ; i++ ) {
        char* text = gen->lines.buf[i].text;
        if( text )
            stateFreeRaw( state, text, strlen( text ) + 1 );
    }
    if( gen->debug )
        finlLineBuf( state, &gen->lines );
    if( gen->upvals )
        stateFreeRaw( state, gen->upvals, sizeof(Upvalue*)*gen->nUpvals );
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
    
    if( gen->upvals ) {
        for( uint i = 0 ; i < gen->nUpvals ; i++ ) {
            if( gen->upvals[i] )
                stateMark( state, gen->upvals[i] );
        }
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
    
    gen->nUpvals = 0;
    gen->upvals  = NULL;
    
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
    
    instr* urefs = gen->misc1;
    
    Gen*    pgen = gen->parent;
    GenVar* pvar = (GenVar*)genGetVar( state, pgen, upv->name );
    if( pvar->type == VAR_LOCAL ) {
        pvar->type = VAR_CLOSED;
        urefs[upv->which] = inMake( OPC_REF_LOCAL, pvar->which );
    }
    else
    if( pvar->type == VAR_CLOSED ) {
        urefs[upv->which] = inMake( OPC_REF_CLOSED, pvar->which );
    }
    else
    if( pvar->type == VAR_UPVAL ) {
        urefs[upv->which] = inMake( OPC_REF_UPVAL, pvar->which );
    }
    else
    if( pvar->type == VAR_GLOBAL ) {
        urefs[upv->which] = inMake( OPC_REF_GLOBAL, pvar->which );
    }
}

Function*
genFinish( State* state, Gen* gen, bool constr ) {
    Index* vargIdx = NULL;
    if( gen->vParams )
        vargIdx = idxNew( state );
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
        instr urefs[vfun->nUpvals];
        gen->misc1 = urefs;
        stabForEach( state, gen->upvs, genUpvalRef );
        for( uint i = 0 ; i < vfun->nUpvals ; i++ )
            genPutInstr( state, pgen, urefs[i] );
        
        // And the closure constructor instruction.
        genPutInstr( state, pgen, inMake( OPC_MAKE_CLS, vfun->nUpvals ) );
    }
    if( gen->debug ) {
        Part dbgP;
        DbgInfo* dbg = stateAllocRaw( state, &dbgP, sizeof(DbgInfo) );
        dbg->func   = gen->func;
        dbg->file   = gen->file;
        dbg->start  = gen->start;
        dbg->nLines = gen->lines.top;
        dbg->lines  = packLineBuf( state, &gen->lines );
        vfun->dbg = dbg;
        stateCommitRaw( state, &dbgP );
    }
    
    stateCommitRaw( state, &constsP );
    stateCommitRaw( state, &labelsP );
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

static void
ensureLine( State* state, Gen* gen, uint linenum ) {
    tenAssert( linenum >= gen->start );
    while( gen->start + gen->lines.top <= linenum ) {
        LineInfo* line = putLineBuf( state, &gen->lines );
        line->line  = gen->start + gen->lines.top - 1;
        line->text  = NULL;
        line->start = gen->code.top;
        line->end   = gen->code.top;
    }
}

void
genSetLine( State* state, Gen* gen, uint linenum ) {
    if( !gen->debug )
        return;
    
    ensureLine( state, gen, linenum );
    gen->line = gen->lines.buf + gen->lines.top - 1;
}


void
genSetLineText( State* state, Gen* gen, uint ln, char const* txt ) {
    if( !gen->debug )
        return;
    ensureLine( state, gen, ln );
    
    size_t len = strlen( txt );
    
    Part  cpyP;
    char* cpy = stateAllocRaw( state, &cpyP, len + 1 );
    memcpy( cpy, txt, len + 1 );
    stateCommitRaw( state, &cpyP );
    
    LineInfo* line = &gen->lines.buf[ln-gen->start];
    line->text = cpy;
}

GenConst*
genAddConst( State* state, Gen* gen, TVal val ) {
    Part cP;
    GenConst* c = stateAllocRaw( state, &cP, sizeof(GenConst) );
    
    SymT s;
    String* str = NULL;
    if( tvIsObjType( val, OBJ_STR ) && (str = tvGetObj( val ))->len < 64 ) {
        
        char buf[ str->len + 1 ];
        memcpy( buf, str->buf, str->len );
        buf[str->len] = OBJ_STR;
        
        s = symGet( state, buf, sizeof(buf) );
    }
    else {
        char    buf[sizeof(ullong) + 1];
        ullong* v = (ullong*)buf;
        uchar*  t = (uchar*)(buf + sizeof(ullong));
    
        *v = tvGetVal( val );
        *t = tvGetTag( val );
        
        s = symGet( state, buf, sizeof(buf) );
    }
    
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
    Part varP;
    GenVar* var = stateAllocRaw( state, &varP, sizeof(GenVar) );
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
genAddUpv( State* state, Gen* gen, SymT name ) {
    return addUpval( state, gen, name );
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

static void
setUpval( State* state, void* udat, void* edat ) {
    Gen*    gen = udat;
    GenVar* var = edat;
    
    Upvalue** upvals = gen->upvals;
    TVal*     global = envGetGlobalByName( state, var->name );
    if( global ) {
        if( tvIsObjType( *global, OBJ_UPV ) )
            upvals[var->which] = tvGetObj( *global );
        else
            upvals[var->which] = upvNew( state, *global );
    }
}

Upvalue**
genGlobalUpvals( State* state, Gen* gen ) {
    uint n = stabNumSlots( state, gen->upvs );
    Part upvalsP;
    Upvalue** upvals = stateAllocRaw( state, &upvalsP, sizeof(Upvalue*)*n );
    for( uint i = 0 ; i < n ; i++ )
        upvals[i] = NULL;
    gen->upvals  = upvals;
    gen->nUpvals = n;
    stabForEach( state, gen->upvs, setUpval );
    gen->upvals  = NULL;
    gen->nUpvals = 0;
    stateCommitRaw( state, &upvalsP );
    
    return upvals;
}
