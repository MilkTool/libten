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
explanation.  The merits of this more complex design are in its
ability to maintain decent performance, while supporting Ten's
somewhat complicated reentrancy system for native function.

The system consists of three types of AR (Activation Record) structs,
all 'inheriting' the structure of the AR base type.  The fiber
maintains a vew members:

    AR*    top;
    void*  pod;
    void (*pop)( State* state );
    void (*push)( State* state, NatAR* nat );

For maintaining the top of the stack.  The `top` pointer is seen
as just an AR struct by most of the system, but the two function
pointers should know how to pop and push, respectively, new ARs
to the stack; these should be swapped out depending on the current
type of AR in `top`.  The `pod` pointer just provides a bit of
state for the functions.

The actual structure of the stack is a bit difficult to present
without the use of richer graphics, but I'll see what I can do:


            CONS               VIRS                    NATS
                            [VVVVVVVV]
                            [VVVVVVVV]
                            [VVVVVVVV]
              [NNNNNNNN]    [VVVVVVVV]
                   ^        [VVVVVVVV]  -> [NNNNNNNN] -> [NNNNNNNN]
              [NNNNNNNN]    [VVVVVVVV]
                   ^        [VVVVVVVV]
[CCCCCCCC] <- [CCCCCCCC] <- [VVVVVVVV]  -> [NNNNNNNN]


Yeah... that probably isn't very helpful.  But basically we have
three different types of AR.  The VirARs keep the saved state of
virtual functions, and are allocated in a dynamic array used as
an actuall stack, so for every active virtual function call a
slot in the `virs` array will be allocated.

The NatAR activation record represents the saved registers for
a native function call, these are allocated on the native stack
and just linked into the fiber; each VirAR contains a `nats`
field which gives the start of a linked list of NatARs, these
represent the native functions called just above the respective
VirAR's function, without any intemediate virtual function calls.

The ConAR (continuation AR) records come out of another list on
the virtual record.  These represent native functions that
were on the stack when the last fiber yield was invoked, and
so need to be 'continued' for a proper unwinding of the stack.
These are allocated on the heap, and must be invoked (continued)
in order befor the associated VirAR is popped from the stack
to populate the current register set.  The list of ConARs
is directly copied from the list of NatARs right before a yield;
the main difference between the two is that ConARs can also have
a list of NatARs to represent the native calls made directly
above the function continuation, without an intermediate virtual
call.

In addition to this mess of stacks and lists, an additional `nats`
list is maintained in the fiber itself, for native calls made
directly above the fiber, before any virtual functions have been
called.

*/
#ifndef ten_fib_h
#define ten_fib_h
#include "ten.h"
#include "ten_types.h"
#include "ten_state.h"

typedef struct {
    Closure*  cls;
    instr*    ip;
    uint      lcl;
} AR;

typedef struct VirAR VirAR;
typedef struct NatAR NatAR;
typedef struct ConAR ConAR;

struct VirAR {
    AR          base;
    NatAR*      nats;
    ConAR*      cons;
};

struct NatAR {
    AR          base;
    NatAR*      prev;
    char const* file;
    uint        line;
};

struct ConAR {
    AR          base;
    ConAR*      prev;
    NatAR*      nats;
};

typedef struct {
    instr* ip;
    TVal*  sp;
    Closure* cls;
    TVal*    lcl;
} Regs;

struct Fiber {

    // A stack/list for NatARs representing functions
    // that were called before any VirARs were pushed
    // the stack.
    NatAR* nats;
    
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
    
    // Pre-allocated error messages.
    String* errOutOfMem;
    
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

void
fibClearError( State* state, Fiber* fib );

void
fibPropError( State* state, Fiber* fib );

void
fibTraverse( State* state, Fiber* fib );

void
fibDestruct( State* state, Fiber* fib );

#endif
