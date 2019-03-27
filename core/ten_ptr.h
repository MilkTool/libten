#ifndef ten_ptr_h
#define ten_ptr_h
#include "ten_types.h"
#include "ten_api.h"
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    SymT  type;
    void  (*destr)( ten_State* core, void* ptr );
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
