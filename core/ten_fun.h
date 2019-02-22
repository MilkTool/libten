#ifndef ten_fun_h
#define ten_fun_h
#include "ten_types.h"
#include "ten_api.h"

typedef struct {
    uint     line;
    uint     start;
    uint     end;
    Closure* bcb;
} LineInfo;

typedef struct {
    NTab* upvals;
    STab* locals;
    
    SymT        func;
    SymT        file;
    uint        nLines;
    LineInfo*   lines;
} DbgInfo;

typedef struct {
    Record* docs;
    uint    next;
} DocInfo;

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
    int      nParams;
    DocInfo* doc;
    union {
        VirFun vir;
        NatFun nat;
    } u;
};

#define funSize( STATE, FUN ) (sizeof(Function))
#define funTrav( STATE, FUN ) (funTraverse( STATE, FUN ))
#define funDest( STATE, FUN ) (funDestruct( STATE, FUN ))

Function*
funNewVir( State* state, int nParams, DocInfo* doc );

Function*
funNewNat( State* state, int nParams, DocInfo* doc, ten_FunCb cb );

void
funTraverse( State* state, Function* fun );

void
funDestruct( State* state, Function* fun );

#endif
