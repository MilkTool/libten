/***********************************************************************
This component implements the global variable pool of a Ten instance,
and the global stack, which serves as the current value stack when no
fibers are running.
***********************************************************************/

#ifndef ten_env_h
#define ten_env_h
#include "ten_types.h"

// Initialize the component.
void
envInit( State* state );

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
