# Ten Embedding Guide
This guide is intended as a quick intro to the embedding API of the
Ten programming language.  Ten has been designed from the ground up
to be embedded into other applications, and has learned from the
mistakes of its predecessor Rig, to provide a nice user friendly
embedding API.

Ten's embedding API is declared in the `ten.h` header file, which
should be included in any embedding application.

## Ten State
One of the neat features of Ten, and one that makes it particularly
suitable as an embedded scripting language, is that it doesn't allocate
any global state.  All the data necessary for an instance of Ten to
run is put into a `ten_State` struct.  This means that multiple instances
of the Ten runtime can exist and be used at once, so long as a single
instance isn't accessed by multiple threads at the same time.

### Initialization
A `ten_State` instance can be created with:

    ten_State*
    ten_make( ten_Config* config, jmp_buf* errJmp );

This takes a `ten_Config` struct, which allows for customization of
the VM, and `errJmp` which specifies an error handler as a `longjmp()`
destination.  All options in the `ten_Config` struct are optional, so
a zero initialized struct or `NULL` can be passed for `config`.

The `errJmp` on the other hand is required, and a pre-initialized
(with `setjmp()`) long jump destination must be passed.  Ten will
jump to this location when an error occurs, so the destination code
should handle the error accordingly.  A typical setup will look
something like:

    // State and Config variables.
    ten_State* ten = NULL;
    ten_Config cfg = { .debug = true };

    // Error handler.
    jmp_buf errJmp;
    int sig = setjmp( errJmp );
    if( sig ) {

        // Error info.
        ten_ErrNum  err = ten_getErrNum( ten, NULL );
        char const* msg = ten_getErrStr( ten, NULL );

        // Print the error message.
        fprintf( stderr, "Error: %s\n", msg );

        // Traverse the stack trace, printing each.
        ten_Trace* tIt = ten_getTrace( ten, NULL );
        while( tIt ) {
            // The unit is either 'compiler' for a compilation error
            // or the tag of the fiber where the error ocured.
            char const* unit = "???";
            if( tIt->unit )
                unit = tIt->unit;

            // This is the name of the file in which the error ocured.
            char const* file = "???";
            if( tIt->file )
                file = tIt->file;

            fprintf(
                stderr,
                "  unit: %-10s line: %-4u file: %-20s\n",
                unit, tIt->line, file
            );
            tIt = tIt->next;
        }

        // Free the VM and exit.
        ten_free( ten );
        exit( 1 );
    }
    ten = ten_make( &cfg, &errJmp );

Though in this case we exit if any error occurs, which is the typical
behavior for a script interpreter; the `ten_State` instance is actually
still usable, unless `err == ten_ERR_FATAL`, in which case it should
be released immediately and not used again.

Though the required error handler may seem a bit awkward, it allows
us to make the assumption that any errors will be handled appropriately
in the rest of the code base, which keeps the API safe and efficient.

### Configuration
The `ten_Config` struct is defined as follows:

    typedef struct ten_Config {
        void*       udata;
        FreallocFun frealloc;

        bool debug;

        double memGrowth;
    } ten_Config;

The `frealloc` field specifies a memory management function, this
is basically an aggregation of `malloc()`, `realloc()`, and `free()`
and has the type:

    typedef void* (*FreallocFun)( void* udata,  void* old, size_t osz, size_t nsz );

Given `old != NULL && osz > 0` then if `nsz == 0` the function should
free `old`, otherwise it should realloc it from the block's old size
`osz` to its requested new size `nsz`.  If `old == NULL || osz == 0`
then a new block should be allocated with size `nsz`.  The default
implementation just uses the standard `realloc()` and `free()`
implementations:

    static void*
    frealloc( void* _, void* old, size_t osz, size_t nsz ) {
        if( nsz > 0 )
            return realloc( old, nsz );
        free( old );
        return NULL;
    }

If `NULL` is returned and `nsz > 0` then Ten assumes that there's no
more memory, and throws a `ten_ERR_FATAL`.

The `udata` field is just an arbitrary user pointer that'll be passed
to each call of the `frealloc` function; it allows the user to associate
some state with the allocator.

The `debug` option indicates if the Ten compiler should include debug
info (line number information and such) with the code it compiles, this
allows Ten to generate comprehensive stack traces that are useful for
debugging, but adds some memory overhead.

And `memGrowth` controls the GC, after each GC cycle the next cycle
will be scheduled to activate when `memNeed > memUsed * (1.0 + memGrowth)`,
where `memUsed` is the number of bytes allocated when the
GC cycled finished and `memNeed` is the number of bytes currently in use
plus the number of bytes needed by the next allocation.  So `memUsed`
basically just controls how quickly Ten's heap space wil grow.

### Finalization
Once a `ten_State` instance is no longer needed, pass it to:

    void
    ten_free( ten_State* s );

This will take care of releasing any resources.  Note that, to avoid
endless loops when `ten_free()` is called from within the error handler,
it will never propegate errors to the user, instead any errors that
occur in the finalization procedure will cause an `abort()` to terminate
the program.

## Ten Variables
One of the difficulties that arise when designing the API of an embedded
language is the consolidation of two very different type systems.  For
example Ten is dynamically types and garbage collected, but C is
statically typed with no GC.

