#if !defined(BUF_NAME) || isEmpty(BUF_NAME)
    #error "BUF_NAME must be defined before including buf.inc"
#endif

#if !defined(BUF_TYPE) || isEmpty(BUF_TYPE)
    #error "BUF_TYPE must be defined before including buf.inc"
#endif

typedef struct {
    BUF_TYPE* buf;
    uint      cap;
    uint      top;
} BUF_NAME;

static void
identCat( init, BUF_NAME )( State* state, BUF_NAME* b ) {
    // Zero everything in case a GC cycle occurs while
    // allocating and the buffer is scanned for objects.
    b->buf = NULL;
    b->top = 0;
    b->cap = 0;
    
    uint cap = 7;
    Part bufP;
    BUF_TYPE* buf = stateAllocRaw( state, &bufP, sizeof(BUF_TYPE)*cap );
    
    b->buf = buf;
    b->top = 0;
    b->cap = cap;
    
    stateCommitRaw( state, &bufP );
}

static void
identCat( finl, BUF_NAME )( State* state, BUF_NAME* b ) {
    stateFreeRaw( state, b->buf, sizeof(BUF_TYPE)*b->cap );
    b->buf = NULL;
    b->top = 0;
    b->cap = 0;
}

static BUF_TYPE*
identCat( put, BUF_NAME )( State* state, BUF_NAME* b ) {
    if( b->top >= b->cap ) {
        Part bufP = { .ptr = b->buf, .sz = sizeof(BUF_TYPE)*b->cap };
        
        uint cap = b->cap * 2;
        BUF_TYPE* buf = stateResizeRaw( state, &bufP, sizeof(BUF_TYPE)*cap );
        b->buf = buf;
        b->cap = cap;
        stateCommitRaw( state, &bufP );
    }
    return &b->buf[b->top++];
}

static BUF_TYPE*
identCat( pack, BUF_NAME )( State* state, BUF_NAME* b ) {
    BUF_TYPE* buf = b->buf;
    if( b->cap > b->top ) {
        Part bufP = { .ptr = b->buf, .sz = sizeof(BUF_TYPE)*b->cap };
        buf = stateResizeRaw( state, &bufP, sizeof(BUF_TYPE)*b->top );
        stateCommitRaw( state, &bufP );
    }
    b->buf = NULL;
    b->top = 0;
    b->cap = 0;
    return buf;
}

static void
identCat( ensure, BUF_NAME )( State* state, BUF_NAME* b, uint n ) {
    uint need = b->top + n;
    if( need > b->cap ) {
        Part bufP = { .ptr = b->buf, .sz = sizeof(BUF_TYPE)*b->cap };
        
        uint cap = b->cap * 2;
        if( cap < need )
            cap = need;
        BUF_TYPE* buf = stateResizeRaw( state, &bufP, sizeof(BUF_TYPE)*cap );
        b->buf = buf;
        b->cap = cap;
        stateCommitRaw( state, &bufP );
    }
}
