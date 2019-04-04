// The environment component of Ten implements the VM's global
// environment, as well as the global stack which is used for
// allocating variables when no fiber is running.
#ifndef ten_env_h
#define ten_env_h
#include "ten_types.h"

// Initialize and test the component.
void
envInit( State* state );

#ifdef ten_TEST
    void
    envTest( State* state );
#endif


// Push and pop values to/from the stack.
Tup
envPush( State* state, uint n );

Tup
envTop( State* state );

void
envPop( State* state );

// Add and reference global variables.
uint
envAddGlobal( State* state, SymT name );

TVal*
envGetGlobalByName( State* state, SymT name );

TVal*
envGetGlobalByLoc( State* state, uint loc );

#endif