Designing a reasonably safe and maintainable API to bridge this chasm is
no joke, but I've done my best to build something that allows for ABI
forward compatibility, is safe for the GC, and is relatively easy to use.

This system is based on API variables, which are a form of middle ground
between Ten's type system and C's.  The `ten_Var` struct is defined as:

    typedef struct {
        ten_Tup* tup;
        unsigned loc;
    } ten_Var;

And represents a slot in a Ten tuple, which is basically a group of
associated values.  The `ten_Tup` struct is actually defined internally,
but it's size is given in the API:

    typedef struct {
        char pri[32];
    } ten_Tup;

To allow tuples to be allocated on the native stack.  Tuples have
a specific lifetime, and it's up to the API user to make sure they
don't use a tuple whose lifetime has expired; otherwise the behavior
of API functions is undefined.

While it's Ten's job to create and format the `ten_Tup` tuples, the
user is responsible for initializing `gen_Var` variables.  Most tuples
are allocated on Ten's runtime stack with the `ten_pushA()` function:

    ten_Tup
    ten_pushA( ten_State* s, char const* pat, ... );

The `pat` string tells Ten how many variable slots to put in the tuple,
and which type of value they should be initialized with.  The
initialization values follow.  The `pat` string should contain zero
or more of the following characters:

    'U' - udf consumes no arguments
    'N' - nil consumes no arguments
    'L' - Log (boolean) value, consumes a `bool` or `int` argument
    'I' - Int (integral) value, consumes a `long` argument
    'D' - Dec (decimal) value, consumes a `double` argument
    'S' - Sym (symbol) value, consumes a `char const*` argument
    'P' - Ptr (pointer) value, consumes a `void*` argument
    'V' - Consumes a `ten_Var*` argument, copying its value to the tuple

The length of symbols added to the tuple at initialization is
determined by its null terminated argument string instead of an
explicit length argument.

Pointers added this way are tagless, so their Ten type name will
be `'Ptr'` instead of `'Ptr:SomeTag'`.

Since most API functions expect to be passed variables rather
than tuples, after allocating a tuple we typically assign variables
to each slot.  Here's how that looks:

    ten_Tup tup = ten_pushA( ten, "UIS", 123L, "abc" );
    ten_Var udfVar = { .tup = &tup, .loc = 0 };
    ten_Var intVar = { .tup = &tup, .loc = 1 };
    ten_Var symVar = { .tup = &tup, .loc = 2 };

The lifetime of a stack allocated tuple ends once a matching `ten_pop()`
call is made to pop the tuple off the stack, any further use after
this will result in undefined behavior.

    void
    ten_pop( ten_State* ten );

All tuples allocated in the body of a `ten_FunCb` (native Ten function)
will also be popped once the function returns; so we usually don't
have to worry about popping locally allocated tuples.


The `ten_pushV()` function can be used in place of `ten_pushA()`,
requiring a `va_list` in place of the variadic argument list.

## Compiling and Running Code
The easiest way to get started running Ten code is with the direct execution
functions; these compile and run the input code with one easy call.

    void
    ten_executeScript( ten_State* s, ten_Source* src, ten_ComScope scope );

    ten_Tup
    ten_executeExpr( ten_State* s, ten_Source* src, ten_ComScope scope );

