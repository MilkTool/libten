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

#endif
