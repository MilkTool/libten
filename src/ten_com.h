/***********************************************************************
This component implements Ten's compiler, which consists of the
parser and somewhat high level code generation, which is augmented
by the lower level code generator implemented in `ten_gen.*`.  The
compiler does everything in a single pass, and currently doesn't do
any constant reduction.
***********************************************************************/

#ifndef ten_com_h
#define ten_com_h
#include "ten.h"
#include "ten_types.h"

typedef struct {
    char const*  file;
    char const*  name;
    char const** params;
    char const** upvals;
    
    bool debug;
    bool global;
    bool script;
    
    ten_Source* src;
} ComParams;

void
comInit( State* state );

Closure*
comCompile( State* state, ComParams* params );


#endif