The difference between the script and expression variants are that
`ten_executeExpr()` will only compile a single expression from the
input source, returning the result of evaluating the expression; while
`ten_executeScript()` will compile the whole input as a sequence of
delimiter (`,` or `\n`) separated expressions, and has no return.
The script compiler will also skip an [unicode BOM](https://en.wikipedia.org/wiki/Byte_order_mark)
and [Unix shebang](https://en.wikipedia.org/wiki/Shebang_(Unix)) if
present at the start of the input.

### Source Streams
All Ten compilation functions expect the input code to be in the form of a
`ten_Source*` source stream; this allows for a lot of flexibility regarding
where the code comes from and how it's formatted; but it also means we
have to do a bit of work beforehand to build a source stream.  Luckily the
API also provides a few convenience constructors for the most common
source code formats:

    ten_Source*
    ten_fileSource( ten_State* s, FILE* file, char const* name );

    ten_Source*
    ten_pathSource( ten_State* s, char const* path );

    ten_Source*
    ten_stringSource( ten_State* s, char const* string, char const* name );

The compilation function will automatically release the source stream if
an error occurs, or when it's finished compiling; so the stream finalizer:

    void
    ten_freeSource( ten_State* s, ten_Source* src );

Should only be called for streams that haven't been passed to a compilation
or execution function.

### Compilation Scopes
Compilation functions also take a `ten_ComScope` argument, which tells
Ten where the code should define variables to.  If the scope is given
as `ten_SCOPE_GLOBAL` then variables defined in the code are added to
the global variable pool; otherwise, if given as `ten_SCOPE_LOCAL` then
the variables are defined locally and only accessible within the same
script or expression.

Generally we want to use `ten_SCOPE_LOCAL` as local variables are more
efficient than globals, and we don't want to allow arbitrary scripts to
contaminate the global namespace.  The most obvious, and only one I can
think of, use case for global scoping is in the implementation of REPLs,
where each line is compiled separately but needs to have access to the
variables defined in previous lines.

### Execution Errors
Any errors encountered while executing the given input code will be
propagated directly to the user's error handler (the `errJmp`), unless
the code itself creates a fiber to execute failure prone code with,
in which case any but `ten_ERR_FATAL` errors will be isolated to the
fiber under whose execution they occur.  To prevent immediate errors
from propagating to the user's error handler, the code should be
compiled ahead of time to a fiber as described in the following
section, and executed manually with `ten_cont()`.

### Compiling Code
As an alternative to direct execution, we can compile code manually
into a fiber or closure before execution with:

    void
    ten_compileScript( ten_State* s, char const** upvals, ten_Source* src, ten_ComScope scope, ten_ComType out, ten_Var* dst );

    void
    ten_compileExpr( ten_State* s,  char const** upvals, ten_Source* src, ten_ComScope scope, ten_ComType out, ten_Var* dst );

These allow us to specify an `out` format as either `ten_COM_FIB` to compile
directly to a fiber object; or `ten_COM_CLS` to compile to a closure.  The
`dst` variable is where the resulting object is put; and we can specify a
`NULL` terminated list of upvalue names.  These are variables that are
accessible to the compiled code, but can also be accessed and modified
externally with:

    void
    ten_getUpvalue( ten_State* s, ten_Var* cls, unsigned upv, ten_Var* dst );

    void
    ten_setUpvalue( ten_State* s, ten_Var* cls, unsigned upv, ten_Var* src );

The upvalue list is optional, and can be passed as `NULL` to leave
any upvalues unspecified.

To execute (continue) a compiled closure we use:

    ten_Tup
    ten_cont( ten_State* s, ten_Var* fib, ten_Tup* args );

This expects a tuple of arguments, but the resulting fibers produced
by the above compilation routines don't have any parameters, so
an empty tuple should be passed.  Here's an example of the whole
process:

    // Construct a code source.
    ten_Source* src = ten_stringSource( ten, "show( \"Hello, World!\", N )", NULL );

    // Allocate a variable for the fiber.
    ten_Tup vars = ten_pushA( ten, "U" );
    ten_Var fibVar = { .tup = &vars, .loc = 0 };

    // Compile the script to a fiber.
    ten_compileScript( ten, NULL, src, ten_SCOPE_LOCAL, ten_COM_FIB, &fibVar );

    // Run the script.
    ten_Tup args = ten_pushA( ten, "" );
    ten_cont( ten, &fibVar, &args );

    // And pop `args` and `vars`.
    ten_pop( ten );
    ten_pop( ten );

Compiling and running the code manually like this keeps the errors
from propagating to the user's error handler, instead they're isolated
to the executing fiber, and can either be ignored or retrieved with
the error checking functions in the following section.


## Handling Errors
Ten's error checking routines are fairly straightforward:

    ten_ErrNum
    ten_getErrNum( ten_State* s, ten_Var* fib );

    void
    ten_getErrVal( ten_State* s, ten_Var* fib, ten_Var* dst );

    char const*
    ten_getErrStr( ten_State* s, ten_Var* fib );

    ten_Trace*
    ten_getTrace( ten_State* s, ten_Var* fib );

All four of these have `fib` parameter, which should either be
the fiber whose error we're interested in or `NULL` to check
for global errors.

The `ten_getErrNum()` function will return the error type
currently associated with the fiber, or global state, it'll
return one of:

    typedef enum {
        ten_ERR_NONE,
        ten_ERR_FATAL,
        ten_ERR_SYSTEM,
        ten_ERR_RECORD,
        ten_ERR_STRING,
        ten_ERR_FIBER,
        ten_ERR_CALL,
        ten_ERR_SYNTAX,
        ten_ERR_LIMIT,
        ten_ERR_COMPILE,
        ten_ERR_USER,
        ten_ERR_TYPE,
        ten_ERR_ARITH,
        ten_ERR_ASSIGN,
        ten_ERR_TUPLE,
        ten_ERR_PANIC
    } ten_ErrNum;

The two important ones are `ten_ERR_NONE`, which indicates that no
error occurred, and `ten_ERR_FATAL` which indicates that a memory
error or other fatal error occur which has rendered the Ten instance
as unusable.  Fatal errors will always propagate down to the user's
error handler, while any others will be isolated to the fiber in which
they occurred.

The `ten_errVal()` function can be used to retrieve the Ten value
associated with the current error; but in the case of a fatal
error it may return `udf`, since if a memory error occured then
allocating an error value may not be possibel; in which case a
`ten_errStr()` call will return a proper error message.

The `ten_errStr()` function will convert the current error value to
string form, or return a constant string error message if an error
value could not be allocated.  The lifetime of the string returned
by this function is only guaranteed until before the next API call.

The `ten_getTrace()` function returns the stack trace produced by
the error, this is a linked list with nodes of the form:

    typedef struct ten_Trace ten_Trace;
    struct ten_Trace {
        char const* unit;
        char const* file;
        unsigned    line;
        ten_Trace*  next;
    };

The fields of these nodes should never be modified or freed manually
be the user.  The lifetime of the trace itself is only guaranteed
until a call to `ten_clearError()` for the unit to which the trace
belongs (a fiber or the global trace).  The trace for a specific
fiber will also be freed when the fiber itself is garbage collected;
so the user must keep a reference to the fiber to keep its trace
from being released.

To clear the error, including its associated stack trace, of the
Ten instance or a fiber use:

    void
    ten_clearError( ten_State* s, ten_Var* fib );

Fiber's can't be reused once they've encountered an error, so while this will
clear the error, the fiber's state will remain as `ten_FIB_FAILED` and any
attempts at continuing the fiber will fail.

At times it may be useful to propagate the error from one fiber, to the
currently running fiber (or the global error handler if no fibers are
running).  This causes the error to be 're-thrown', to be handled by
the next unit in line.

    void
    ten_propError( ten_State* s, ten_Var* fib );

If no fiber is specified, then the current global error is re-thrown, this
is useful when used with:

    jmp_buf*
    ten_swapErrJmp( ten_State* s, jmp_buf* errJmp );

Which allows for the temporary replacement of the current global error
handler.  A useful pattern is something like:

    jmp_buf  jmp;
    jmp_buf* old = ten_swapErrJmp( ten, &jmp );
    if( setjmp( jmp ) ) {
      // Do some cleanup.
      ten_swapErrJmp( ten, old );
      ten_propError( ten, NULL );
    }

## Globals and Temporaries
Every Ten instance (`ten_State`) object has a pool of global variables
which can be accessed by any code compiled in the instance.  These
variables can also be directly accessed and manipulated by an API user
via the global variable API:

    void
    ten_def( ten_State* s, ten_Var* name, ten_Var* val );

    void
    ten_set( ten_State* s, ten_Var* name, ten_Var* val );

    void
    ten_get( ten_State* s, ten_Var* name, ten_Var* dst );

The globals are of course internal Ten variables, so they're represented
differently than the API variables to which users have access, so these
routines expect and API variable from/to which it can copy the value of
the global variable.

The semantics of each of these functions is the same as for their
respective language level forms.  Definitions can create new variables,
or undefined existing ones (by defining to `udf`).  Mutations (sets)
can only change the values of existing variables.  And references
(gets) will return `udf` if no variable of the given name exists.

The API expects a `ten_Var*` containing a symbol value for the global's
name; this is perhaps a bit inconvenient from a user perspective, since
string literals are much simpler to work with; but it allows for much
more efficient access when performance is critical since it allows us
to skip the symbol lookup overhead that'd otherwise be required to
create a symbol from a string literal.

To reduce the inconvenience of this and other elements of Ten's
interface, each runtime instance maintains a circular buffer of
temporary variables; which can be allocated and initialized with:

    ten_Var*
    ten_udf( ten_State* s );

    ten_Var*
    ten_nil( ten_State* s );

    ten_Var*
    ten_log( ten_State* s, bool log );

    ten_Var*
    ten_int( ten_State* s, long in );

    ten_Var*
    ten_dec( ten_State* s, double dec );

    ten_Var*
    ten_sym( ten_State* s, char const* sym );

    ten_Var*
    ten_ptr( ten_State* s, void* ptr );

    ten_Var*
    ten_str( ten_State* s, char const* str );

These give us the convenience of literals when desired by using a
temporary for some API argument; but still allows for a more efficient
approach when performance is more critical.

And example of how these might be used is something like:

    ten_def( ten, ten_sym( ten, "myVar" ), ten_int( ten, 123) );

    ten_Var* myVar = ten_udf( ten );
    ten_get( ten, ten_sym( ten, "myVar" ), myVar );
    long myVal = ten_getInt( ten, myVar );

**Remember**, these temporaries are in a circular buffer, so if too
many are allocated at once the same variable will be returned by
two different allocations.  The runtime instance has enough of these
(32 for now) that they're generally safe to use as quick arguments
or return destinations, just don't try to hold on to them for too
long; and don't count on an exact number of temporaries being available
as they might also be allocated internally.

## Singleton Values
Ten's singleton values have a fairly simple access API since there's no
need to 'retrieve' any value; the ability to type check and set values
is enough.  The full Udf API is:

    bool
    ten_isUdf( ten_State* s, ten_Var* var );

    bool
    ten_areUdf( ten_State* s, ten_Tup* tup );

    ten_Var*
    ten_udfType( ten_State* s );

    void
    ten_setUdf( ten_State* s, ten_Var* dst );

These have fairly obvious purposes save for `ten_udfType()`, which
returns a symbol variable containing the type name `'Udf'`, suitable
for use as the type for `ten_expect()`, which will be covered later.

The `ten_areUdf()` is also an outlier, but it just checks that all
values in a tuple are of type Udf.

The Nil interface is exactly the same, just for the Nil type:

    bool
    ten_isNil( ten_State* s, ten_Var* var );

    bool
    ten_areNil( ten_State* s, ten_Tup* tup );

    void
    ten_setNil( ten_State* s, ten_Var* dst );

    ten_Var*
    ten_nilType( ten_State* s );

## Atomic Values
Again, the interfaces for atomic (or primitive) values are fairly
consistent, only the type names and C types change for each function
to reflect the particular type we're dealing with:

    // Logical values.
    bool
    ten_isLog( ten_State* s, ten_Var* var );

    void
    ten_setLog( ten_State* s, bool log, ten_Var* dst );

    bool
    ten_getLog( ten_State* s, ten_Var* var );

    ten_Var*
    ten_logType( ten_State* s );

    // Integral values.
    bool
    ten_isInt( ten_State* s, ten_Var* var );

    void
    ten_setInt( ten_State* s, long in, ten_Var* dst );

    long
    ten_getInt( ten_State* s, ten_Var* var );

    ten_Var*
    ten_intType( ten_State* s );

    // Decimal values.
    bool
    ten_isDec( ten_State* s, ten_Var* var );

    void
    ten_setDec( ten_State* s, double dec, ten_Var* dst );

    double
    ten_getDec( ten_State* s, ten_Var* var );

    ten_Var*
    ten_decType( ten_State* s );

These are all pretty obvious, so I won't waste space explaining them.


## Symbols
The symbol interface has a lot in common with those already covered:

    bool
    ten_isSym( ten_State* s, ten_Var* var );

    void
    ten_setSym( ten_State* s, char const* sym, size_t len, ten_Var* dst );

    ten_Var*
    ten_symType( ten_State* s );


But how we have two getters, one for each aspect of the symbol: length and content:

    char const*
    ten_getSymBuf( ten_State* s, ten_Var* var );

    size_t
    ten_getSymLen( ten_State* s, ten_Var* var );

**It's important** to note that the lifetime for the string returned by
`ten_getSymBuf()` is only guaranteed to last until the next API call,
so the string should be copied for extended use.

## Pointers
Pointers are an interesting facet of Ten's design.  Like symbols, they're
something in between atomic (primitive) values and heap allocated ones since
they can be compared directly; but also have some data on the heap.

Though Lua and similar languages also support some semblance of a pointer
type; Ten is unique in that it allows the pointer type to be preserved
as a part of the value.  This is done by associating each pointer with a
`ten_PtrInfo`, which keeps track of the pointer's type and destructor.

A `ten_PtrInfo` struct can be allocated with:

    ten_PtrInfo*
    ten_addPtrInfo( ten_State* s, ten_PtrConfig* config );

And will be freed with the Ten instance.  This expects a pointer config
struct:

    typedef struct {

        char const* tag;

        void (*destr)( ten_State* s, void* addr );
    } ten_PtrConfig;


The `tag` field should give the type of the pointer, or whatever it
should be called within Ten; and the destructor gives a function to
be called once all instances of the pointer (same `ten_PtrInfo` and
address) have been garbage collected.

The pointer setter takes an optional `ten_PtrInfo` for type information,
if none is provided then the variable is set to a generic pointer with
no additional type information.  Pointer with no type tag have a type
name of `'Ptr'` while those with a tag have `'Ptr:SomeTag'`.  This allows
for more precise type checking when passing around pointer, a very
dangerous type of value.

    void
    ten_setPtr( ten_State* s, void* addr, ten_PtrInfo* info, ten_Var* dst );

The type related functions should be familiar from the other types
we've covered:

    bool
    ten_isPtr( ten_State* s, ten_Var* var, ten_DatInfo* info );

    ten_Var*
    ten_ptrType( ten_State* s, ten_PtrInfo* info );

Only now we have an additional parameter.  The `info` parameter is
optional (can be NULL), if provided then `ten_isPtr()` will only
return true if `var` has type Ptr, and it's associated with the
given `info`.  A call to `ten_ptrType()` will return the generic
pointer type name `'Ptr'` instead of a tagged name.

And again we have two getters, one for each aspect of the pointer:

    void*
    ten_getPtrAddr( ten_State* s, ten_Var* var );

    ten_PtrInfo*
    ten_getPtrInfo( ten_State* s, ten_Var* var );


## Strings
The string interface is basically the same as for symbols:

    bool
    ten_isStr( ten_State* s, ten_Var* var );

    void
    ten_newStr( ten_State* s, char const* str, size_t len, ten_Var* dst );

    char const*
    ten_getStrBuf( ten_State* s, ten_Var* var );

    size_t
    ten_getStrLen( ten_State* s, ten_Var* var );

    ten_Var*
    ten_strType( ten_State* s );


The main difference is that we use `ten_newStr()` instead of `ten_setStr()`
since strings are heap allocated objects.

Another significant difference is that the pointer returned by `ten_getStrBuf()`
is guaranteed to be valid so long as the string is alive; so it'll be safe to
use as long as the user maintains a reference to the string in some variable.

## Indices and Records
The Idx (or Index) type is one of Ten's types that we'll rarely, if ever,
see from within the Ten language, but is important from an embedder's
perspective.  Ten's record system is designed to be very efficient with
memory, especially when a lot of different record instances are created
with the same, or similar, structure.

The language achieves this efficiency by allowing groups of records,
which are expected to maintain a similar structure (or set of keys),
to share a single lookup table, dubbed an index.  And this is exactly
what the Idx type represents, and index which can be shared between
records.

![Record Design](RecordDesign.png)

The proper sharing of indexes between similarly structured records will
generally result in very significant memory savings; however their
misuse, by using the same index for records which will not maintain
a similar set of keys; causes each record to allocate value slots for
keys that they'll never need, causing an imbalance, and wasting memory.

![Record Unbalanced](RecordUnbalanced.png)

Normal Ten code handles this mapping of index to records automatically
by assigning an index to each record constructor; so every record
constructed at this point will share the same index.  This is usually
a good strategy, but not always, so the prelude provides the `sep()`
function to separate a record from its current index, creating a new
index for the record instead.

But embedders have more control, and with that, more responsibility.  It's
the job of the API user to decide which index should be associated with
each record they create; and this decision can have a significant impact
on the performance of your software as a whole.

For indexes we have a quite simple interface, since their only purpose
is to be passed to record constructors.

    bool
    ten_isIdx( ten_State* s, ten_Var* var );

    void
    ten_newIdx( ten_State* s, ten_Var* dst );

    ten_Var*
    ten_idxType( ten_State* s );


These all have the obvious meanings with respect to those functions
already covered thus far.  Records on the other hand are a bit more
complex:

    bool
    ten_isRec( ten_State* s, ten_Var* var );

    void
    ten_newRec( ten_State* s, ten_Var* idx, ten_Var* dst );

    void
    ten_recSep( ten_State* s, ten_Var* rec );

    void
    ten_recDef( ten_State* s, ten_Var* rec, ten_Var* key, ten_Var* val );

    void
    ten_recSet( ten_State* s, ten_Var* rec, ten_Var* key, ten_Var* val );

    void
    ten_recGet( ten_State* s, ten_Var* rec, ten_Var* key, ten_Var* dst );

    ten_Var*
    ten_recType( ten_State* s );


We have the usual constructor and type related functions.  But also a
set of functions for setting and getting record fields; these have
the same semantics (and use the same code) and their respecitve language
level forms.  So definitions can add or remove (by defining to `udf`),
while mutations (sets) can only modify existing fields.  References
(gets) will return `udf` if the field doesn't exist.

The functions for allocating temporary values discussed in
[Globals and Temporaries](#globals-and-temporaries) are equally
convenient for dealing with record fields, so long as performance
isn't a major concern.

    ten_recDef( ten, &recVer, ten_sym( ten, "someKey" ), ten_int( ten, 123 ) );

However for more performance critical tasks, or those which will be
performed fairly often, it's much more efficient to keep the most
often used record keys (at least the symbols) in persistent variables
to avoid the symbol lookup overhead needed to create new symbol
values.  The cleanest way to do this is to put the symbols in member
slots of a Dat (user data) object.  This strategy will be discussed
further in the [Data Objects](#data-objects) section.

## Functions and Closures
One essential feature of embedded languages is the ability to call
native functions from within the virtual environment.  Ten is quite
good at this, with hardly any distinction between native and virtual
closures after their creation.

Ten's functions and closures share a relationship similar to that of 
indices and records.  Functions represent the actual code to be
executed, along with any constants and other immutable aspects
of the pair while closures are the actual callable objects, which 
combine an immutable function with some mutable state, either
in the form of upvalue variables or user data.  A like indices,
functions can be shared between multiple closures instances.

The function relatd API should look familiar:

	bool
	ten_isFun( ten_State* s, ten_Var* var );

	void
	ten_newFun( ten_State* s, ten_FunParams* p, ten_Var* dst );

	ten_Var*
	ten_funType( ten_State* s );

Function creation, however, requires quite a few parameters, most of
them optional. We put these in a seperate struct to avoid an overly
long parameter list for the constructor.

    typedef struct {
        char const*  name;
        
        char const** params;
        
        ten_FunCb cb;
    } ten_FunParams;


The `name` field give the function name Ten should use when refering
to the function in error reports and such.  This is optional and can
be given as `NULL`, but providing a name can make things a lot easier
in when it comes time to debug your code.

The `params` array should give a list of parameter names, used for
the same purpose as the function name; only this is required since
it'll also be used to determine the number of parameters expected
by the function.  The last name can optionally be followed by an
ellipsis `...` to indicate that it's a variadic parameter; the
semantics of these are the same as for virtual functions, varidadic
arguments will be put into a record, which will be passed as an
argument in the place of the variadic parameter.  If provided,
this list should be `NULL` terminated.

The `cb` is the actual native function that'll be called when an
associated closure is invoked in the VM.  This has a signature:

    typedef ten_Tup (*ten_FunCb)( ten_PARAMS );

Where `ten_PARAMS` is a macro:

    #define ten_PARAMS ten_State* ten, ten_Tup* args, ten_Tup* mems, void* dat

The parameter list is put in this macro to make it easier for users
to define matching functions.  Just use this macro in place of a
full parameter list, for example:

    ten_Tup
    fooFun( ten_PARAMS ) {
        ... 
    }

Since the parameter list is rather long, this also keeps things a bit
more tidy.  The typical function creation process will look something
like:

        ten_FunParams p = {
            .name   = "foo",
            .params = (char const*[]){ "a", "b", NULL },
            .cb     = fooFun
        };
        ten_newFun( ten, &p, &funVar );


Native function callbacks are passed quite a bit of data, but
we can often ignore the last two parameters since they'll be
passed as `NULL` if we haven't associated a Dat object with
the closure.

The `ten` parameter is of course passed as the Ten instance,
and the `args` tuple gives a tuple of arguments passed by
the caller; it'll contain the same number of slots as the
number of names given in the `p.params` list.  A typical
native callback function would do something like:

    ten_Tup
    fooFun( ten_PARAMS ) {
        ten_Var aArg = { .tup = args, .loc = 0 };
        ten_Var bArg = { .tup = args, .loc = 1 };
        ...
    }

The tuple returned by this callback should be contain the
return values for the function; and can be allocated on
the runtime stack or elsewhere (in other words any live
tuple will work).

Once we've built a function, creating a closure is pretty simple:

    ten_newCls( ten, &funVar, NULL, &datVar );

The closure portion of Ten's API has the following:

    bool
    ten_isCls( ten_State* s, ten_Var* var );

    void
    ten_newCls( ten_State* s, ten_Var* fun, ten_Var* dat, ten_Var* dst );

    void
    ten_getUpvalue( ten_State* s, ten_Var* cls, unsigned upv, ten_Var* dst );

    void
    ten_setUpvalue( ten_State* s, ten_Var* cls, unsigned upv, ten_Var* src );

    ten_Var*
    ten_clsType( ten_State* s );


For now we ignore the optional `dat` parameter for the closure
constructor, passing `NULL` in its place.  This allows us to
associate a Dat object with the closure to keep some type of
user state, but it'll be discussed later.

The `getUpvalue()` and `setUpvalue()` functions are only useful for
compiled closures as they allow us to get and set the values of
the closure's free variables registered at compile time.

After construction, a native closure can be called just like any
Ten closure.

## Data Objects
Data objects are objects allocated within the Ten runtime, for the
purpose of containing native state which can't otherwise be
represented with its simple type system.

The memory of one of these objects is allocated and freed by Ten,
but the API user is allowed to freely read and write to it.  In
additional to the buffer of native memory, Dat objects can also
maintain a number of 'member' variables, in which the user can
store related Ten values.

Like pointer objects discussed earlier, data objects have an
associated `ten_DatInfo` instance to tell Ten how these objects
should be used.  But unlike pointers for which this info is
optional, teh `ten_DatInfo` struct is required for the data
constructor.

    bool
    ten_isDat( ten_State* s, ten_Var* var, ten_DatInfo* info );

    void*
    ten_newDat( ten_State* s, ten_DatInfo* info, ten_Var* dst );

    void
    ten_setMember( ten_State* s, ten_Var* dat, unsigned mem, ten_Var* val );

    void
    ten_getMember( ten_State* s, ten_Var* dat, unsigned mem, ten_Var* dst );

    ten_Tup
    ten_getMembers( ten_State* s, ten_Var* dat );

    ten_DatInfo*
    ten_getDatInfo( ten_State* s, ten_Var* dat );

    void*
    ten_getDatBuf( ten_State* s, ten_Var* dat );

    ten_DatInfo*
    ten_addDatInfo( ten_State* s, ten_DatConfig* config );

    ten_Var*
    ten_datType( ten_State* s, ten_DatInfo* info );


By now we've covered look-alikes for most of these functions, and
the behaviors are similar enough that youre intuition about should
be accurate.

The member accessors, however, deserve special note.  These serve
the same purpose as the upvalue accessors for closures; they copy
the values from/to the object's internal member variables.
Alternatively these variables can be accessed by retrieving the
whole tuple of members via `ten_getMembers()` and manually associating
API variables with each.

The `ten_getDatBuf()` accessor returns a pointer to the object's
main data buffer; which is where the user is expexted to keep their
state.

As with pointers, before creating a data object we need to register
a `ten_DatInfo` to tell Ten _how_ to create it.  The config for this
looks like:

    typedef struct {
        char const* tag;
        
        unsigned size;
        unsigned mems;
        
        void (*destr)( ten_State* s, void* buf );
    } ten_DatConfig;

With the `tag` giving the type tag for object created with this
info, the full type name will be `'Dat:SomeTag'` unless the tag
is given as `NULL`, in which case it'll just be `'Dat'`.

The `size` tells Ten how large to make the data buffer, and `mems`
says how many member variables to allocate for each instance of
the object.

And `destr` provides an optional destructor function to be called
for any cleanup before the GC claims the object's memory.

Data objects can be associated with closures by passing their
variable as the `dat` parameter of the closure constructor.  This
causes the object's memory buffer to be passed as the `dat`
parameter to the native callback, and its member tuple to be
passed as `mems`.

    #define ten_PARAMS ten_State* ten, ten_Tup* args, ten_Tup* mems, void* dat

Given this ability to link the two object types, data objects
become even more useful as places to allocate upvalues (as members)
for native functions or to put other needed state.  A typical
pattern is to put any symbols, used for accessing record fields
in a closure, as members of its associated data object to allow for
efficient access.  For example:

    typedef enum {
        FOO_MEM_car,
        FOO_MEM_cdr,
        FOO_MEM_tag,
        FOO_MEM_LAST
    } FooMems;
    
    typedef struct {
        int fooVar1;
        int fooVar2;
    } FooData;
    
    ...
    
    ten_Tup
    fooFun( ten_PARAMS ) {
        ten_Var carMem = { .tup = mems, .loc = FOO_MEM_CAR };
        ...
        ten_recGet( ten, &recVar, &carMem, &dstVar );
        ...
    }
    
    ...
    
    ten_DatInfo* fooInfo = ten_addDatInfo(
        ten,
        &(ten_DatConfig){
            .tag   = "FooData",
            .size  = sizeof(FooData),
            .mems  = FOO_LAST,
            .destr = NULL
        }
    );
    ten_newDat( ten, fooInfo, &datVar );


An alternative approach, though less memory efficient, is to
put API variables that for referencing members in the data
struct itself:

    typedef struct {
        ten_Tup mems;       // = ten_getMembers( ten, &datVar );
        ten_Var carMem;     // = { .tup = &mems, .loc = 0 };
        ten_Var cdrMem;     // = { .tup = &mems, .loc = 1 };
        ten_Var tagMem;     // = { .tup = &mems, .loc = 2 };
    } FooData;

## Fibers
The fiber portion of Ten's API consits of things we've already
seen:

    bool
    ten_isFib( ten_State* s, ten_Var* var );

    void
    ten_newFib( ten_State* s, ten_Var* cls, ten_Var* tag, ten_Var* dst );

    ten_Var*
    ten_fibType( ten_State* s );

And utilities that resemble, and have the same functionality,
as their counterpart prelude functions.

    ten_FibState
    ten_state( ten_State* s, ten_Var* fib );

    ten_Tup
    ten_cont( ten_State* s, ten_Var* fib, ten_Tup* args );

    void
    ten_yield( ten_State* s, ten_Tup* vals );

Though there are a few things worth noting.

The `ten_FibState` value returned as a fiber state can be one
of the following:

    typedef enum {
        ten_FIB_RUNNING,
        ten_FIB_WAITING,
        ten_FIB_STOPPED,
        ten_FIB_FINISHED,
        ten_FIB_FAILED
    } ten_FibState;

With the same meanings as their symbol counterparts as returned
by the `state()` prelude function.

The `ten_yield()` function doesn't work exactly like the prelude
funtion in that the next continuation will cannot restore the
state of the native callback from which this will be called;
so a fiber continueation after this call will continue from the
last virtual function on the stack, using the continuation
arguments as the native function's return values.  This function
can only be called from within a native callback, as doing so
outside of a running fiber makes no sense.

## Type Checking
Ten's embedding API provides two main functions, aside from the
type specific ones that have already been covered, for checking
the type in a given variable:

    void
    ten_type( ten_State* s, ten_Var* var, ten_Var* dst );

    void
    ten_expect( ten_State* s, char const* what, ten_Var* type, ten_Var* var );

These are equivalent to the prelude's `type()` and `expect()`
functions.  The `ten_type()` will load `dst` with the type name
symbol of the given `var` while `ten_expect()` will throw an
error if `var`'s type name does not match the given `type`.  The
`what` parameter here tells the function what to report as having
the wrong type in its error message.

    ten_Var numArg = { .tup = args, .loc = 0 };
    ten_expect( ten, "num", ten_intType( ten ), &numArg );



## Misc
By now we've covered the bulk of Ten's embedding API, the only thing
left is to discuss a few miscelanious functions not related to
any of the other aspects of the interface.

    #define ten_call( S, CLS, ARGS ) \
        ten_call_( S, CLS, ARGS, __FILE__, __LINE__ )
        
    ten_Tup
    ten_call_( ten_State* s, ten_Var* cls, ten_Tup* args, char const* file, unsigned line );

This is used to call back into the Ten instance from a native callback;
since it needs a running fiber to make the call on, it's illegal to
call it outside of a native function call.  The actual function
`ten_call_()` has a few parameters for telling Ten where the call was
made from (file and line number) to allow Ten to produce comprehensive
stack trace entries for these calls; but passing these manually can
be tedius, so the `ten_call()` macro is provided to automatically pass
the current filename and line number.  The only time it might be
useful to manually call `ten_call_()` might be when creating API
bindings for other languages which may use a different, or none at all,
macro system.

The `ten_panic()` function allows the user to throw errors into the
Ten instance:

    void
    ten_panic( ten_State* s, ten_Var* val );

The given `val` will be used as an error value, can can be any
valid Ten value; though it'll generally be a string.


The `ten_dup()` function copies the given tuple to the top of
the stack:

    ten_Tup
    ten_dup( ten_State* s, ten_Tup* tup );

And `ten_size()` returns the number of value slots in a tuple:

    unsigned
    ten_size( ten_State* state, ten_Tup* tup );

This one is equivalent to the prelude's `loader()` function,
used to register an import strategy for the instance.

    void
    ten_loader( ten_State* s, ten_Var* type, ten_Var* loadr, ten_Var* trans );

Compare two Ten values:

    bool
    ten_equal( ten_State* s, ten_Var* var1, ten_Var* var2 );

And copy a value from one variable to another:

    void
    ten_copy( ten_State* s, ten_Var* src, ten_Var* dst );

The `ten_string()` function is mostly useful in implementing
REPLs, it stringifies the values of the given tuple, preserving
quotes for strings and symbols; it's suitable for printing
the result of a REPL expression, but not much else.

    char const*
    ten_string( ten_State* s, ten_Tup* tup );

## That's All Peeps
This guide is a bit messy for the time being since I have a limited
amount of time to contribute and wanted to cover the whole API since
a reference hasn't yet been published.  A lot of the API functions
are used in the same way as their couterparts in Ten's prelude, so
a better idea about them can be gained by scanning through the
[language guide](LanguageGuide.md), though I'm afraid it's in no
better state than this guide for now.
