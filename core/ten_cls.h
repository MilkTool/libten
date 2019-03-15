#ifndef ten_cls_h
#define ten_cls_h
#include "ten_types.h"

struct Closure {
    Function* fun;
    union {
        Data*     dat;
        Upvalue** upvals;
    } dat;
};

#define clsSize( STATE, CLS ) (sizeof(Closure))
#define clsTrav( STATE, CLS ) (clsTraverse( STATE, CLS ))
#define clsDest( STATE, CLS ) (clsDestruct( STATE, CLS ))

void
clsInit( State* state );

#ifdef ten_TEST
    void
    clsTest( State* state );
#endif

Closure*
clsNew( State* state, Function* fun, Data* dat );

void
clsTraverse( State* state, Closure* cls );

void
clsDestruct( State* state, Closure* cls );

#endif
