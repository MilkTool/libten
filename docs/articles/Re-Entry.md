# Re-Entry
One of the neat feature's of Ten's runtime implementation is its portable
implementation of native function re-entry.  What this means is that a native
function, called from Ten code, can be implemented in such a way that its
call can be interrupted, only to continue execution later.

Implementing this functionality in a portable way becomes nearly essential
for Ten due to its mostly standards (C99) compliant implementation, and its
inherent concurrency through fibers.

For example consider the following snippet of C code, implementing the
native function `foo()`.

    ten_Tup
    tf_foo( ten_PARAMS ) {
        ten_Tup vals = ten_pushA( ten, "UU" );
        ...
        ten_yield( ten, &vals );
        ...
        return vals;
    }

Imagine this is called from an independent Ten fiber.

    def fib: fiber[] foo()
    cont( fib, {} )

The `ten_yield()` call is expected to pause the function, along with the
fiber as a whole, in its current state to be resumed later.  But this is
easier said than done.

While it's easy enough to implement the 'pause' effect of yielding a fiber
when we're only dealing with virtual (bytecode) functions; since we have
direct access to all virtual registers and can save/restore them without issue.
Native functions, such as the one above, are more troublesome since the C99
standard offers no way to save and restore the state of a function call outside
the boundaries of the runtime stack.

If we weren't aiming for full portability, it would be possible to dive
into assembly or dissect the call ABI to achieve the same effect (save/restore
the state of a call frame independent of its original stack location).  Alas,
such hackery can't be relied upon in Ten's runtime.

