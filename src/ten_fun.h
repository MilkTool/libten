/***********************************************************************
This component of the VM implements Ten's Function data type, which
represents the immutable parts of Ten's callable Closure type.
Basically this consists of constants, code, and debug info if debugging
is enabled.

Each instance of the Function type can either be a virtual or native
function, tagged respectively as `ten_VIR` or `ten_NAT`.  Virtual
functions are backed by Ten's bytecode and native functions are backed
by a native function callback provided by either the host application
or by the prelude implemented in `ten_lib.*`.
***********************************************************************/

#ifndef ten_fun_h
#define ten_fun_h
#include "ten.h"
#include "ten_types.h"

typedef struct {
    uint     line;
    char*    text;
    uint     start;
    uint     end;
} LineInfo;

typedef struct {
    SymT        func;
    SymT        file;
    uint        start;
    uint        nLines;
    LineInfo*   lines;
} DbgInfo;

typedef struct {
    uint nConsts;
    uint nLabels;
    uint nUpvals;
    uint nLocals;
    uint nTemps;
    
    TVal*   consts;
    instr** labels;
    
    uint   len;
    instr* code;
    
    DbgInfo* dbg;
} VirFun;

typedef struct {
    ten_FunCb cb;
    SymT      name;
    SymT*     params;
} NatFun;

typedef enum {
    FUN_VIR,
    FUN_NAT
} FunType;

struct Function {
    FunType  type;
    uint     nParams;
    Index*   vargIdx;
    union {
        VirFun vir;
        NatFun nat;
    } u;
};

#define funSize( STATE, FUN ) (sizeof(Function))
#define funTrav( STATE, FUN ) (funTraverse( STATE, FUN ))
#define funDest( STATE, FUN ) (funDestruct( STATE, FUN ))

void
funInit( State* state );

#ifdef ten_DEBUG
    void
    funDump( State* state, Function* fun );
#endif

Function*
funNewVir( State* state, uint nParams, Index* vargIdx );

Function*
funNewNat( State* state, uint nParams, Index* vargIdx, ten_FunCb cb );

void
funTraverse( State* state, Function* fun );

void
funDestruct( State* state, Function* fun );

#endif
