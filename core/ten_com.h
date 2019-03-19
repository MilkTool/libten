#ifndef ten_com_h
#define ten_com_h
#include "ten_types.h"
#include "ten_api.h"

typedef struct {
    char const*  file;
    char const** params;
    
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

Function*
comCompile( State* state, ComParams* params );


#endif
