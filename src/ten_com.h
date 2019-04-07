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

#ifdef ten_TEST
    void
    comTest( State* state );
#endif

Closure*
comCompile( State* state, ComParams* params );


#endif
