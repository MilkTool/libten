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

## Accessing Values
Ten leaves it to the user to make sure a variable has the appropriate
type before attempting to get its value.  In a debug build Ten will
assert the type of a variable before using it for a particular purpose;
but this adds extra overhead to the API, so release builds assume that
the user knows what they're doing.  Thus passing a variable with one
type of Ten value, to an API routine that expects another type will
result in undefined behavior in release builds of the library, so
be careful.

### Type Checking
For each Ten value type the API provides a `ten_is*()` function,
where `*` is the name of the type, for checking if a variable
contains a value of that type.

An example usage for these is:

    long v = 0;
    if( ten_isInt( ten, &intVar ) )
      v = ten_getInt( ten, &intVar );

In addition, the `ten_type()` function give the type name of a
the value in a variable, as a symbol loaded into the `dst` variable:

    void
    ten_type( ten_State* s, ten_Var* var, ten_Var* dst );

And `ten_expect()` confirms that the type of the variable is what's
expected, or throws an error with an appropriate message:

    void
    ten_expect( ten_State* s, char const* what, ten_Var* type, ten_Var* var );

Here the `what` argument should be string description of what's being
type checked, the `type` variable should contain a type name symbol,
and `var` is the variable to be type checked.

These two routines are equivalent to the `type()` and `expect()` builtin
Ten functions, and in fact they use the same code behind the scene.

Every type also has a respective `ten_*Type()` function, where `*` is
the lower case type name, which returns a symbol variable suitable
to pass to `ten_expect()` as a type name of the particular type, these
variables should never be passed to a mutating API call.  Most of these
functions have use the same signature:

    ten_Var*
    ten_*Type( ten_State* s );

The exceptions are `ten_ptrType()` and `ten_datType()`, which accept
an additional `ten_PtrInfo` or `ten_DatInfo` respectively.  These will
be covered in more detail in the section on native functions and objects.


The singleton types `nil` and `udf` also have functions for checking
the types of all values of a tuple.

    bool
    ten_areUdf( ten_State* s, ten_Tup* tup );

    bool
    ten_areNil( ten_State* s, ten_Tup* tup );

These will return true only if all values in the tuple have the
respective singleton value.

### Getting Values
Since the singleton types `udf` and `nil` can only have one possible
value, providing accessor functions for these would be redundant; so
the API omits them. Otherwise Ten's atomic (primitive) types have
accessor functions of the form `ten_get*()` where `*` is the name
of the type.

    bool
    ten_getLog( ten_State* s, ten_Var* var );

    long
    ten_getInt( ten_State* s, ten_Var* var );

    double
    ten_getDec( ten_State* s, ten_Var* var );

The Sym (symbol) and Ptr (pointer) types are something in between atomic
and heap allocated types; so they have specialized accessors for each
aspect.  Symbols have the following:

    char const*
    ten_getSymBuf( ten_State* s, ten_Var* var );

    size_t
    ten_getSymLen( ten_State* s, ten_Var* var );

Which return the buffer (content) and length of a symbol value, the
lifetime of the string returned by `ten_getSymBuf()` is only guaranteed
until before the next API call; so it should be copied if needed further.

The pointer type has accessors for the pointer's address, and the
`ten_PtrInfo` associated with the value, which may be `NULL` if none
was given when the pointer was created.

    void*
    ten_getPtrAddr( ten_State* s, ten_Var* var );

    ten_PtrInfo*
    ten_getPtrInfo( ten_State* s, ten_Var* var );


Of all Ten's heap allocated object types, only strings can be converted
directly into the respective C type; their accessors are similar to
those for symbols:

    char const*
    ten_getStrBuf( ten_State* s, ten_Var* var );

    size_t
    ten_getStrLen( ten_State* s, ten_Var* var );


## Globals and Temporaries
Each `ten_State` maintains a store of global variables which are
accessible anywhere in a Ten program.  These can also be directly
accessed and manipulated by the API user with:

    void
    ten_def( ten_State* s, ten_Var* name, ten_Var* val );

    void
    ten_set( ten_State* s, ten_Var* name, ten_Var* val );

    void
    ten_get( ten_State* s, ten_Var* name, ten_Var* dst );

These expect a symbol variable for their `name` parameter, but
created such a variable for each use case can be tedius; so this
is where Ten's temporary buffers come in handy.  Internally Ten
allocates a static circular buffer of variables.  These can be
used for quickly converting between C values and Ten variables
to passed to an API routine, but shouldn't be kept for prolonged
use as their values will be replaced when the ring buffer circles
back.  Temporary variables can be allocated and initialized with:

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


These make using the global variable API much more convenient, for
example we can say:

    ten_set( ten, ten_sym( ten, "varName" ), ten_int( ten, 123 ) );

    ten_Var* val = ten_udf( ten );
    ten_get( ten, ten_sym( ten, "varName" ), val );

    int i = ten_getInt( ten, val );

Just make sure not to overuse the temporaries, though there will be
enough of them available (currently 32) to make a ring buffer overflow
unlikely so long as they're only used for arguments and the occasional
return destination; though they also may be allocated internally, so
don't rely on an exact count.
