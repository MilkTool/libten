# <a name="5">5 - The API</a>
This section describes Ten's C API, which is the set of exposed
types and functions that can be used by a host program, or native
module, to interact with the Ten runtime.

The full `libten` interface, `libten` is the library that implements
the Ten language, is declared in `ten.h`.  The library maintains no
mutable global state, so multiple instances of the Ten runtime can
be used simultaneously in a single process, so long as only a single
thread maintains control over each instance.

## <a name="5.1">5.1 - Ten State</a>
Ten doesn't allocate any mutable global state, so each runtime instance
is completely independent.  A Ten instance is represented by an opaque
`ten_State` pointer, which will be listed as the first parameter for
every API function except `ten_make()`, which creates a new such
instance.

    ten_State*
    ten_make( ten_Config* config, jmp_buf* errJmp );

The `config` can be passed as `NULL` to use the default setup, which
is reasonable for most use cases.  See [ten_Config](#type-ten_Config)
for details on what can be configured.  The `errJmp` parameter should
be passed as a valid long jump destination, which should be equipped
for error handling since this is where Ten will jump whenever a
global error occurs.

    ten_State* volatile ten;
    jmp_buf             jmp;
    if( setjmp( jmp ) ) {
        fprintf( stderr, "Error\n" );
        exit( 1 );
    }
    ten = ten_make( NULL, &jmp );

Once we're done with a Ten instance it should be released with:

    ten_free( ten );

## <a name="5.2">5.2 - Variables</a>
Ten uses API variables (`ten_Var`) as an abstraction over Ten's internal
architecture, to allow Ten values to be stored, accessed, and modified
via C code.

    typedef struct {
        ten_Tup* tup;
        unsigned loc;
    } ten_Var;

These variables themselves are little more than references to a specific
slot in a `ten_Tup`, which is where Ten puts the actual state used to
keep track of where the underlying values are located, and how many
slots are allocated.  The `ten_Tup` type represents and internal array
of Ten values which can be relocated internally without breaking the
API.  Since most API functions work with variables instead of tuples,
we'll usually create variables alongside each tuple allocation.

    ten_Tup tup = ten_pushA( ten, "UUU" );
    ten_Var var1 = { .tup = &tup, .loc = 0 };
    ten_Var var2 = { .tup = &tup, .loc = 1 };
    ten_Var var3 = { .tup = &tup, .loc = 2 };

The size of a specific tuple can be obtained with:

    unsigned
    ten_size( ten_State* ten, ten_Tup* tup );

### <a name="5.2.1">5.2.1 - Stacked Variables</a>
Most `ten_Tup` instances will refer to stack allocated values, that
is, the tuple refers to a section of the current runtime stack.  Since
Ten is inherently a multi-threaded (multi-fibered?) language, there is
no single execution stack.  Instead each Ten instance maintains a global
stack where C code can allocate variables when no fiber is currently
running; but whenever a Ten fiber _is_ running, stack allocations are
made to the running fiber's stack instead of this global one.  What
this means is that stack allocations made from within native function
calls will be allocated on a specific fiber's stack, but those made
from the root level C code, outside of any calls into the Ten runtime,
will be allocated on the global stack.

The most straightforward way to allocate a tuple on the current stack
is with:

    ten_Tup
    ten_pushA( ten_State* ten, char const* pat, ... );

This expects a `pat`tern string consisting of zero or more of the
following:

|  Letter  |      C Type            | Ten Type |
|:--------:|:----------------------:|:--------:|
|   `U`    |              -         |   `Udf`  |
|   `N`    |              -         |   `Nil`  |
|   `L`    | `_Bool`, `bool`, `int` |   `Log`  |
|   `I`    |            long        |   `Int`  |
|   `D`    |            double      |   `Dec`  |
|   `S`    |        char const*     |   `Sym`  |
|   `P`    |            void*       |   `Ptr`  |
|   `V`    |          ten_Var*      |     ?    |

The pattern tells the function how many slots to allocate in the array,
and which types to initialize them as; and the following variadic
arguments should provide initialization values for each respective
letter, save for those indicating a singleton value.  The `V` letter
indicates that the initialization value should be copied from the
given `ten_Var*`.  Symbols `S` created in this way can't specify an
arbitrary length for the symbol, it'll be computed with `strlen()`,
and pointers can't specify a `ten_PtrInfo` so will be untagged.

Tuples allocated with this function, and stack allocated tuples
in general, can only be used until a respective `ten_pop()` call
is made, to remove it from the stack.  Stack allocations made
within a native function callback will be popped automatically after
the function's return.  Any use of tuples, or associated variables,
that have already been popped from the stack will result in undefined
behavior; Ten can't catch this mistake, even in debug mode, so extra
care should be taken.

### <a name="5.2.2">5.2.2 - Temporary Variables</a>
It's often the case that we just need an easy way to pass C values
off as Ten values; without going through the tedius process of
allocating a tuple and initializing variables.  This can be achieved
through the Ten API's temporary variables.  Each runtime instance
allocates a certain number (32 in the current iteration) of API
variables internally, to be used as a quick medium of transferring
C values into the Ten universe.  Such temporaries can be allocated
and initialized with C values via:

    ten_Var*
    ten_udf( ten_State* ten );

    ten_Var*
    ten_nil( ten_State* ten );

    ten_Var*
    ten_log( ten_State* ten, bool log );

    ten_Var*
    ten_int( ten_State* ten, long in );

    ten_Var*
    ten_dec( ten_State* ten, double dec );

    ten_Var*
    ten_sym( ten_State* ten, char const* sym );

    ten_Var*
    ten_ptr( ten_State* ten, void* ptr );

    ten_Var*
    ten_str( ten_State* ten, char const* str );

The given C values will automatically be converted to the equivalent
Ten value type, which is put in the returned variable.

The temporaries returned by these functions are in a circular queue,
meaning that if too many of them are allocated for simultaneous use,
some of the returned pointers may refer to the same variable.  In
general it's best to use these temporaries only as quick parameters
to a single API call.

### <a name="5.2.3">5.2.3 - Accessing Values</a>
The values stored within Ten's API variables can be accessed as C
types via the type's respective accessor functions; but not all Ten
value types can be trivially represented in terms of C's type system.

Since the singleton types have only one possible value, it's enough
to just check if a given variable has a value of that type; without
a respective C type.

    bool
    ten_isUdf( ten_State* ten, ten_Var* var );

    bool
    ten_isNil( ten_State* ten, ten_Var* var );

And for primitive C types we have:

    bool
    ten_getLog( ten_State* ten, ten_Var* var );

    long
    ten_getInt( ten_State* ten, ten_Var* var );

    double
    ten_getDec( ten_State* ten, ten_Var* var );

Since symbols, strings, pointers, and data objects have more
than a single aspect, they each provide multiple accessors for
each aspect of the value:

    char const*
    ten_getSymBuf( ten_State* ten, ten_Var* var );

    size_t
    ten_getSymLen( ten_State* ten, ten_Var* var );

    char const*
    ten_getStrBuf( ten_State* ten, ten_Var* var );

    size_t
    ten_getStrLen( ten_State* ten, ten_Var* var );

    void*
    ten_getPtrAddr( ten_State* ten, ten_Var* var );

    ten_PtrInfo*
    ten_getPtrInfo( ten_State* ten, ten_Var* var );

    ten_DatInfo*
    ten_getDatInfo( ten_State* ten, ten_Var* dat );

    void*
    ten_getDatBuf( ten_State* ten, ten_Var* dat );


Since Ten can represent short symbols in terms of int values, which
can be stored directly in a value for efficiency, instead of on the
heap; the API can only guarantee that the string returned by
`ten_getSymBuf()` will be valid until before the next API call, so
it it's needed beyond that point it'll have to be copied before then.
The memory buffers returned by the other accessors are tied to the
lifetime of their associated objects; so are only guaranteed to remain
valid so long as the object isn't garbage collected.

It's important to note that Ten _assumes_ that the variables passed
to these accessors, and other API functions, contain values of the
correct types; it's up to the user to ensure this is the case, otherwise
unexpected things can happen.  When built in debug mode Ten will
`assert` that the given variables contain the correct types, aborting
the program otherwise.  But in release mode there is not such safety
check.  Binary distributions of `libten` ship with a debug build of
the library names as `libten-debug`, which should generally be used in
place of `libten` during development and testing.

### <a name="5.2.4">5.2.4 - Changing Values</a>
The values of existing variables can be changed via a combination of
the `ten_copy()` function, which copies the value of one variable to
another; and the temporary variable initializers outlined previously.

    void
    ten_copy( ten_State* ten, ten_Var* src, ten_Var* dst );

For example:

    ten_copy( ten, ten_dec( ten, 1.2 ), &dstVar );

But the temporary value initializers are designed to be easy
and convenient, so don't accept the full set of parameters for
initializing symbols and pointers; so we have special functions
for doing that:

    void
    ten_setSym( ten_State* ten, char const* sym, size_t len, ten_Var* dst );

    void
    ten_setPtr( ten_State* ten, void* addr, ten_PtrInfo* info, ten_Var* dst );

Since strings and datas are objects rather than atomic Ten types,
variables can't just be 'set' to such values, the values need to
be created then put into a variable.  So we use the `new` verb
for these and other object constructors in place of the `set` used
for symbols and pointers.

    void
    ten_newStr( ten_State* ten, char const* str, size_t len, ten_Var* dst );

    void*
    ten_newDat( ten_State* ten, ten_DatInfo* info, ten_Var* dst );

## <a name="5.3">5.3 - Executing Code</a>
The easiest way to execute a chunk of Ten code is via the convenience
functions:

    void
    ten_executeScript( ten_State* ten, ten_Source* src, ten_ComScope scope );

    ten_Tup
    ten_executeExpr( ten_State* ten, ten_Source* src, ten_ComScope scope );

These allow us to skip most of the tedious process of compiling the
code, wrapping it in a fiber, then _finally_ executing it; when all
we ever really wanted was the _execute_ step.

Both these and their counterpart functions for compiling code expect
the source code to be given as a `ten_Source`, which is a stream
abstraction, allowing Ten to compile code without caring about where
it came from.  The API provides a few function for creating such
streams from some of the more common code sources:

    ten_Source*
    ten_fileSource( ten_State* ten, FILE* file, char const* name );

    ten_Source*
    ten_pathSource( ten_State* ten, char const* path );

    ten_Source*
    ten_stringSource( ten_State* ten, char const* string, char const* name );

The `name` in these should give the name or url associated with
the code; it'll be reported in compilation and execution errors.
Ten will automatically release any streams passed into the API.

The `scope` parameter to the execution function tells the compiler
where the code should make definitions to.  This should generally
be `ten_SCOPE_LOCAL`.  Passing `ten_SCOPE_GLOBAL` causes any
definitions made at the root scope of the given code to define to
the runtime's global scope, overwriting existing global variables
or creating new ones; this usually isn't what we want, and is only
really practical for implementing REPLs.

The `ten_execute*()` functions create temporary fibers that'll be
used in executing the compiled code, since these fibers are never
returned to the user; then a yield will halt execution of the code
indefinitely.  If the temporary fibers fail (an error occurs) then
the error will automatically be forwarded to the next error handler,
either the current fiber or the current `errJmp` destination.

The difference between script execution and expression evaluation
is that `ten_executeScript()` will compile a sequence of delimited
Ten expressions from the source, and will have no result value,
while `ten_executeExpr()` will only compile a single expression from
the beginning of the source stream, returning the result of its
evaluation as a tuple.  In addition, as mentioned in the [language
description section](the-language.md) of this manual, scripts can
be prefixed with a
[unicode BOM](https://en.wikipedia.org/wiki/Byte_order_mark)
and a
<a href="https://en.wikipedia.org/wiki/Shebang_(Unix)">Unix shebang line</a>,
which the script compiler will ignore if present.

## <a name="5.4">5.4 - Compiling Code</a>
The compilation functions work pretty similarly to their execution
counterparts:

    void
    ten_compileScript( ten_State* ten, char const** upvals, ten_Source* src, ten_ComScope scope, ten_ComType out, ten_Var* dst );

    void
    ten_compileExpr( ten_State* ten,  char const** upvals, ten_Source* src, ten_ComScope scope, ten_ComType out, ten_Var* dst );

But there are two additional parameters.  The `upvals` is optional
and can be passed as `NULL`, but if given should give a `NULL`
terminated array of variable names.  The variables listed here will
be added as upvalues to the compiled closure, and can later be
accessed and modified via `ten_getUpvalue()` and `ten_setUpvalue()`.

The `out` tells the function what to produce as output, if given as
`ten_COM_FIB` then the compiled code will be wrapped in a fiber and
put into the `dst` variable; if passed as `ten_COM_CLS` then the
raw closure will be the result instead.

## <a name="5.5">5.5 - Accessing Globals</a>
Each instance of the Ten runtime maintains a pool of global variables,
which can be accessed from any code compiled after the variable has
been defined.

These variables can also be accessed and manipulated freely by C
code via:

    void
    ten_def( ten_State* ten, ten_Var* name, ten_Var* src );

    void
    ten_set( ten_State* ten, ten_Var* name, ten_Var* src );

    void
    ten_get( ten_State* ten, ten_Var* name, ten_Var* dst );

The `name` should be given as a symbol, and the respective global
will be loaded into `dst` or from `src`.  The semantics of these
functions are consistent with the semantics of the language itself,
so `ten_set()` can only mutate existing variables without defining
new ones, and `ten_def()` can define new variables, replace existing
ones, or undefine variables by defining to `udf`; and `ten_get()`
will give `udf` for any undefined variables.

    ten_def( ten, ten_sym( ten, "myVar" ), ten_int( ten, 123 ) )

## <a name="5.6">5.6 - Accessing Fields</a>
Record fields can be accessed through a similar interface as globals:

    void
    ten_recDef( ten_State* ten, ten_Var* rec, ten_Var* key, ten_Var* val );

    void
    ten_recSet( ten_State* ten, ten_Var* rec, ten_Var* key, ten_Var* val );

    void
    ten_recGet( ten_State* ten, ten_Var* rec, ten_Var* key, ten_Var* dst );

Again, the semantics are consistent with language level field operations.

## <a name="5.7">5.7 - Type Checking</a>
It's extremely important that the C code interacting with Ten does proper
type checking, because the interface functions won't do this automatically,
and passing the wrong Ten type to an API function can cause some unexpected
stuff to happen in a release build of the library.

The most efficient way to do type checking is via each type's respective
`ten_is<TYPE>()` function, where `<TYPE>` is a type name, for example
`ten_isInt()`.  The signature for such functions is:

    bool
    ten_is<TYPE>( ten_State* ten, ten_Var* var );

Though pointers and data objects are special cases:

    bool
    ten_isPtr( ten_State* ten, ten_Var* var, ten_PtrInfo* info );

    bool
    ten_isDat( ten_State* ten, ten_Var* var, ten_DatInfo* info );

For these the `info` can be passed as `NULL`, in which case they behave
exactly like the others; but if an `info` is given then the function
checks not only that the variable has the respective value type, but
also that the value was created with the given `info`.

The API also exposes a `ten_type()` function, which is the C counterpart
to the prelude's `type()` function.

    void
    ten_type( ten_State* ten, ten_Var* var, ten_Var* dst );

This loads the type name of a the value in `var`, into `dst`.

Similarly `ten_expect()` is the C counterpart to the prelude's `expect()`
function, which panics if the given values isn't of the expected type.

    void
    ten_expect( ten_State* ten, char const* what, ten_Var* type, ten_Var* var );

The `what` is what's reported as having the wrong type if the function panics,
and `type` should contain a type name symbol; this can be specified either
manually:

    ten_expect( ten, "myVar", ten_sym( ten, "Int" ), &myVar );

Or as one of the API's type variables, which can be obtained with the
respective `ten_<TYPE>Type()` function:

    ten_Var*
    ten_<TYPE>Type( ten_State* ten );

Again, the pointer and data variants of this function are exceptional:

    ten_Var*
    ten_ptrType( ten_State* ten, ten_PtrInfo* info );

    ten_Var*
    ten_datType( ten_State* ten, ten_DatInfo* info );

If these are given an `info` of `NULL` then the returned variable
will have the name of the respective base type; but if given an
`info` then the full (tagged if the info has a tag) type name will
be returned.

## <a name="5.8">5.8 - Native Data</a>
Ten has two special data types specifically to allow C data to be
represented within the runtime.  The Ptr (pointer) type represents
a raw C pointer, just an address, optionally tagged with the original
C type.  The Dat (data) type represents a buffer of raw C memory, and
can optionally allocate a set of associated member variables, which
are Ten variables that'll be attached to the object.

Both of these types can be associated with a `ten_<TYPE>Info` unit,
which provides information about the associated C type and how it
should be created and maintained in the Ten universe.

An important thing to note is that the destructor callbacks given
for Dat and Ptr types must **NOT** call back into the Ten runtime.
These will be called from the garbage collector, so any unexpected
long jumps (triggered by yields or errors) can break the state of
the runtime.


### <a name="5.8.1">5.8.1 - Pointers</a>
Ten's pointer type is similar to its symbol type, it isn't just a
raw pointer thrown into the Ten type system.  Instead, for each pointer
added to the runtime, an entry will be added in the internal pointer
table; and this entry will keep track of the pointer's type and address
information.  Any subsequent attempts to add the same pointer will be
merged into this same entry, so at any given type only one such entry
will exist for any pair of `(addr, info)`, where `addr` is the pointer's
address and `info` is its associated `ten_PtrInfo`, which may be `NULL`.

Pointers can be created without an associated `ten_PtrInfo` instance,
but such pointers will be untagged, so no type info will be maintained
for them.

We use the following to add a `ten_PtrInfo` to the runtime, to be used
in the creation of tagged pointers.

    typedef struct {
        char const*   tag;
        void        (*destr)( void* addr );
    } ten_PtrConfig;

    ten_PtrInfo*
    ten_addPtrInfo( ten_State* ten, ten_PtrConfig* config );

The `tag` should give the respective C type name with which pointers
created with this info should be tagged; and `destr` gives an optional
destructor.
Note that the destructor is for the associated pointer entry in the
pointer table, not each appearance of the pointer in the runtime, so
it'll only be called once all instances of a given pointer have expired
from the runtime.  Ten won't keep a copy of the given `config` pointer.

A pointer (tagged or otherwise) can be added to the runtime with:

    void
    ten_setPtr( ten_State* ten, void* addr, ten_PtrInfo* info, ten_Var* dst );

### <a name="5.8.2">5.8.2 - Data Objects</a>
The interface provided for data objects is very similar to the one
for pointers; a key difference though is that the `ten_DatInfo`
associated with a `Dat` object isn't allowed to be `NULL`, since
it keeps track of size and member information as well as the
object's type tag.

    typedef struct {
        char const* tag;
        unsigned    size;
        unsigned    mems;
        void      (*destr)( void* buf );
    } ten_DatConfig;

    ten_DatInfo*
    ten_addDatInfo( ten_State* ten, ten_DatConfig* config );

Again the `tag` gives the name of the C struct which objects created
with this info should represent, it can be `NULL` in which case
associated objects will be untagged.

The `size` gives the size of the raw memory buffer to be allocated,
and `mems` gives the number of member variables to attach to each
associated `Dat` instance.

The `destr`, if not `NULL`, will be called for each associated `Dat`
before it's collected by Ten's GC. A `Dat` object can be created with:

    void*
    ten_newDat( ten_State* ten, ten_DatInfo* info, ten_Var* dst );

And its member variables accessed and mutated via:

    void
    ten_setMember( ten_State* ten, ten_Var* dat, unsigned mem, ten_Var* val );

    void
    ten_getMember( ten_State* ten, ten_Var* dat, unsigned mem, ten_Var* dst );

## <a name="5.9">5.9 - Native Closures</a>
The process for creating native closures can be a bit... cumbersome due to
the number of parameters that need to be specified, and the use cases that
must be considered.  The first step is to create a native function, which
represents the immutable portion of the closure, which can be shared between
different instances.

    typedef struct {
        char const*  name;
        char const** params;
        ten_FunCb    cb;
    } ten_FunParams;

    void
    ten_newFun( ten_State* ten, ten_FunParams* p, ten_Var* dst );

The `name` can be given as `NULL`, but if a valid function name is
provided then it'll be reported in related error messages, which
can make debugging easier.  The `params` field is required as it
tells Ten _how many_ parameters the function expects, as well as
their names; it should given as a `NULL` terminated array of
parameter names following Ten's identifer rules.  The last name
in this list may be followed by an ellipsis `...`, to indicate
a variadic parameter; the semantics for these are the same as for
Ten closures, extra arguments will be passed in a record, which
will be passed as the variadic argument.  The `cb` is the actual
native function to be invoked when the associated Ten closure
is called.  This should have a signature of:

    ten_Tup
    cb( ten_Call* call );

Where the `ten_Call` struct looks like:

    typedef struct {
        ten_State* ten;  // The runtime state
        ten_Tup    args; // Tuple of arguments passed to the call
        ten_Tup    mems; // Members of the Data associated with the closure
        void*      data; // Memory buffer of the Data associated with the closure
    } ten_Call;
    
This is all enclosed in a single struct to make it easier and more
efficient to pass these fields to helper functions.  The callback
itself should return a tuple of return values.

The API provides a few helper macros to make native function definitions
a bit cleaner.  The `ten_define()` macro helps with the function's
signature, and gives it a prefix to prevent naming conflicts, and
`ten_arg()` and `ten_mem()` expand to `ten_Var` values for arguments
and members of the function respectively.

    ten_define(myFun) {
        ten_Var arg0 = ten_arg( 0 );
        ten_Var arg1 = ten_arg( 1 );
        ten_var mem0 = ten_mem( 0 );
        
        ...
    }

The name of a function defined with `ten_define()` can be resolved
with `ten_fun()` which expands to the given name, plus the same
prefix as used in `ten_define()`.

    ten_FunParams p = {
        .name   = "myFun",
        .params = (char const*[]){ "arg1", "arg2", NULL },
        .cb     = ten_fun(myFun)
    };
    ten_newFun( ten, &p, &funVar );

Once we've created a function, closures can be built with:

    void
    ten_newCls( ten_State* ten, ten_Var* fun, ten_Var* dat, ten_Var* dst );

Where `fun` gives a function and `dat` an optional `Dat` object to bind
to the closure.  If this is passed as `NULL` then the `mems` and `dat`
arguments of the function callback will also be passed a `NULL` when
the function is invoked.


### <a name="5.9.1">5.9.1 - Re-Entry</a>
Ten uses long jumps internally to handle fiber yields and errors, so
calls to native functions can't be expected to proceed without interruption
wherever they call API functions since Ten may decide to jump out of the
call.

For jumps triggered by errors, this usually isn't much of an issue, though
care must be taken to ensure that local resources are released properly
in case of a jump.

An issue arises, however, when we consider that fibers can be continued
after a yield; so what happens to the native function calls that were
in progress when the yield occurred?

Since Ten is implemented in portable C code, and the standard library
has no utilities for saving the state of a particular function call;
the API must provide its own system for making native function calls
re-entrant.

The system described here is, however, optional.  Native functions can
be implemented without using these calls, in which case any interrupted
calls to the function won't be continued when the fiber is continued,
instead Ten will automatically return an empty tuple `()` on the funtion's
behalf.

Ten's API provides two functions for making native calls re-entrant.

    long
    ten_seek( ten_State* ten, void* ctx, size_t size );

    void
    ten_checkpoint( ten_State* ten, unsigned cp, ten_Tup* dst );

The `ten_seek()` function registers `ctx` of size `size` as the
current call state, which will be copied and saved by Ten if a yield
is invoked within the call.  The function returns `-1` on the
first entry into the call, and subsequently returns the last
checkpoint seen before the previous entry was interrupted, or
again `-1` if no such checkpoint had been seen.  This should be
called before any invoked to `ten_checkpoint()` in a particular
entry into the native call, doing otherwise will result in
undefined behavior.  This should generally be called within
a goto switch at the start of the function, before any other logic,
but after all variable initialization.

The `ten_checkpoint()` function tells Ten what to return by
`ten_seek()` if the call is interrupted, and must subsequently
be continued; and where (within the `ctx`) it should put
the continuation values when the call is next re-entered.  The
continuation values will either be the returns values for the
function call in which the interrupting yield occurred or,
if the interruption was caused by a `ten_yield()` call in the
current function, the arguments passed to the `ten_cont()` call
that continued the fiber.

A native callback should call `ten_checkpoint()` before each
invocation of `ten_call()` or `ten_yield()`, otherwise the

The typical pattern for using this system is presented below.

    ten_define(foo) {
        // The call context.
        struct {
          int     var1;
          int     var2;
          ten_Tup args;
          ten_Tup rets;
        } ctx;

        // Variable initialization, this should be done for
        // every entry, since the tuple addresses will not
        // persist.
        ten_Var arg1 = ten_var( ctx.rets, 0 );
        ten_Var arg2 = ten_var( ctx.rets, 1 );

        ten_Var ret1 = ten_var( ctx.rets, 0 );
        ten_Var ret2 = ten_var( ctx.rets, 1 );

        ten_Var bar = ten_arg( 0 );

        // The goto switch continues the function from right
        // after the block that triggered the previous yield.
        switch( ten_seek( ten, &ctx, sizeof( ctx ) ) ) {
            case 0: goto cp0;
            case 1: goto cp1;
            case 2: goto cp2;
        }

        ... do some  stuff ...

        ten_checkpoint( ten, 0, &ctx.rets );
            ctx.args = ten_pushA( ten, "" );
            ctx.rets = ten_call( ten, &bar, &ctx.args );
        cp0:

        // We can rely on `ctx.rets` having a valid return tuple
        // even if the call was interrupted as Ten will put a
        // proper tuple in the checkpoint's destination location.

        ... do some more stuff ...

        ten_checkpoint( ten, 1, &ctx.rets );
          ctx.args = ten_pushA( ten, "II", 123, 321 );
          ten_yield( ten, &ctx.args );
        cp1:

        ... then do more stuff ...

        ten_checkpoint( ten, 2, &ctx.rets );
          ctx.args = ten_pushA( ten, "" );
          ten_yield( ten, &ctx.args );
        cp2:

        ...
    }

While multiple entries into the native function might be required,
from Ten's perspective only one call is made, with each yield and
continuation just starting from in the middle of the function where
the call left off.

## <a name="5.10">5.10 - Records</a>
Records can be created from C code with:

    void
    ten_newRec( ten_State* ten, ten_Var* idx, ten_Var* dst );

The `idx` can be passed as `NULL`, in which case a new index will
be created for the record automatically.  If provided the `idx`
variable should hold an index, which can be created with:

    void
    ten_newIdx( ten_State* ten, ten_Var* dst );

While the record constructor allows the `idx` argument to be
omitted, this convenience shouldn't be abused; records that
are expected to maintain a common set of keys should be created
with the same index.  It's also good practice to mark records
for index separation when appropriate to prevent index pollution
with:

    void
    ten_recSep( ten_State* ten, ten_Var* rec );

## <a name="5.11">5.11 - Fibers</a>
The C functions for dealing with fibers resemble their counterparts
in the Ten prelude pretty closely, so I'll avoid a detailed account.

    void
    ten_newFib( ten_State* ten, ten_Var* cls, ten_Var* tag, ten_Var* dst );

    ten_FibState
    ten_state( ten_State* ten, ten_Var* fib );

    ten_Tup
    ten_cont( ten_State* ten, ten_Var* fib, ten_Tup* args );

    void
    ten_yield( ten_State* ten, ten_Tup* vals );

    void
    ten_panic( ten_State* ten, ten_Var* val );

The `tag` in `ten_newFib()` is optional, so can be passed a `NULL`,
but giving a descriptive tag can help with debugging.  The `ten_cont()`
and `ten_yield()` functions leave a tuple on the stack after finishing.
For `ten_cont()` this is the same tuple returned by the function, and
contains the continuation's yield values.  For `ten_yield()`, since
the function doesn't return, the tuple can be obtained on continuation
via the system outlined in [5.9.1 - Re-Entry](#5.9.1).

The `ten_state()` function returns the current state of the given
fiber as one of the following, which correspond to their symbol
counterparts returned by the prelude `state()` function.

    typedef enum {
        ten_FIB_RUNNING,
        ten_FIB_WAITING,
        ten_FIB_STOPPED,
        ten_FIB_FINISHED,
        ten_FIB_FAILED
    } ten_FibState;

## <a name="5.12">5.12 - Handling Errors</a>
Most Ten errors are localized to the fibers in which they occur, so
they'll never be seen by the host application.  But when a critical
error occurs, or an error is triggered by one of the API functions
outside the execution of a particular fiber; Ten will jump to the
`errJmp` specified in the `ten_make()`, where the error is expected
to be handled appropriately.  Recall this snippet from
[5.1 - Ten State](#5.1):

    ten_State* ten;
    jmp_buf    jmp;
    if( setjmp( jmp ) ) {
        fprintf( stderr, "Error\n" );
        exit( 1 );
    }
    ten = ten_make( NULL, &jmp );

The error handler implemented here isn't very helpful, it doesn't
attempt to recover and doesn't produce a comprehensive error report;
just exists.

A more useful error handler, and a modified form of the code used
in the official Ten CLI, is given below.

    ten_State* ten;
    jmp_buf    jmp;
    if( setjmp( jmp ) ) {
        ten_ErrNum  err = ten_getErrNum( ten, NULL );
        char const* msg = ten_getErrStr( ten, NULL );
        if( err == ten_ERR_NONE )
            return;
        fprintf( stderr, "Error: %s\n", msg );

        ten_Trace* tIt = ten_getTrace( ten, NULL );
        while( tIt ) {
            char const* unit = "???";
            if( tIt->unit )
                unit = tIt->unit;
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
        exit( 1 );
    }
    ten = ten_make( NULL, &jmp );

Though we still exit here without attempting any sort of recovery,
that's more for a lack of anything else to do; the runtime instance
will still be useable after error except `ten_ERR_FATAL`, which
indicates a lack of memory.  If a fatal error _is_ received then the
runtime should be freed (via `ten_free()`) and discarded.

The above snippet makes use of the following error handling functions:

    ten_ErrNum
    ten_getErrNum( ten_State* ten, ten_Var* fib );

    char const*
    ten_getErrStr( ten_State* ten, ten_Var* fib );

    ten_Trace*
    ten_getTrace( ten_State* ten, ten_Var* fib );

Whose purpose should be pretty clear.  The `ten_getTrace()` function
returns the stack trace produced by the error, which consists of nodes
of the form:

    typedef struct ten_Trace ten_Trace;
    struct ten_Trace {
        char const* unit;
        char const* file;
        unsigned    line;
        ten_Trace*  next;
    };

This list shouldn't be freed by the host, and is only guaranteed to
be available until the next error occurs.  The `unit` for each trace
will either be the tag of the fiber in which the error occurred,
the string `"compiler"` for compilation errors, or `NULL` if the
error occurred in an untagged fiber, or outside of a running fiber.
The `file` will give the name of the file where the error occurred,
it can be `NULL` not known.

If the `fib` argument for these functions is given (not `NULL`), then
the requested information will be returned for the specified fiber
rather than the global error info.

The error value produced with the error can also be retrieved as a
value instead of a string with:

    void
    ten_getErrVal( ten_State* ten, ten_Var* fib, ten_Var* dst );

And the current error info can be cleared with:

    void
    ten_clearError( ten_State* ten, ten_Var* fib );

Though this won't change the stage of a failed fiber.

Since Ten uses long jumps for handling errors, it means that any API
function can trigger a long jump out of a native function; this can be
an issue when it comes to local resource management, as such a jump
can occur before a function has had a chance to clean up after itself.

The `ten_swapErrJmp()` function can be used to temporarily replace the
current error handler; to allow the caller to perform cleanup or other
operations before propagating the error.

    jmp_buf*
    ten_swapErrJmp( ten_State* ten, jmp_buf* errJmp );

This function returns the previous error handler, which may not be the
one originally passed to `ten_make()`, so the caller should be careful
later to swap the same pointer back in instead of replacing it with
the root level error handler.

Once any cleanup operations have been complete, and the native code
is ready to propagate the error, a call to:

    void
    ten_propError( ten_State* ten, ten_Var* fib );

With `fib = NULL` will do so, if `fib` is given then the given
fiber's error will be propagated instead of the global error.

A typical error intercept would look something like:

    void* someResourceThatNeedsToBeFreed = malloc( 1024000 );

    jmp_buf  jmp;
    jmp_buf* old = ten_swapErrJmp( ten, &jmp );
    if( setjmp( jmp ) ) {
        ten_swapErrJmp( ten, old );

        free( someResourceThatNeedsToBeFreed );

        ten_propError( ten, NULL );
    }

    ...

    if( shouldSomeErrorBeThrown() )
        ten_panic( ten, ten_str( ten, "SomeErrorThatShouldBeThrown" ) );
    ...

    ten_swapErrJmp( ten, old );

## <a name="5.13">5.13 - Types and Functions</a>
This subsection provides a brief description of each of the API's types and
functions; it can be used as a quick API reference, but doesn't provide
the more detailed explanations given in the previous parts of this section.


### <a name="type-ten_State">`struct ten_State`</a>
Represents an instance of the Ten runtime.

### <a name="type-ten_DatInfo">`struct ten_DatInfo`</a>
Represents a set of meta info to be attached to Ten's `Dat` objects.

### <a name="type-ten_DatConfig">`struct ten_DatConfig`</a>
The set of parameters used for creating a `ten_DatInfo` via `ten_addDatInfo()`.

    typedef struct {
        char const* tag;
        unsigned    size;
        unsigned    mems;
        void      (*destr)( void* buf );
    } ten_DatConfig;

The `tag` field gives a type tag to be attached to associated
`Dat` objects; this should generally be the name of the C struct
represented in the object's memory buffer; though it can be given
as `NULL` to leave the objects untagged.

The `size` and `mems` fields the the size of the memory buffer to
allocate for each object, and the number of member variables to attach
to it respectively.

The `destr`, if not `NULL`, will be called with memory buffer of each
associated object before it's released by the garbage collector.


### <a name="type-ten_PtrInfo">`struct ten_PtrInfo`</a>
Represents a set of meta info to be attached to Ten's `Ptr` values.

### <a name="type-ten_PtrConfig">`struct ten_PtrConfig`</a>
The set of parameters used for creating a `ten_PtrInfo` via `ten_addPtrInfo()`.

    typedef struct {
        char const*   tag;
        void        (*destr)( void* addr );
    } ten_PtrConfig;

The `tag` field gives a type tag to be attached to associated pointer
values.  If `NULL` then then the values will be taggless.

The `destr` will be called with a pointer's address when all occurences
of the pointer have expired from the runtime.


### <a name="type-ten_Tup">`struct ten_Tup`</a>
Represents an internal array of Ten values, which can be referenced
through `ten_Var` instances.

### <a name="type-ten_Var">`struct ten_Var`</a>
Represents slot within a `ten_Tup`.  Used by the API as variables
where values can be put or taken from.

### <a name="type-ten_FunCb">`func ten_FunCb`</a>
Callback implementing a native function to be called as a Ten function.

    typedef ten_Tup (*ten_FunCb)( ten_Call* call );

### <a name="type-ten_Call">`struct ten_Call`</a>
Various bits of function call info combined into a single struct to
make it easier and more efficient to pass the info on to helper
functions.

    typedef struct {
        ten_State* ten;  // The runtime state
        ten_Tup    args; // Tuple of arguments passed to the call
        ten_Tup    mems; // Members of the Data associated with the closure
        void*      data; // Memory buffer of the Data associated with the closure
    } ten_Call;

### <a name="type-ten_MemCb">`func ten_MemCb`</a>
Memory management function.  Should implement the combined functionality
of the standard `malloc()`, `realloc()`, and `free()` functions.

    typedef void* (*ten_MemCb)( void* udata,  void* old, size_t osz, size_t nsz );

Given `nsz = 0 && osz > 0` the function should free `old`.

Given `nsz > 0 && osz > 0` the function should reallocate `old`
from `osz` to `nsz`.

Given `nsz > 0 && osz = 0` the function should allocate a block
of size `nsz`.

Each call will be passed the `udata` given in `ten_Config`.

### <a name="type-ten_FunParams">`struct ten_FunParams`</a>
Parameters for bulding a Ten function from a native callback.

    typedef struct {
        char const*   name;
        char const**  params;
        ten_FunCb     cb;
    } ten_FunParams;

The `name` gives a name for the created function, it can be given as
`NULL`, but providing a descriptive name helps with debugging.

The `params` list is required.  It should be a `NULL` terminated
array of parameter names for the function, which should follow
the rules for Ten identifiers.  The last name can be followed by
an ellipsis `...` to indicate a variadic parameter.

The `cb` should give the callback to be wrapped by the created
Ten function.

### <a name="type-ten_ErrNum">`enum ten_ErrNum`</a>
Ten error codes.

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

### <a name="type-ten_ComType">`enum ten_ComType`</a>
Indicates the output type for compilation functions.

    typedef enum {
        ten_COM_CLS,
        ten_COM_FIB
    } ten_ComType;

### <a name="type-ten_ComScope">`enum ten_ComScope`</a>
Possible scope values for compilation and execution functions.

    typedef enum {
        ten_SCOPE_LOCAL,
        ten_SCOPE_GLOBAL
    } ten_ComScope;

### <a name="type-ten_FibState">`enum ten_FibState`</a>
Possible states of a Ten fiber.

    typedef enum {
        ten_FIB_RUNNING,
        ten_FIB_WAITING,
        ten_FIB_STOPPED,
        ten_FIB_FINISHED,
        ten_FIB_FAILED
    } ten_FibState;

### <a name="type-ten_Trace">`struct ten_Trace`</a>
A trace node making up a single frame of a stack trace, with
a link to the next node.

    typedef struct ten_Trace ten_Trace;
    struct ten_Trace {
        char const* unit;
        char const* file;
        unsigned    line;
        ten_Trace*  next;
    };

### <a name="type-ten_Source">`struct ten_Source`</a>
Represents a stream of UTF-8 encoded Ten source code.

    typedef struct ten_Source {
        char const* name;
        int        (*next)( struct ten_Source* src );
        void       (*finl)( struct ten_Source* src );
    } ten_Source;

The `name` should give a descriptive name to the source, a file path
or similar, to be reported in errors originating from the code.

The `next` function should return the next byte of the stream, or
`-1` to indicate the end of the source.

The `finl` function will be called to cleanup after the stream, either
once Ten is finished compiling its code or if an error occurs during
compilation.

### <a name="type-ten_Config">`struct ten_Config`</a>
Runtime configuration.  Any of its fields can be passed as zero, and
Ten will use reasonable defaults.

    typedef struct ten_Config {
        void*       udata;
        ten_MemFun  frealloc;

        bool ndebug;

        double memGrowth;
    } ten_Config;

The `frealloc` field specifies a memory management callback to be used
by the runtime, and `udata` gives an arbitrary pointer to be passed
to each call.

The `ndebug` field can be set to `true` to indicate that Ten shouldn't
include debug info in the code it compiles; this reduces the memory
footprint, but reduces the quality of error reporting.

The `memGrowth` specifies a rate of growth for Ten's heap, or rather,
when the next GC cycle should be triggered.  After each GC cycle the
next cycle will be scheduled to run when `memNeed > memUsed*(1.0 +
memGrowth)`, where `memNeed` is the current number of allocated bytes
plus the number needed to finish the current allocation, and `memUsed`
is the number of bytes in use after the last garbage collection cycle.

### <a name="type-ten_Version">`struct ten_Version`</a>
Represents the semantic version of the linked Ten library.

    typedef struct {
        unsigned major;
        unsigned minor;
        unsigned patch;
    } ten_Version;

### <a name="var-ten_VERSION">`ten_VERSION : ten_Version`</a>
The semantic version of the linked Ten library.

### <a name="fun-ten_make">`ten_make( config, errJmp )`</a>
    config : ten_Config
    errJmp : jmp_buf
    return : ten_State*


Create a new Ten instance.  The `errJmp` should give a long jump destination
to which Ten will jump in case of an unhandled error.

### <a name="fun-ten_free">`ten_free( ten )`</a>
    ten : ten_State*

Releases an instance of the Ten runtime.

### <a name="fun-ten_pushA">`ten_pushA( ten, pat, ... )`</a>
    ten    : ten_State*
    pat    : char const*
    return : ten_Tup

Pushes a tuple to the current stack.  The `pat` should be a string
consisting of zero or more of the following letters.

|  Letter  |      C Type            | Ten Type |
|:--------:|:----------------------:|:--------:|
|   `U`    |              -         |   `Udf`  |
|   `N`    |              -         |   `Nil`  |
|   `L`    | `_Bool`, `bool`, `int` |   `Log`  |
|   `I`    |            long        |   `Int`  |
|   `D`    |            double      |   `Dec`  |
|   `S`    |        char const*     |   `Sym`  |
|   `P`    |            void*       |   `Ptr`  |
|   `V`    |          ten_Var*      |     ?    |

The length of the `pat` string will be the size of the allocated tuple,
with each letter indicating what type of Ten value it should be
initialized to.  The initialization values should follow the `pat`.

### <a name="fun-ten_pushV">`ten_pushV( ten, pat, ap )`</a>
    ten    : ten_State*
    pat    : char const*
    ap     : va_list
    return : ten_Tup

Same as `ten_pushV()`, but initialization values are provided in a
`va_list` instead of directly.

### <a name="fun-ten_pop">`ten_pop( ten )`</a>
    ten : ten_State*

Pop the top tuple from the stack.  It's up to the host to make sure
they don't underflow the stack.

### <a name="fun-ten_dup">`ten_dup( ten, tup )`</a>
    ten    : ten_State*
    tup    : ten_Tup*
    return : ten_Tup

Copies `tup` to the top of the stack.

### <a name="fun-ten_size">`ten_size( ten, tup )`</a>
    ten     : ten_State*
    tup     : ten_Tup*
    return  : unsigned

Returns the size of `tup`.

### <a name="fun-ten_def">`ten_def( ten, name, src )`</a>
    ten     : ten_State*
    name    : ten_Var*   : Sym
    src     : ten_Var*   : Any

Defines a global variable, initializing it to the value given in `src`.

### <a name="fun-ten_set">`ten_set( ten, name, src )`</a>
    ten     : ten_State*
    name    : ten_Var*    : Sym
    src     : ten_Var*    : Any

Sets the value of a global variable to the one given in `src`.

### <a name="fun-ten_get">`ten_get( ten, name, dst )`</a>
    ten     : ten_State*
    name    : ten_Var*    : Sym
    dst     : ten_Var*

Copies the value of a global variable into `dst`.

### <a name="fun-ten_type">`ten_type( ten, var, dst )`</a>
    ten     : ten_State*
    var     : ten_Var*
    dst     : ten_Var*

Copies the type name of `var` into `dst`.

### <a name="fun-ten_expect">`ten_expect( ten, what, type, var )`</a>
    ten     : ten_State*
    what    : char const*
    type    : ten_Var*    : Sym
    var     : ten_Var*    : Any

Ensures that the type name of `var` matches that given in `type`.  If
not then panics with a comprehensive error message, mentioning `what`
as having the wrong type.

### <a name="fun-ten_equal">`ten_equal( ten, var1, var2 )`</a>
    ten     : ten_State*
    var1    : ten_Var*    : Any
    var2    : ten_Var*    : Any
    return  : bool

Compares the values in the given variables, returning `true` if they're
equal, `false` otherwise.

### <a name="fun-ten_copy">`ten_copy( ten, src, dst )`</a>
    ten     : ten_State*
    src     : ten_Var*    : Any
    dst     : ten_Var*

Copies the value in `src` into `dst`.

### <a name="fun-ten_string">`ten_string( ten, tup )`</a>
    ten     : ten_State*
    tup     : ten_Tup*
    return  : char const*

Stringifies the given tuple, quoting strings and symbols, to produce
a something suitable as the output for a REPL evaluation.

### <a name="fun-ten_loader">`ten_loader( ten, type, loadr, trans )`</a>
    ten     : ten_State*
    type    : ten_Var*    : Sym
    loadr   : ten_Var*    : Cls
    trans   : ten_Var*    : Cls

Installs a module loader `loadr` for the given module `type`.  The
`trans` argument can be passed as `NULL`.

### <a name="fun-ten_panic">`ten_panic( ten, err )`</a>
    ten     : ten_State*
    err     : ten_Var*    : Any

Panics the current fiber, or throws an error to the runtime's
error handler if no fiber is running.

### <a name="fun-ten_call">`ten_call( ten, cls, args )`</a>
    ten     : ten_State*
    cls     : ten_Var*    : Cls
    args    : ten_Tup*
    return  : ten_Tup

Calls the given `cls` with `args`.  The result is returned as a
tuple, which is pushed to the stack.  This should only be called
from a native function callback as a function call requires a
running fiber.

### <a name="fun-ten_call_">`ten_call_( ten, cls, args, file, line )`</a>
    ten     : ten_State*
    cls     : ten_Var*    : Cls
    args    : ten_Tup*
    file    : char const*
    line    : unsigned
    return  : ten_Tup

Same as `ten_call()`, but explicitly passes the `file` and `line` to
report in stack traces.  The `file` string will not be copied, but a
pointer to it will be kept.

### <a name="fun-ten_yield">`ten_yield( ten, vals )`</a>
    ten     : ten_State*
    vals    : ten_Tup*

Yields the current fiber with the given `vals`.

### <a name="fun-ten_seek">`ten_seek( ten, ctx, size )`</a>
    ten     : ten_State*
    ctx     : void*
    size    : size_t
    return  : long

See [5.9.1 - Re-Entry](#5.9.1).

### <a name="fun-ten_checkpoint">`ten_checkpoint( ten, cp, dst )`</a>
    ten     : ten_State*
    cp      : unsigned
    dst     : ten_Tup*

See [5.9.1 - Re-Entry](#5.9.1).

### <a name="fun-ten_udf">`ten_udf( ten )`</a>
    ten     : ten_State*
    return  : ten_Var*    : Udf

Allocates a temporary variable, and initializes to `udf`.

### <a name="fun-ten_nil">`ten_nil( ten )`</a>
    ten     : ten_State*
    return  : ten_Var*    : Nil

Allocates a temporary variable, and initializes to `nil`.

### <a name="fun-ten_log">`ten_log( ten, log )`</a>
    ten     : ten_State*
    log     : bool
    return  : ten_Var*    : Log

Allocates a temporary variable, and initializes to `log`.

### <a name="fun-ten_int">`ten_int( ten, in )`</a>
    ten     : ten_State*
    in      : long
    return  : ten_Var*    : Int

Allocates a temporary variable, and initializes to `in`.

### <a name="fun-ten_dec">`ten_dec( ten, dec )`</a>
    ten     : ten_State*
    dec     : double
    return  : ten_Var*    : Dec

Allocates a temporary variable, and initializes to `dec`.

### <a name="fun-ten_sym">`ten_sym( ten, sym )`</a>
    ten     : ten_State*
    sym     : char const*
    return  : ten_Var*    : Sym

Allocates a temporary variable, and initializes to `sym`.

### <a name="fun-ten_ptr">`ten_ptr( ten, addr )`</a>
    ten     : ten_State*
    addr    : void*
    return  : ten_Var*    : Ptr

Allocates a temporary variable, and initializes to `addr`.

### <a name="fun-ten_str">`ten_str( ten, str )`</a>
    ten     : ten_State*
    str     : char const*
    return  : ten_Var*    : Str

Allocates a temporary variable, and initializes to `str`.

### <a name="fun-ten_fmtA">`ten_fmtA( ten, fmt, ... )`</a>
    ten     : ten_State*
    fmt     : char const*
    return  : ten_Var*    : Str

Allocates a temporary variable and initializes to a string
formatted according to `sprintf()` format specifiers.  In
addition to the formatters supported by the `sprintf()`
family of functions, three additional formatters are allowed.
The `%v` and `%q` formatters expect `ten_Var*` arguments and
stringify their contents.  The only difference is that `%q`
includes quotation marks for strings and symbols while `%v`
omits them.  The third formatter `%t` also expects a `ten_Var*`
but outputs the value's type instead of the value itself.

### <a name="fun-fmtV">`ten_fmtV( ten, fmt, va_list ap )`</a>
    ten     : ten_State*
    fmt     : char const*
    return  : ten_Var*    : Str

This does the same as [`ten_fmtA`](#ten_fmtA) except the format
arguments are passed as a `va_list` instead of variadic arguments,
making it possible to forward arguments to the function.


### <a name="fun-ten_fileSource">`ten_fileSource( ten, file, name )`</a>
    ten     : ten_State*
    file    : FILE*
    name    : char const*
    return  : ten_Source*

Creates a file source stream from the given `file`, the `name` should
give the name of the source file.

### <a name="fun-ten_pathSource">`ten_fileSource( ten, path )`</a>
    ten     : ten_State*
    path    : char const*
    return  : ten_Source*

Creates a file source stream from the given file `path`.

### <a name="fun-ten_stringSource">`ten_stringSource( ten, string, name )`</a>
    ten     : ten_State*
    string  : char const*
    name    : char const*
    return  : ten_Source*

Creates a string source from `string`, the `name` should give the code's
source name, to be reported in errors.

### <a name="fun-ten_compileScript">`ten_compileScript( ten, upvals, src, scope, out, dst )`</a>
    ten     : ten_State*
    upvals  : char const**
    src     : ten_Source*
    scope   : ten_ComScope
    out     : ten_ComType
    dst     : ten_Var*

Compiles `src` as a script to a closure or fiber.  The `upvals` can be
given as `NULL`, but if provided should be a `NULL` terminated array of
upvalue names, which will be accessible to the C API after compilation.
The `scope` determines how definitions are processed, if passed as
`ten_SCOPE_GLOBAL` then root level definitions in the given code will
be made to the global environment, if passed as `ten_SCOPE_LOCAL` then
they'll be made in a temporary local scope.

### <a name="fun-ten_compileExpr">`ten_compileExpr( ten, upvals, src, scope, out, dst )`</a>
    ten     : ten_State*
    upvals  : char const**
    src     : ten_Source*
    scope   : ten_ComScope
    out     : ten_ComType
    dst     : ten_Var*

Compiles the first expression in `src` to a closure or fiber.  The `upvals`
can be given as `NULL`, but if provided should be a `NULL` terminated array
of upvalue names, which will be accessible to the C API after compilation.
The `scope` determines how definitions are processed, if passed as
`ten_SCOPE_GLOBAL` then root level definitions in the given code will be
made to the global environment, if passed as `ten_SCOPE_LOCAL` then they'll
be made in a temporary local scope.

### <a name="fun-ten_executeScript">`ten_executeScript( ten, src, scope )`</a>
    ten     : ten_State*
    src     : ten_Source*
    scope   : ten_ComScope

Compiles `src` as a script and spins up a temporary fiber to execute it.

### <a name="fun-ten_executeExpr">`ten_executeExpr( ten, src, scope )`</a>
    ten     : ten_State*
    src     : ten_Source*
    scope   : ten_ComScope
    return  : ten_Tup

Compiles `src` as an expression and spins up a temporary fiber to execute
it, returning the result, and pushing it to the stack.

### <a name="fun-ten_isUdf">`ten_isUdf( ten, var )`</a>
    ten     : ten_State*
    var     : ten_Var*    : Any
    return  : bool

Returns `true` if the given `var` contains `udf`.

### <a name="fun-ten_areUdf">`ten_areUdf( ten, tup )`</a>
    ten     : ten_State*
    tup     : ten_Tup*
    return  : bool

Returns `true` all values in the given `tup` are `udf`.

### <a name="fun-ten_udfType">`ten_udfType( ten )`</a>
  ten       : ten_State*
  return    : ten_Var*    : Sym

Returns a variable containing the symbol `'Udf'`, this should
not be mutated.

### <a name="fun-ten_isNil">`ten_isNil( ten, var )`</a>
    ten     : ten_State*
    var     : ten_Var*    : Any
    return  : bool

Returns `true` if the given `var` contains `nil`.

### <a name="fun-ten_areNil">`ten_areNil( ten, tup )`</a>
    ten     : ten_State*
    tup     : ten_Tup*
    return  : bool

Returns `true` all values in the given `tup` are `nil`.

### <a name="fun-ten_nilType">`ten_nilType( ten )`</a>
    ten     : ten_State*
    return  : ten_Var*    : Sym

Returns a variable containing the symbol `'Nil'`, this should
not be mutated.

### <a name="fun-ten_isLog">`ten_isLog( ten, var )`</a>
    ten     : ten_State*
    var     : ten_Var*    : Any
    return  : bool

Returns `true` if the given value has a `Log` value.

### <a name="fun-ten_getLog">`ten_getLog( ten, var )`</a>
    ten     : ten_State*
    var     : ten_Var*    : Log
    return  : bool

Returns the value in `var` as a `bool`.


### <a name="fun-ten_logType">`ten_logType( ten )`</a>
    ten     : ten_State*
    return  : ten_Var*    : Sym

Returns a variable containing the symbol `'Log'`, this should
not be mutated.



### <a name="fun-ten_isInt">`ten_isInt( ten, var )`</a>
    ten     : ten_State*
    var     : ten_Var*    : Any
    return  : bool

Returns `true` if the given value has a `Int` value.

### <a name="fun-ten_getInt">`ten_getInt( ten, var )`</a>
    ten     : ten_State*
    var     : ten_Var*    : Int
    return  : long

Returns the value in `var` as a `long`.


### <a name="fun-ten_intType">`ten_intType( ten )`</a>
    ten     : ten_State*
    return  : ten_Var*    : Sym

Returns a variable containing the symbol `'Int'`, this should
not be mutated.



### <a name="fun-ten_isDec">`ten_isDec( ten, var )`</a>
    ten     : ten_State*
    var     : ten_Var*    : Any
    return  : bool

Returns `true` if the given value has a `Dec` value.

### <a name="fun-ten_getDec">`ten_getDec( ten, var )`</a>
    ten     : ten_State*
    var     : ten_Var*    : Dec
    return  : double

Returns the value in `var` as a `double`.


### <a name="fun-ten_decType">`ten_decType( ten )`</a>
    ten     : ten_State*
    return  : ten_Var*    : Sym

Returns a variable containing the symbol `'Dec'`, this should
not be mutated.



### <a name="fun-ten_isSym">`ten_isSym( ten, var )`</a>
    ten     : ten_State*
    var     : ten_Var*    : Any
    return  : bool

Returns `true` if the given value has a `Sym` value.

### <a name="fun-ten_getSymBuf">`ten_getSymBuf( ten, var )`</a>
    ten     : ten_State*
    var     : ten_Var*    : Sym
    return  : char const*

Returns the given `Sym` variable's string buffer.

### <a name="fun-ten_getSymLen">`ten_getSymLen( ten, var )`</a>
    ten     : ten_State*
    var     : ten_Var*    : Sym
    return  : size_t

Returns the given `Sym` variable's size.


### <a name="fun-ten_symType">`ten_symType( ten )`</a>
    ten     : ten_State*
    return  : ten_Var*    : Sym

Returns a variable containing the symbol `'Sym'`, this should
not be mutated.





### <a name="fun-ten_isPtr">`ten_isPtr( ten, var, info )`</a>
    ten     : ten_State*
    var     : ten_Var*    : Any
    info    : ten_PtrInfo*
    return  : bool

Returns `true` if the given value has a `Ptr` value.  If `info`
is not `NULL` then also checks that the pointer was created with
the given `info`.

### <a name="fun-ten_getPtrAddr">`ten_getPtrAddr( ten, var )`</a>
    ten     : ten_State*
    var     : ten_Var*    : Sym
    return  : void*

Returns the given `Ptr` variable's address.

### <a name="fun-ten_getPtrInfo">`ten_getPtrInfo( ten, var )`</a>
    ten     : ten_State*
    var     : ten_Var*    : Sym
    return  : ten_PtrInfo*

Returns the given `Ptr` variable's `ten_PtrInfo`.


### <a name="fun-ten_ptrType">`ten_ptrType( ten, info )`</a>
    ten     : ten_State*
    info    : ten_PtrInfo*
    return  : ten_Var*    : Sym

Returns a variable containing the symbol `'Ptr'`, this should
not be mutated.  If `info` is not `NULL` then this symbol is
augmented with a tag, so the returned symbol will be `'Ptr:TAG'`
where `TAG` is the `info`'s tag.



### <a name="fun-ten_isStr">`ten_isStr( ten, var )`</a>
    ten     : ten_State*
    var     : ten_Var*    : Any
    return  : bool

Returns `true` if the given value has a `Str` value.

### <a name="fun-ten_getStrBuf">`ten_getStrBuf( ten, var )`</a>
    ten     : ten_State*
    var     : ten_Var*    : Sym
    return  : char const*

Returns the given `Str` variable's string buffer.

### <a name="fun-ten_getStrLen">`ten_getStrLen( ten, var )`</a>
    ten     : ten_State*
    var     : ten_Var*    : Sym
    return  : size_t

Returns the given `Str` variable's size.


### <a name="fun-ten_strType">`ten_strType( ten )`</a>
    ten     : ten_State*
    return  : ten_Var*    : Sym

Returns a variable containing the symbol `'Str'`, this should
not be mutated.




### <a name="fun-ten_isIdx">`ten_isIdx( ten, var )`</a>
    ten     : ten_State*
    var     : ten_Var*    : Any
    return  : bool

Returns `true` if the given value has a `Idx` value.

### <a name="fun-ten_newIdx">`ten_newIdx( ten, dst )`</a>
    ten     : ten_State*
    dst     : ten_Var*

Creates a new index, putting it in `dst`.

### <a name="fun-ten_idxType">`ten_idxType( ten )`</a>
    ten     : ten_State*
    return  : ten_Var*    : Sym

Returns a variable containing the symbol `'Idx'`, this should
not be mutated.





### <a name="fun-ten_isRec">`ten_isRec( ten, var )`</a>
    ten     : ten_State*
    var     : ten_Var*    : Any
    return  : bool

Returns `true` if the given value has a `Rec` value.

### <a name="fun-ten_newRec">`ten_newIdx( ten, idx, dst )`</a>
    ten     : ten_State*
    idx     : ten_Var*    : Idx
    dst     : ten_Var*

Creates a new record with the given `idx`, putting it in `dst`.  If
`idx = NULL` then automatically creates an index to use.

### <a name="fun-ten_recType">`ten_recType( ten )`</a>
    ten     : ten_State*
    return  : ten_Var*    : Sym

Returns a variable containing the symbol `'Rec'`, this should
not be mutated.

### <a name="fun-ten_recSep">`ten_recSep( ten, rec )`</a>
    ten     : ten_State*
    rec     : ten_Var*    : Rec

Marks the record in `rec` for separation.  See
[2.2 - Records](basic-concepts.md#2.2).


### <a name="fun-ten_recDef">`ten_recDef( rec, key, src )`</a>
    ten     : ten_State*
    rec     : ten_Var*    : Rec
    key     : ten_Var*    : Any
    src     : ten_Var*    : Any

Defines a new field in the given `rec`.

### <a name="fun-ten_recSet">`ten_recSet( rec, key, src )`</a>
    ten     : ten_State*
    rec     : ten_Var*    : Rec
    key     : ten_Var*    : Any
    src     : ten_Var*    : Any

Mutates a field in the given `rec`.

### <a name="fun-ten_recGet">`ten_recGet( rec, key, dst )`</a>
    ten     : ten_State*
    rec     : ten_Var*    : Rec
    key     : ten_Var*    : Any
    dst     : ten_Var*    : Any

Copies the value of a field in `rec` to `dst`.






### <a name="fun-ten_isFun">`ten_isFun( ten, var )`</a>
    ten     : ten_State*
    var     : ten_Var*    : Any
    return  : bool

Returns `true` if the given value has a `Fun` value.

### <a name="fun-ten_newFun">`ten_newFun( ten, p, dst )`</a>
    ten     : ten_State*
    p       : ten_FunParams* p
    dst     : ten_Var*

Creates a new Ten function, see [ten_FunParams](#type-ten_FunParams).

### <a name="fun-ten_funType">`ten_funType( ten )`</a>
    ten     : ten_State*
    return  : ten_Var*    : Sym

Returns a variable containing the symbol `'Fun'`, this should
not be mutated.



### <a name="fun-ten_isCls">`ten_isCls( ten, var )`</a>
    ten     : ten_State*
    var     : ten_Var*    : Any
    return  : bool

Returns `true` if the given value has a `Cls` value.

### <a name="fun-ten_newCls">`ten_newCls( ten, fun, dat, dst )`</a>
    ten     : ten_State*
    fun     : ten_Var*    : Fun
    dat     : ten_Var*    : Dat
    dst     : ten_Var*

Creates a new Ten closure wrapping the given `fun`.

### <a name="fun-ten_clsType">`ten_clsType( ten )`</a>
    ten     : ten_State*
    return  : ten_Var*    : Sym

Returns a variable containing the symbol `'Cls'`, this should
not be mutated.

### <a name="fun-ten_getUpvalue">`ten_getUpvalue( ten, cls, upv, dst )`</a>
    ten     : ten_State*
    cls     : ten_Var*    : Cls
    upv     : unsigned
    dst     : ten_Var*

Copies an upvalue of the given `cls` to `dst`.

### <a name="fun-ten_setUpvalue">`ten_setUpvalue( ten, cls, upv, src )`</a>
    ten     : ten_State*
    cls     : ten_Var*    : Cls
    upv     : unsigned
    src     : ten_Var*

Copies the value in `src` to one of `cls`'s upvalue slots.




### <a name="fun-ten_isFib">`ten_isFib( ten, var )`</a>
    ten     : ten_State*
    var     : ten_Var*    : Any
    return  : bool

Returns `true` if the given value has a `Fib` value.

### <a name="fun-ten_newFib">`ten_newFib( ten, cls, tag, dst )`</a>
    ten     : ten_State*
    cls     : ten_Var*    : Cls
    tag     : ten_Var*    : Sym
    dst     : ten_Var*

Creates a new Ten fiber wrapping the given `cls`.  The `tag` is
optional, it can be passed as `NULL`.

### <a name="fun-ten_fibType">`ten_fibType( ten )`</a>
    ten     : ten_State*
    return  : ten_Var*    : Sym

Returns a variable containing the symbol `'Fib'`, this should
not be mutated.

### <a name="fun-ten_state">`ten_state( ten, fib )`</a>
    ten     : ten_State*
    fib     : ten_Var*    : Fib
    return  : ten_FibState

Returns a fiber's state.  See [ten_FibState](#type-ten_FibState).

### <a name="fun-ten_cont">`ten_cont( ten, fib, args )`</a>
    ten     : ten_State*
    fib     : ten_Var*    : Fib
    args    : ten_Tup*
    return  : ten_Tup

Continues the given fiber with `args`.  The returned tuple is
pushed to the stack.



### <a name="fun-ten_isDat">`ten_isDat( ten, var )`</a>
    ten     : ten_State*
    var     : ten_Var*    : Any
    return  : bool

Returns `true` if the given value has a `Dat` value.

### <a name="fun-ten_newDat">`ten_newDat( ten, info, dst )`</a>
    ten     : ten_State*
    info    : ten_DatInfo*
    dst     : ten_Var*
    return  : void*

Creates a new `Dat` object based on the given `ten_DatInfo`.
Returns a pointer to the object's memory buffer.


### <a name="fun-ten_getDat">`ten_getDat( ten, buf, dst )`</a>
    ten     : ten_State*
    buf     : void*
    dst     : ten_Var*

Copies the `Data` object associated with the given `buf` into `dst`.
The `buf` should be a pointer previously returned by either `ten_newDat()`
or `ten_getDatBuf()`.

### <a name="fun-ten_datType">`ten_datType( ten, info )`</a>
    ten     : ten_State*
    return  : ten_Var*    : Sym

Returns a variable containing the symbol `'Dat'`, this should
not be mutated.  If `info` isn't `NULL` then augments the symbol
with a tag, so `'Dat:TAG'` is returned instead, where `TAG` is
the tag of the given `info`.

### <a name="fun-ten_getMember">`ten_getMember( ten, dat, mem, dst )`</a>
    ten     : ten_State*
    dat     : ten_Var*    : Dat
    mem     : unsigned
    dst     : ten_Var*

Copies a member of the given `dat` to `dst`.

### <a name="fun-ten_setMember">`ten_setMember( ten, dat, mem, src )`</a>
    ten     : ten_State*
    dat     : ten_Var*    : Dat
    mem     : unsigned
    src     : ten_Var*

Copies the value in `src` to one of `dat`'s member slots.

### <a name="fun-ten_getDatInfo">`ten_getDatInfo( ten, dat )`</a>
    ten     : ten_State*
    dat     : ten_Var*    : Dat
    return  : ten_DatInfo*

Returns the `ten_DatInfo` associated with the given `dat`.

### <a name="fun-ten_getDatBuf">`ten_getDatBuf( ten, dat )`</a>
    ten     : ten_State*
    dat     : ten_Var*    : Dat
    return  : void*

Returns the given `dat`'s memory buffer.


### <a name="fun-ten_members">`ten_members( ten, dat )`</a>
    ten     : ten_State*
    dat     : ten_Var*    : Dat
    return  : ten_Tup

Returns the tuple of member variables associated with the given
`Data` object.


### <a name="fun-ten_getErrNum">`ten_getErrNum( ten, fib )`</a>
    ten     : ten_State*
    fib     : ten_Var*    : Fib
    return  : ten_ErrNum

Returns the given fiber's error code, or the global error code
if `fib = NULL`.

### <a name="fun-ten_getErrVal">`ten_getErrVal( ten, fib, dst )`</a>
    ten     : ten_State*
    fib     : ten_Var*    : Fib
    dst     : ten_Var*

Copies the fiber's error value into `dst`.  If `fib = NULL` then
copies the global error value.

### <a name="fun-ten_getErrStr">`ten_getErrStr( ten, fib )`</a>
    ten     : ten_State*
    fib     : ten_Var*    : Fib
    return  : char const*

Stringifies the current error value of `fib`.  If `fib = NULL`
then does the same for the global error value.

### <a name="fun-ten_getTrace">`ten_getTrace( ten, fib )`</a>
    ten     : ten_State*
    fib     : ten_Var*    : Fib
    return  : ten_Trace*

Returns the given fiber's stack trace, if `fib = NULL` then
then returns the global stack trace.

### <a name="fun-ten_clearError">`ten_clearError( ten, fib )`</a>
    ten     : ten_State*
    fib     : ten_Var*    : Fib

Clears the fiber's error info, if `fib = NULL` then clears
the global error info.

### <a name="fun-ten_propError">`ten_propError( ten, fib )`</a>
    ten     : ten_State*
    fib     : ten_Var*    : Fib

Propagates the fiber's error to the next error handler in line,
either a fiber's local error handler or the global handler.  If
`fib = NULL` then the global error info is propegated.

### <a name="fun-ten_swapErrJmp">`ten_swapErrJmp( ten, errJmp )`</a>
    ten     : ten_State*
    errJmp  : jmp_buf*
    return  : jmp_buf*

Swaps the current error handler jump with the given one.  Returns
the old destination.

### <a name="mac-ten_define">`ten_define( NAME )`</a>
A helper macro to aid in the definition of native function callbacks.
This expands to a static function prototype for the given `NAME`
with a prefix added to prevent naming collisions.  The given name
should generally be the name that's gonna be used to refer to the
function within Ten.  The `ten_fun()` macro can be used to resolve
the function name produced by this macro.

    ten_define(foo) {
        ...
    }

### <a name="mac-ten_arg">`ten_arg( N )`</a>
A helper macro to access the `N`th argument passed to a native
function.  This macro expects `ten_Call* call` to be defined in
the current scope, and expands to a compound `ten_Var` literal.

    ten_define(foo) {
        ten_Var arg0 = ten_arg( 0 );
        ...
    }

### <a name="mac-ten_mem">`ten_mem( N )`</a>
A helper macro to access the `N`th member of the Data object
bound to the current native closure.  This macro expects 
`ten_Call* call` to be defined in the current scope, and expands
to a `ten_Var` compound literal.

    ten_define(foo) {
        ten_Var mem0 = ten_mem( 0 );
        ...
    }

### <a name="mac-ten_var">`ten_var( TUP, N )`</a>
A helper macro for accessing the `N`th slot of the given `TUP`
tuple.  This macro expands to a compoind `ten_Var` literal.

    ten_Tup tup = ten_pushA( ten, "U" );
    ten_Var var = ten_var( tup, 0 );

### <a name="mac-ten_fun">`ten_fun( NAME )`</a>
This macro adds a prefix to `NAME` to match the prefix used by
`ten_define`, this allows the function to be referenced without
relying on a particular prefix.

    ten_FunCb fun = ten_fun(foo);

[Next: Ten Module Loader](ten-module-loader.md)
