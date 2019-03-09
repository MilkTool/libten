// The State instance is the center of a Ten VM, it represents the
// global VM state and is responsible for managing resources and
// handling errors.  This is where all the other components come
// together to form a useable VM instance.

#ifndef ten_state_h
#define ten_state_h
#include "ten_types.h"
#include "ten_api.h"
#include <setjmp.h>
#include <stddef.h>
#include <stdarg.h>

// This represents the header for a garbage collected heap
// object.  The object is transparant to the rest of the
// other components of the system as its prepended to any
// object allocations.
typedef struct {
    
    // This tagged pointer holds a pointer to the next
    // GC object in the pool, as well as the following
    // bits in its tag portion:
    //
    // [rrrrrrrrrrr][m][tttt]
    //       11      1   4
    //
    // Where the [m] bit is the keep-alive mark used by the
    // tracing GC and [t] bits are used to store the object
    // type.  The [r] are reserved (a.k.a unused) for now.
    
    #define OBJ_MARK_SHIFT (4)
    #define OBJ_TAG_SHIFT  (0)
    #define OBJ_MARK_BIT   (0x1 << OBJ_MARK_SHIFT)
    #define OBJ_TAG_BITS   (0xF << OBJ_TAG_SHIFT)
    TPtr next;
    
    // The object's type specific data follows.
    char data[];
} Object;

// Convert an object pointer to and from a pointer to its data.
#define OBJ_DAT_OFFSET ((size_t)&((Object*)0)->data)
#define objGetDat( OBJ ) ((void*)(OBJ) + OBJ_DAT_OFFSET)
#define datGetObj( DAT ) ((Object*)((void*)(DAT) - OBJ_DAT_OFFSET))
#define datGetTag( DAT ) ((tpGetTag( datGetObj( DAT )->next ) >> OBJ_TAG_SHIFT) & OBJ_TAG_BITS)



// The way Ten's VM is setup, we're gonna be long jumping up the
// stack whenever an error occurs; this is efficient for error
// handling, but can lead to some loose ends as far as memory
// allocations and other resource managment goes if we long jump
// at the wrong times.  So to allow these jumps in a safe way without
// chance of a memory or resource leak we define two types: Part
// and Defer.
//
// The Part type is for allocating memory, it represents
// an incomplete object or raw memory allocation; providing access
// to the allocated memory with the condition that, should an error
// jump occur before the memory is committed (with another function
// call) it will be released automatically.  Object allocations also
// are not added to the GC list until committed, this gives us time
// to ensure the object is initialized to a valid GC state without
// worrying about the possibility of a cycle.
//
// The Defer type basically just allows us to register an arbitrary
// callback to be invoked in case of an error.  The callback
// is passed a pointer to the Defer itself; so the defer is expected
// to be extended with any other data it needs to perform its task.
//
// Both of these types are typically allocated on the native stack
// and linked into the kernel.  So if we forget to commit or otherwise
// unlink them before the call frame they're allocated in is popped
// the pointers can become corrupted; causing issues with the entire
// list.  So in debug builds we prefix and postfix these with magic
// numbers, which we validate regularly to check for corruption.

typedef struct Part {
    #define PART_BEGIN_NUM \
        ((uint)'P' << 24 | (uint)'B' << 16 | (uint)'M' << 8 | (uint)'N')
    #ifdef ten_DEBUG
        uint beginNum;
    #endif
    
    // Pointer to the next Part, and a pointer to the previous
    // Part's pointer to this one.  We use the second pointer
    // for making the adjustments necessary after removing this
    // Part form the list.
    struct Part*  next;
    struct Part** link;
    
    // A pointer to the allocation, this will either be a pointer
    // to a raw memory allocation or a pointer to the data portion
    // of an object allocation.
    void* ptr;
    
    // The size of the allocation, autmatically filled by the
    // allocator and should not be changed.
    size_t sz;
    
    #define PART_END_NUM \
        ((uint)'P' << 24 | (uint)'E' << 16 | (uint)'M' << 8 | (uint)'N')
    #ifdef ten_DEBUG
        uint endNum;
    #endif
} Part;


