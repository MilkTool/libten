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
    (TYPE*)((void*)(SCAN) - (ullong)&((TYPE*)NULL)->scan)

#define structFromFinl( TYPE, FINL ) \
    (TYPE*)((void*)(FINL) - (ullong)&((TYPE*)NULL)->finl)


#define identCat_( A, B ) A ## B
#define identCat( A, B )  identCat_( A, B )

#define isEmpty( DEF ) (identCat( DEF, 1 ) == 1)

#define ref( VAR ) *expAssert(                                               \
    (VAR)->loc < ((Tup*)(VAR)->tup)->size,                                  \
    *((Tup*)(VAR)->tup)->base + ((Tup*)(VAR)->tup)->offset + (VAR)->loc,    \
    "Variable 'loc' out of tuple bounds, tuple size is %u",                                   \
    ((Tup*)(VAR)->tup)->size                                                                    \
)

#define panic( FMT... )                                         \
    do {                                                        \
        char const* fiber = NULL;                               \
        if( state->fiber )                                      \
            fiber = symBuf( state, state->fiber->tag );         \
        statePushTrace( state, fiber, __FILE__, __LINE__ );     \
        stateErrFmtA( state, ten_ERR_PANIC, FMT );              \
    } while( 0 )

#endif
