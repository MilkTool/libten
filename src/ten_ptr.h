#ifndef ten_ptr_h
#define ten_ptr_h
#include "ten.h"
#include "ten_types.h"
#include <stddef.h>
#include <stdbool.h>

typedef struct PtrInfo PtrInfo;

struct PtrInfo {
    PtrInfo* next;
    
    ten_Var typeVar;
    Tup     typeTup;
    TVal*   typePtr;
    TVal    typeVal;
    void  (*destr)( ten_State* core, void* addr );
};

void
ptrInit( State* state );

PtrInfo*
ptrAddInfo( State* state, ten_PtrConfig* config );

PtrT
ptrGet( State* state, PtrInfo* info, void* addr );

void*
ptrAddr( State* state, PtrT ptr );

PtrInfo*
ptrInfo( State* state, PtrT ptr );


void
ptrStartCycle( State* state );

void
ptrMark( State* state, PtrT ptr );

void
ptrFinishCycle( State* state );

#endif
