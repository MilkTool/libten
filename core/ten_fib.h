// Here we implement the Fiber type, which represents a thread of
// execution.  Unlike most other interpreters, which put the main
// interpreter loop in a separate VM file, in Ten it makes sense
// to put the interpreter loop in the Fiber implementation since
// the Fiber is where all the execution state will be stored.
#ifndef ten_fib_h
#define ten_fib_h
#include "ten_types.h"
#include "ten_state.h"
#include "ten_api.h"

typedef struct {
    Closure*  cls;
    instr*    rAddr;
    uint      oLcls;
} AR;

typedef struct NatAR {
    AR ar;
    
    // Where the call was made from.
    char const* file;
    uint        line;
    
    // Pointers to previous native activation record.
    struct NatAR* prev;
} NatAR;

typedef struct {
    AR      ar;
    NatAR*  nats;
} VirAR;

typedef enum {
    FIB_RUNNING,
    FIB_WAITING,
    FIB_STOPPED,
    FIB_FINISHED,
    FIB_FAILED
} FibStatus;

typedef struct {
    instr* ip;
    TVal*  sp;
    Closure* cls;
    TVal*    lcl;
} Regs;

struct Fiber {
    // We use this in place of the `nats` list of a VirAR when
    // the first function called by the fiber, the entry function,
    // is a native function.
    NatAR* nats;
    
    // Current state of the fiber.
    FibStatus state;
    
    
    // The three stacks.
    struct {
        VirAR* ars;
        uint   cap;
        uint   top;
    } arStack;

    struct {
        TVal*  tmps;
        uint   cap;
    } tmpStack;
    
    // Current set of registers, these are allocated as
    // locals but linked here for the sake of GC and
    // stack allocation.  When the fiber isn't running
    // this points to rBuf.
    Regs* rPtr;
    Regs  rBuf;
    
    // The closure used as the entry point into the fiber.
    Closure* entry;
    
    // The parent fiber, the one that continued this one.  Will be
    // NULL if this is is the root fiber.
    Fiber* parent;
    
    // Error information.
    ten_ErrNum  errNum;
    ten_Trace*  errTrace;
    TVal        errVal;
    char const* errStr;
    
    // This is a defer that'll be registered at the start of
    // each continuation.  If an error occurs while the fiber
    // is running then it'll create a stack trace for the
    // fiber and release unneeded resources.
    Defer errDefer;
    
    // This is where we'll jump to if we want to yield control
    // to the parent fiber.
    jmp_buf* yieldJmp;
};

#define fibSize( STATE, FIB ) (sizeof(Fiber))
#define fibTrav( STATE, FIB ) (fibTraverse( STATE, FIB ))
#define fibDest( STATE, FIB ) (fibDestruct( STATE, FIB ))

void
fibInit( State* state );

Fiber*
fibNew( State* state, Closure* entry );

Tup
fibPush( State* state, Fiber* fib, uint n );

Tup
fibTop( State* state, Fiber* fib );

void
fibPop( State* state, Fiber* fib );

Tup
fibCont( State* state, Fiber* fib, Tup* args );

void
fibYield( State* state );

void
fibTraverse( State* state, Fiber* fib );

void
fibDestruct( State* state, Fiber* fib );

#endif
