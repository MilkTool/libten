/***********************************************************************
This component implements Ten's Pointer data type, which is similar to
the Data data type in that it's used exclusively for interacting with
native code.  Unlike Data values though, pointers REFER to a block of
C memory, rather than containing one; so the memory itself is exclusively
managed by native code.

This type is loosely similar to Lua's lightuserdata type, except unlike
Lua's equivalent; Ten's pointers can keep track of the original C data
type (from which the pointer was cast) so preserves some degree of type
safety, and Ten allows registration of a destructor callback, which will
be called when all occurences of the pointer reference expire from the
Ten runtime.
***********************************************************************/

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
    void  (*destr)( void* addr );
};

void
ptrInit( State* state );

PtrInfo*
ptrAddInfo( State* state, ten_PtrConfig* config );

bool
ptrExists( State* state, PtrInfo* info, void* addr );

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
