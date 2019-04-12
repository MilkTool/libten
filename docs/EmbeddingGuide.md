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
will be scheduled to activate when `memNeed > memUsed * (1.0 + memGrowth)`, where `memUsed` is the number of bytes allocated when the
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

Fiber's can't be reused once they're encountered an error, so while this will
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