This document gives an overview of the strategy Ten uses to implement native
function call re-entry, it may be helpful to refer to the [relevant section
of the manual](../manual/the-api.md#5.9.1) as the API used to access this
functionality isn't described in detail here.

## The AR Stack
Every Ten fiber maintains its own AR (Activation Record) stack, which is
where we save the state of previous call frames before calling into a new
function.  Only Ten's AR stack is a bit special as it allows us to save
not only the state of bytecode function calls, but also that of native calls;
and it does so in a way that's compatible with the `cont()`/`yield()`
semantics of fibers.

To achieve these features, the stack is structured a bit... unconventionally;
here's what it looks like:

![AR Stack Organization][ar-stack-organization.svg]

Each box in the diagram, except the `Fiber` box, represents an activation
record, which contains the data needed to restore the function call it
represents to its previous state.

The stack is composed of three different types of AR, indicated by their
colors above; however all are based on a common `AR` base structure.

    typedef struct {
        Closure*  cls;
        uint      lcl;
    } AR;

Which keeps a pointer to the called closure, and an offset the the
start of the call's local values on the value stack; for native calls this
will be where all local allocations are made, with `ten_pushA()` and such.

The other AR types track the state of different types of function calls.
The `VirAR`s are quite simple, they represent virtual function calls, for
which only the instruction pointer needs to be saved, in addition to the
fields in the base AR.

    struct VirAR {
        AR          base;
        instr*      ip;
        ...
    };

A `NatAR` struct represents the saved state of a native function call, the
C ABI will save and restore the native registers, and we have no direct
access to them; but we still need a way to save and restore the state of a
native function call.

    struct NatAR {
        AR          base;
        ...
        void*       context;
        size_t      ctxSize;
        uintptr_t   dstOffset;
        long        checkpoint;
    };

The `.context` is `NULL` by default, but can be populated with a call to
`ten_seek()`; the `.ctxSize` keeps the size of the struct referenced by
`.context`.  Together these track the state of a native function call,
which should be fully contained within `.context`.  Consider the following
adaption of the `foo()` implementation mentioned previously.

    ten_Tup
    tf_foo( ten_PARAMS ) {
        struct {
            ten_Tup vals;
        } ctx;

        switch( ten_seek( ten, &ctx, sizeof( ctx ) ) ) {
            ...
        }

        ctx.vals = ten_pushA( ten, "UU" );
        ...
        ten_yield( ten, &vals );
        ...
        return ctx.vals;
    }

The call to `ten_seek()` tells Ten where the call's state is stored
by setting `.context = &ctx` and `.ctxSize = sizeof( ctx )`,
allowing the runtime to copy the state onto the heap, preserving it,
if a yield occurs within the function.

The `.checkpoint` and `.dstOffset` fields are set with `ten_checkpoint()`,
which tells the where to continue the call's execution from if
if interrupted after the checkpoint is set.  Let's add a bit more to the
above definition.

    ten_Tup
    tf_foo( ten_PARAMS ) {
        struct {
            ten_Tup vals;
            ten_Tup cont;
        } ctx;

        switch( ten_seek( ten, &ctx, sizeof( ctx ) ) ) {
            case 0: goto checkpoint0;
        }

        ctx.vals = ten_pushA( ten, "UU" );
        ...

        ten_checkpoint( ten, 0, &ctx.cont );
            ten_yield( ten, &ctx.vals );
        checkpoint0:

        ...
        return ctx.vals;
    }

The `ten_checkpoint()` call sets `.checkpoint` to the given checkpoint number,
in this case `.checkpoint = 0`.  The function expects the given destination
tuple, in this case `ctx.cont` to be allocated within `ctx`; so it sets
`.dstOffset = (void*)&ctx.cont - .context`.

The `.dstOffset` tells the runtime where to put the call's continuation
arguments when resumed.  For example in the above case, since this function
calls `ten_yield()` directly, the continuation arguments will consist of
the arguments passed to the next continuation of the fiber via `cont()`.
For example if we modify the previous Ten snippet to:

    def fib: fiber[] foo()
    cont( fib, {} )         `first entry into the call, pauses at ten_yield()
    cont( fib, { 1, 2 } )   `second entry, the runtime puts ( 1, 2 ) into ctx.cont

The second and subsequent entries into a native call, after yield, restore
`ctx` to its state just before the yield, with the exception of the tuple
at `.dstOffset` which is set to reference the top of the value stack where
the continuation arguments have been placed.  In addition the `ten_seek()`
function returns the last checkpoint encountered in the previous entry, so
in the second entry it'll return `0` to reflect the previous `ten_checkpoint()`
call; causing the switch to jump over the code that's already executed, thus
effectively restoring the call's instruction pointer from its previous entry.

The `ConAR` struct is defined exactly the same as `NatAR`, the difference is
how it's allocated, and what it's used for.  `ConAR`s represent native calls
that hadn't finished execution when a fiber yield occurred, thus they're copied
directly from the respective `NatAR`.  But unlike their `NatAR` counterparts,
which are allocated on the native C stack, and reference the raw `.context`
pointer given by the user (generally also stack allocated); `ConAR`s are
heap allocated, and hold a heap allocated copy of the original `.context`
data.

    struct ConAR {
        AR          base;
        ...
        void*       context;
        size_t      ctxSize;
        uintptr_t   dstOffset;
        long        checkpoint;
    };

Before a yield Ten converts all `NatAR`s to respective `ConAR`s, to allow
their continued existence independent of the native C stack, which will
be partially destroyed in the yield.  The following diagram gives a flattened
view of this conversion on the previously illustrated stack setup.

![AR Stack Conversion][ar-stack-organization-conversion.svg]

The linkage fields, used to tie the ARs together into a stack, are omitted
from this discussion as the linkage itself is less important than the
structure.  However it should be noted how we flatten the stack from
the previous diagram above.  The `cons` list preceeds the respective `nats`
 list, and the fiber's two base lists preceed all `VirAR`s, which themselves
 each preceed their respective two lists.  The arrows indicate linkage from
 top to bottom, so they point toward the previous stack item.


## Call Entries
Each native function call will be entered at least once.  While executing
within the call, the state of this entry is maintained by the current
register set.

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

If a call occurs on top of the current one, then the native call state
will be pushed to a `NatAR` allocated on the native C stack.  If a yield
is invoked directly by a native function, as in the previous example, then
there's no need to create a `ConAR` for the call, it can be re-entered directly
from the register set which is preserved across fiber continuations.

If the yield occurs within a function called by the native function, then
the `NatAR` associated with the call's saved state is converted to a `ConAR`
which preserves the call state across continuations.

Continuing a fiber after a yield requires the `ConAR`s at the top of the
stack, if any, be evaluated before any `VirAR`s can continue execution.  For
non-re-entrant calls (those with `.context == NULL`) the runtime just removes
the AR from the stack, and returns an empty tuple `()` on the function call's
behalf since it isn't capable of returning anything itself.

For re-entrant calls Ten will call back into the native function callback,
allowing it to restore its saved state with `ten_seek()`; all previous
variable allocations (on the value stack) made in the previous entry will
still be available and accessible via the restored tuples in `ctx`.

## Note
This description needs some work... It's difficult to describe the
re-entry system at a reasonable level of abstraction.  Checkout the code
([`ten_fib.h`][ten_fib.h] and [`ten_fib.c`][ten_fib.c])
to try and get a better grasp of how this all works.

[ar-stack-organization.svg]:               ../assets/ar-stack-organization.svg
[ar-stack-organization-conversion.svg]:    ../assets/ar-stack-organization-conversion.svg
[ten_fib.h]:                               ../../src/ten_fib.h
[ten_fib.c]:                               ../../src/ten_fib.c
