/*
Here we implement the Fiber type, which represents a thread of
execution.  Unlike most interpreter implementations, which put
the main interpreter loop in a separate VM file, in Ten it makes
sense to put the interpreter loop in the Fiber implementation
since the Fiber is where all the execution state will be stored.

While most of the fiber's implementation is fairly straightforward,
and the bulk of it is really implemented in the individual operation
implementations in `inc/ops/`; the stack system used for Ten's
activation records is a bit... complex, and warrants an in depth
explanation.  Which I should write, when I figure out how this
will work.
*/
#ifndef ten_fib_h
#define ten_fib_h
#include "ten.h"
#include "ten_types.h"
#include "ten_state.h"

typedef struct {
    Closure*  cls;
    uint      lcl;
} AR;

typedef struct VirAR VirAR;
typedef struct NatAR NatAR;
typedef struct ConAR ConAR;

struct VirAR {
    AR          base;
    instr*      ip;
    NatAR*      nats;
    ConAR*      cons;
};

struct NatAR {
    AR          base;
    NatAR*      prev;
    
    char const* file;
    uint        line;
    
    void*       context;
    size_t      ctxSize;
    uintptr_t   dstOffset;
    long        checkpoint;
};

struct ConAR {
    AR          base;
    ConAR*      prev;
    
    char const* file;
    uint        line;
    
    void*       context;
    size_t      ctxSize;
    uintptr_t   dstOffset;
    long        checkpoint;
};

typedef struct {
    instr*      ip;
    TVal*       sp;
    Closure*    cls;
    TVal*       lcl;

    void*       context;
    size_t      ctxSize;
    uintptr_t   dstOffset;
    long        checkpoint;
} Regs;

struct Fiber {

    NatAR* nats;
    ConAR* cons;
    
    // Dynamic stack/array of VirARs.
    struct {
        VirAR* buf;
        uint   top;
        uint   cap;
    } virs;
    
    // The temporary and local stack.
    struct {
        TVal* buf;
        uint  cap;
    } stack;
    
    void (*pop)( State* state, Fiber* fib );
    void (*push)( State* state, Fiber* fib, NatAR* nat );
    
    
    // Current state of the fiber.
    ten_FibState state;
    
    
    // Flag indicating if the fiber is tagged, and the tag
    // itself if it is.
    bool tagged;
    SymT tag;
    
    // A pointer to the current register set, this'll point
    // to `rbuf` when the fiber isn't active (running).
    Regs* rptr;
    Regs  rbuf;
    
    
    // The closure wrapped by the fiber; the fiber's
    // entry point.  This will be set to `NULL` after
    // the first continuation to indicate that the
    // fiber has already been initialized.
    Closure* entry;
    
    // The fiber that started/continued this one, or NULL
    // if this is a root fiber.
    Fiber* parent;
    
    // Error information.
    ten_ErrNum  errNum;
    TVal        errVal;
    ten_Trace*  trace;
    
    // This is a defer that'll be registered at the start of
    // each continuation.  If an error occurs while the fiber
    // is running then it'll create a stack trace for the
    // fiber and release unneeded resources.
    Defer defer;
    
    // This is where we'll jump to if we want to yield
    // execution control to the parent fiber.
    jmp_buf* yjmp;
};

#define fibSize( STATE, FIB ) (sizeof(Fiber))
#define fibTrav( STATE, FIB ) (fibTraverse( STATE, FIB ))
#define fibDest( STATE, FIB ) (fibDestruct( STATE, FIB ))

void
fibInit( State* state );

Fiber*
fibNew( State* state, Closure* entry, SymT* tag );

Tup
fibPush( State* state, Fiber* fib, uint n );

Tup
fibTop( State* state, Fiber* fib );

void
fibPop( State* state, Fiber* fib );

Tup
fibCont( State* state, Fiber* fib, Tup* args );

#define fibCall( STATE, CLS, ARGS ) \
    fibCall_( STATE, CLS, ARGS, __FILE__, __LINE__ )

Tup
fibCall_( State* state, Closure* cls, Tup* args, char const* file, uint line );

void
fibYield( State* state, Tup* vals, bool pop );

long
fibSeek( State* state, void* ctx, size_t size );

void
fibCheckpoint( State* state, unsigned cp, Tup* tup );

void
fibClearError( State* state, Fiber* fib );

void
fibPropError( State* state, Fiber* fib );

void
fibTraverse( State* state, Fiber* fib );

void
fibDestruct( State* state, Fiber* fib );

#endif
