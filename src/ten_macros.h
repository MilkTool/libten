// Here we define a few utilitiy macros that are used throughout the
// rest of the code base.  Most macros specific to a file or component
// are defined privately within the unit's implementation.  But those
// defined here are used extensively in various unrelated units, so
// they get their own header so we don't have to redefine them in each
// unit's code.
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

// The vget() macro has been implemented to prevent assignement
// on purpose as this causes issues when the pointers on
// the left are evaluated before the right hand expression
// causes changes to the stack or other shared state.  So
// use the vset() macro for setting variable from values.
// This macro can still cause issues when used multiple times
// in an expression with side effects, so we need to be
// careful about where it's used.
#define vget( VAR ) expAssert(                                              \
    (VAR).loc < ((Tup*)(VAR).tup)->size,                                    \
    ( tvUdf(), *(*((Tup*)(VAR).tup)->base + ((Tup*)(VAR).tup)->offset + (VAR).loc) ), \
    "Variable 'loc' out of tuple bounds, tuple size is %u",                 \
    ((Tup*)(VAR).tup)->size                                                 \
)

// The vset() macro overcomes some of the shortcomings of the ref()
// macro by forcing the right hand side to be evaluated first.
#define vset( VAR, VAL )                                        \
    do {                                                        \
        Tup* tup = (Tup*)(VAR).tup;                             \
        fmtAssert(                                              \
            (VAR).loc < tup->size,                              \
            "Variable 'loc' out of tuple bounds, "              \
            "tuple size is %u",                                 \
            tup->size                                           \
        );                                                      \
                                                                \
        TVal rhs = (VAL);                                       \
                                                                \
        *(*tup->base + tup->offset + (VAR).loc) = rhs;          \
    } while( 0 )


#define panic( FMT... )                                         \
    do {                                                        \
        char const* fiber = NULL;                               \
        if( state->fiber && state->fiber->tagged )              \
            fiber = symBuf( state, state->fiber->tag );         \
        statePushTrace( state, fiber, __FILE__, __LINE__ );     \
        stateErrFmtA( state, ten_ERR_PANIC, FMT );              \
    } while( 0 )

#endif
