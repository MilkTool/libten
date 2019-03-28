#ifndef ten_ptr_h
#define ten_ptr_h
#include "ten_types.h"
#include "ten_api.h"
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    #define PTR_MAGIC ((ulong)'P' << 16 | (ulong)'T' << 8 | 'R') 
    uint  magic;
    SymT  type;
    void  (*destr)( ten_State* core, void* addr );
} PtrInfo;

void
ptrInit( State* state );

#ifdef ten_TEST
    void
    ptrTest( State* state );
#endif

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
