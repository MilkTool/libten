// Here we implement the Fiber type, which represents a thread of
// execution.  Unlike most other interpreters, which put the main
// interpreter loop in a separate VM file, in Ten it makes sense
// to put the interpreter loop in the Fiber implementation since
// the Fiber is where all the execution state will be stored.
//
// Ten's Fiber implementation also has a bit of an unusual structure
// which is worth explaining here to help understand the implementation
// itself.
//
// Each Fiber has three stacks instead of the usual two.  We have a
// stack for locals and temporary values `tmpStack`, a stack for
// upvalues `upvStack`, and another for activation records `arStack`.
//
// The semantics of Ten's variables, which allow them to be undefined
// or redefined, severing its connection to any closures; pevents us
// from using Lua's strategy of maintaining a linked list of upvalues
// for each activation record since we need to be able to figure out
// what upvalue a variable is linked to so we can close the upvalue
// upon redefinition.  So we maintain a separate stack of upvalues
// and for each value on the temporary stack allocate an additional
// byte which can be used to link to an upvalue on the stack.
//
// The activation record struct `VirAR` may also look a bit odd,
// and what's with that naming?  Ten's VM has a unique, as of this
// writing, feature in which at allocates activation records for
// calls from native functions as well as virtual ones.  So we have two
// types of activation records `VirAR` and `NatAR`.  The virtual
// activation records are those that are allocated on the `arStack`
// whilst `NatAR` structs are allocated on the native stack, and
// linked to the nearest `VirAR` with its `nats` field.  This use
// of activation records for calls from native functions allows us
// to keep track of where that call was made from, to generate an
// appropriate stack trace that includes the locations of calls made
// from native source code.
#ifndef ten_fib_h
#define ten_fib_h

typedef struct {
    Closure*  closure;
    TVal*     locals;
    Upvalue*  upvals;
    instr*    rAddr;
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
    NatAr*  nats;
} VirAR;

typedef enum {
    FIB_RUNNING,
    FIB_WAITING,
    FIB_STOPPED,
    FIB_FINISHED,
    FIB_FAILED
} FibState;

struct Fiber {
    // We use this in place of the `nats` list of a VirAR when
    // the first function called by the fiber, the entry function,
    // is a native function.
    NatAR* nats;
    
    
    // The three stacks.
    struct {
        VirAR* ars;
        uint   cap;
        uint   top;
    } arStack;

    struct {
        TVal*  tmps;
        uchar* upvs;
        uint   cap;
        uint   top;
    } tmpStack;
    
    struct {
        Upvalue** upvs;
        uint      cap;
        uint      top;
    } upvStack;
    
    // The parent fiber, the one that continued this one.  Will be
    // NULL if this is is the root fiber.
    Fiber* parent;
    
    // Error information.
    ten_ErrNum    errNum;
    ten_ErrTrace* errTrace;
    TVal          errVal;
    char const*   errStr;
    
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

Fiber*
fibNew( State* state, Closure* entry );

Tup
fibPush( State* state, Fiber* fib, uint n );

Tup
fibTop( State* state, Fiber* fib );

void
fibPop( State* state, Fiber* fib );

void
fibCont( State* state, Fiber* fib, Tup* args );

void
fibYield( State* state );

void
fibTraverse( State* state, Fiber* fib );

void
fibDestruct( State* state, Fiber* fib );

#endif