typedef struct Defer {
    #define DEFER_BEGIN_NUM \
        ((uint)'D' << 24 | (uint)'B' << 16 | (uint)'M' << 8 | (uint)'N')
    #ifdef ten_DEBUG
        uint beginNum;
    #endif
    
    // Pointer to the next Defer, and a pointer to the previous
    // Defer's pointer to this one.
    struct Defer*  next;
    struct Defer** link;
    
    // The actual callback to be called when an error occurs,
    // the user can choose whether or not to invoke it before
    // removing the Defer even if an error hasn't occured.
    // The error type and value itself will be set in the State
    // before calling, so can be retrieved from there.
    void (*cb)( State* state, struct Defer* self );
    
    #define DEFER_END_NUM \
        ((uint)'D' << 24 | (uint)'E' << 16 | (uint)'M' << 8 | (uint)'N')
    #ifdef ten_DEBUG
        uint endNum;
    #endif
} Defer;


// We try to keep the components of Ten's VM as isolated from each other
// as possible.  Whith each components knowning as little as possible
// about the inner workings of the others.  The State component
// implements GC for the VM as a whole, but shouldn't have to know
// about the internals of the other components; so it allows them
// to register Scanners and Finalizers.  The callbacks specified in
// Scanners will be called during each GC cycle, allowing each component
// to traverse and mark its GC values.  The Finalizers will be called
// before destruction of the VM, they can be registered to do any
// cleanup required by a component.  The callbacks from both types
// are passed a self pointer, so the structs are expected to be
// extended with any extra data needed for their task.

typedef struct Scanner {
    struct Scanner*  next;
    struct Scanner** link;
    
    void (*cb)( State* state, struct Scanner* self );
} Scanner;

typedef struct Finalizer {
    struct Finalizer*  next;
    struct Finalizer** link;
    
    void (*cb)( State* state, struct Finalizer* self );
} Finalizer;


// This is the State definition itself.  The only things it's directly
// responsible for is memory management (including GC) and error
// handling; otherwise it's just a place to put pointers to all the
// other components' state.  We also keep a few fields for statistics
// tracking here if ten_VERBOSE is defined.
struct State {
    
    // A copy of the VM configuration, its structure is specified
    // in the user API file `ten.h`.
    ten_Config config;
    
    // Pointers to the states of all other components.
    ApiState* apiState;
    ComState* comState;
    GenState* genState;
    EnvState* envState;
    FmtState* fmtState;
    SymState* symState;
    StrState* strState;
    IdxState* idxState;
    RecState* recState;
    FunState* funState;
    ClsState* clsState;
    FibState* fibState;
    UpvState* upvState;
    DatState* datState;
    PtrState* ptrState;
    
    // Error related stuff.  The `errJmp` points to the
    // current error handler, which will be jumped to
    // in case of an error.  The others contain the
    // type and value associated with the error, and
    // `trace` is where we put the generated stack
    // trace.  In addition to `errVal` we also have
    // `errStr`, which will be used in its place if
    // `errVal` is `udf`.  This allows us to report
    // errors as raw strings in critical cases where
    // a String object can't be allocated; this is
    // expected to always be a static string.
    jmp_buf*    errJmp;
    ten_ErrNum  errNum;
    TVal        errVal;
    char const* errStr;
    ten_Trace*  trace;
    
    // Current number of bytes allocated on the heap, and
    // the number that needs to be reached to triggern the
    // next GC.
    size_t memUsed;
    size_t memLimit;
    #define MEM_LIMIT_INIT   (2048)
    #define MEM_LIMIT_GROWTH (1.5)
    
    // For most garbage collection cycles Ten will only collect
    // heap objects linked into the `objects` list.  This lends
    // to its efficiency since Symbols and Pointers will generally
    // have a longer lifetime than most normal objects.  But
    // occasionally we perform a full GC cycle and scan for Symbols
    // and Pointers as well as objects.  These are used to keep
    // track of which cycle we're performing now and when to do
    // the next full collection.
    bool gcFull;
    uint gcCount;
    
    // List of GC objects.
    Object* objects;
    
    // The current fiber, this is the GC root pointer and
    // is where we start GC reference traversal after
    // invoking all the Scanners.
    Fiber* fiber;
    
