/***********************************************************************
This file defines some general utility macros used elsewhere in the
implementation.
***********************************************************************/

#ifndef ten_macros_h
#define ten_macros_h

#define addNode( LIST, NODE )                           \
    do {                                                \
        (NODE)->next = *(LIST);                         \
        (NODE)->link = (LIST);                          \
        *(LIST) = (NODE);                               \
        if( (NODE)->next )                              \
            (NODE)->next->link = &(NODE)->next;         \
    } while( 0 )

#define remNode( NODE )                                 \
    do {                                                \
        *(NODE)->link = (NODE)->next;                   \
        if( (NODE)->next )                              \
            (NODE)->next->link = (NODE)->link;          \
    } while( 0 )

#define structFromScan( TYPE, SCAN ) \
    (TYPE*)((void*)(SCAN) - (uintptr_t)&((TYPE*)NULL)->scan)

#define structFromFinl( TYPE, FINL ) \
    (TYPE*)((void*)(FINL) - (uintptr_t)&((TYPE*)NULL)->finl)


#define identCat_( A, B ) A ## B
#define identCat( A, B )  identCat_( A, B )

#define isEmpty( DEF ) (identCat( DEF, 1 ) == 1)

// The varGet() and tupGet() macros have been implemented to
// prevent assignement on purpose as assignment causes issues
// when the pointers on the left are evaluated before the right
// hand expression causes changes to the stack or other shared
// state.  So use the *Set() macros for setting values.
// This macro can still cause issues when used multiple times
// in an expression with side effects, so we need to be
// careful about where it's used.
#define tupGet( TUP, LOC ) expAssert(                                       \
    (LOC) < (TUP).size,                                                     \
    *( 0, *(TUP).base + (TUP).offset + (LOC) ),                             \
    "Attempt to access out of range tuple slot",                            \
    NULL                                                                    \
)

#define varGet( VAR ) tupGet( *(Tup*)(VAR).tup, (VAR).loc )


#define tupSet( TUP, LOC, VAL ) do {                            \
    Tup* _tup = &(TUP);                                         \
    TVal _val = (VAL);                                          \
    fmtAssert(                                                  \
        (LOC) < _tup->size,                                     \
        "Attempt to access out of range tuple slot",            \
        NULL                                                    \
    );                                                          \
                                                                \
    *(*_tup->base + _tup->offset + (LOC) ) = _val;              \
} while( 0 )

#define varSet( VAR, VAL ) tupSet( *(Tup*)(VAR).tup, (VAR).loc, (VAL) )


#define panic( FMT... )                                         \
    do {                                                        \
        char const* fiber = NULL;                               \
        if( state->fiber && state->fiber->tagged )              \
            fiber = symBuf( state, state->fiber->tag );         \
        statePushTrace( state, fiber, __FILE__, __LINE__ );     \
        stateErrFmtA( state, ten_ERR_PANIC, FMT );              \
    } while( 0 )

#define elemsof( ARRAY ) (sizeof(ARRAY)/sizeof((ARRAY)[0]))
#endif