    // The Part and Defer lists.  We have two lists for Parts,
    // one for raw memory allocations and another for object
    // allocations.
    Part*  rawParts;
    Part*  objParts;
    Defer* defers;
    
    // The Scanner and Finalizer lists.
    Scanner*   scanners;
    Finalizer* finalizers;
    
    
    // Runtime stats.
    #ifdef ten_VERBOSE
        size_t heapInitUsage;
        size_t heapMaxUsage;
        size_t gcNumCycles;
        size_t gcNumObjects;
        size_t gcMaxObjects;
        size_t gcMaxFreed;
        size_t gcAvgFreed;
        double gcMaxDelay;
        double gcAvgDelay;
        double vmInitDelay;
        double vmFinlDelay;
        double vmInitTime;
        double vmFinlTime;
    #endif
};

// Initialization and finalization.
void
stateInit( State* state, ten_Config const* config, jmp_buf* errJmp );

void
stateFinl( State* state );


// These are convenient routines for accessing the 'current'
// stack.  If there's a current running fiber (i.e `fib != NULL`)
// then these calls will be forwarded to that fiber; otherwise
// they'll be forwarded to the environment component.
Tup
statePush( State* state, uint n );

Tup
stateTop( State* state );

void
statePop( State* state );


// Throw an error.  The `stateErrStr()` should only be used for
// critical errors where a String allocation would otherwise
// fail; it expects `str` to be a static string, and will not
// release or copy it.  In most cases `stateErrFmt*()` is what
// we use, this uses the formatting component to generate a
// String object as the error value based on the given pattern
// and arguments.  The `stateErrVal()` specifies an error value
// directly.
void
stateErrStr( State* state, ten_ErrNum err, char const* str );

void
stateErrFmtA( State* state, ten_ErrNum err, char const* fmt, ... );

void
stateErrFmtV( State* state, ten_ErrNum err, char const* fmt, va_list ap );

void
stateErrVal( State* state, ten_ErrNum err, TVal val );

void
stateErrProp( State* state );


// At some places we need to intercept the errors produced to
// prevent them from propegating to the rest of the program;
// in such cases the unit in question calls this to replace the
// current error handling jump with its own; once the routine
// finishes it's expected to swap back to the original, so it
// must save the returned pointer.
jmp_buf*
stateSwapErrJmp( State* state, jmp_buf* jmp );


// Memory management.  The allocators return the same value as
// put in the `ptr` field of the given Part struct.  After
// allocation the respective `stateCommit*()` function must
// be called to finalize the allocation; this should be called
// only once the allocated 'thing' is initialized and there's
// no chance of a leak if an error occurs.
void*
stateAllocObj( State* state, Part* p, size_t sz, ObjTag tag );

void
stateCommitObj( State* state, Part* p );

void
stateCancelObj( State* state, Part* p );

void*
stateAllocRaw( State* state, Part* p, size_t sz );

void*
stateResizeRaw( State* state, Part* p, size_t sz );

void
stateCommitRaw( State* state, Part* p );

void
stateCancelRaw( State* state, Part* p );

void
stateFreeRaw( State* state, void* old, size_t osz );


// Defers.  Once registered a defer will be invoked if
// an error occurs; but they can also be invoked manually
// with `stateCommitDefer()` or removed from the list
// with `stateCancelDefer()`.
void
stateInstallDefer( State* state, Defer* defer );

void
stateCommitDefer( State* state, Defer* defer );

void
stateCancelDefer( State* state, Defer* defer );


// Install and remove Scanners and Finalizers.
void
stateInstallScanner( State* state, Scanner* scanner );

void
stateRemoveScanner( State* state, Scanner* scanner );

void
stateInstallFinalizer( State* state, Finalizer* finalizer );

void
stateRemoveFinalizer( State* state, Finalizer* finalizer );


// Push a trace entry to the front of the `trace`, this
// allows arbitrary components to add more specificity
// to stack traces.
void
statePushTrace( State* state, char const* file, uint line );

// Clear the current `trace`, this frees all entries.
void
stateClearTrace( State* state );

// Clear the current error value and trace.
void
stateClearError( State* state );

// Mark an object reachable and scan it for further references.
void
stateMark( State* state, void* ptr );

// Force a GC cycle.
void
stateCollect( State* state );

#endif
