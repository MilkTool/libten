# Ten Language Guide
This guide is intended as a quick intro to the Ten programming
language.  Since the language itself is quite small and pretty
intuitive, this brief intro should go a good way toward outlining
Ten's capabilities and style.

* [About Ten](#about-ten)
* [Setting Up](#setting-up)
  - [Installation](#installation)
  - [Troubleshooting](#troubleshooting)
  - [Hello, World!](#hello-world)
* [Types](#types)
    - [Undefined](#undefined)
    - [Nil](#nil)
    - [Logical](#logical)
    - [Integral](#integral)
    - [Decimal](#decimal)
    - [Symbol](#symbols)
    - [String](#strings)
    - [Record](#records)
    - [Closure](#closures)
    - [Fiber](#fibers)
    - [Other Types](#other-types)
    - [Type Checking](#type-checking)
    - [Type Conversion](#type-conversion)
* [Tuples](#tuples)
* [Operators](#operators)
    - [Power Operator](#power-operator)
    - [Fix Operator](#fix-operator)
    - [Arithmetic Operators](#arithmetic-operators)
    - [Logical Operators](#logical-operators)
    - [Comparison Operators](#comparison-operators)
* [Variables](#variables)
    - [Definition](#definition)
    - [Mutation](#mutation)
* [Functions](#functions)
    - [Function Scope](#function-scope)
    - [Function Calls](#function-calls)
    - [Variadic Parameters](#variadic-parameters)
    - [Recursion](#recursion)
* [Conditional Expressions](#conditional-expressions)
  - [Case Analysis](#case-analysis)
  - [Replacement](#replacement)
  - [Short-Circuit Logic](#short-circuit-logic)
* [Block Expressions](#block-expressions)
* [Signals and Handlers](#signals-and-handlers)
* [Iteration](#iteration)
  - [Iterators](#iterators)
  - [Loops](#loops)
* [Strings](#strings)
* [Text](#text)
* [Lists](#lists)
* [Compiling](#compiling)
* [User IO](#user-io)
* [Modules](#modules)
  - [Script Modules](#script-modules)
  - [Native Modules](#native-modules)
  - [Data Modules](#data-modules)

## About Ten
Ten is a general purpose scripting language designed from the
ground up for simplicity.  The language is designed around two
ideals:

  1. _That elegance is achieved through simplicity_.  Elegant
     code is simple, readable, intuitive; and while a simple
     language doesn't enforce the creation of such code, it
     certainly discourages writing of the needlessly complex
     by omitting many of the gadgets thoughtlessly used in
     its creation.

  2. _A language should stay out of the coder's way_.  That is,
     a good language doesn't bloat the mind with 'features' that
     try to do your work for you, but more often just give you
     extra to consider and reason about.  A language should
     provide a minimal tool-set and let you focus on the problem
     itself instead of all the flashy gadgets.

That said, Ten isn't the be-all and end-all of languages, and
it doesn't try to be.  Ten is a _scripting_ language, good for
writing semi-disposable code quickly and easily; not so good
at anything needing a lower abstraction level or amazing
performance.

The idea behind Ten isn't particularly unique, several existing
languages place emphasis on simplicity and minimalism, it does
however feature its own unique style, without the compatibility
baggage of older languages; and a few original ideas including
the useful semantics of `udf` values and the state sharing
properties of records.

## Setting Up
Before getting into the meat of the language, we'd better setup
a common working environment to keep us on the same page.  The
core Ten language itself is implemented as a library, which
isn't practical for our purposes of demonstration and exploration.
The official CLI (Command Line Interface) for Ten, on the other
hand, is much more suitable.  Unfortunately the CLI depends on
POSIX functionality, so Windows users will need some form of
abstraction layer like [Cygwin](https://www.cygwin.com/).

The CLI is packaged with some supplementary libraries in addition
to the core Ten language with its builtin functions; this guide
will only cover the core language features, along with a brief
exploration of the [ten-load](https://github.com/raystubbs/ten-load)
module loading system which is also included with the CLI.

### Installation
Ten's CLI, along with the language implementation itself, currently
isn't at a stable enough point in development to warrant binary
distributions for any platform.  But installation from source is
quite painless:

    git clone --recursive https://github.com/raystubbs/ten-cli
    cd ten-cli
    make
    sudo make install

Assuming that is, that a GNU toolchain is being used for the build,
otherwise you'll need to compile everything manually for now; since
it's all standard C compilation shouldn't be difficult if you're
versed in the tools.

If bandwidth or storage is limited, and you'd rather not clone
the full repo then you'll need to download the following and
manually organize them into the indicated file tree:

* [ten-cli](https://github.com/raystubbs/ten-cli/archive/master.zip)
  - [ten-lang](https://github.com/raystubbs/ten-lang/archive/master.zip)
  - [ten-load](https://github.com/raystubbs/ten-load/archive/master.zip)

Then do the same as above, omitting the `git clone`.

### Troubleshooting
The first step after installation is to make sure everything works, try:

    ten --version

This should print something to the effect of:

    Ten 0.2.0
    License MIT
    Copyright (C) 2019 Ray Stubbs

If that's what you get, great, we're good to go.  Otherwise, check
through the following trouble cases.

#### Case 1
If you get message to the effect of:

    bash: ten: command not found...

Then the CLI executable isn't in your path.  Ten installs to the
`/usr/local/` directory subtree, and some systems don't have this
setup properly for execution by default.  To fix this issue you'll
need to add the `/usr/local/bin/` directory to the `PATH`
environment variable.  This can be done temporarily with:

    export PATH=$PATH:/usr/local/bin/

Unfortunately however, this is localized to the current terminal,
and will reset when it's closed.  For something more permanent
you'll need to add this line to your profile script[@](#mt-profile).

Try running the test line again, if you get the same error...
then something's really broken, you can submit an issue
[here](https://github.com/raystubbs/ten-cli/issues) and it'll
be worked on.  If you see a different error message then
look through the rest of the cases, chances are if you had
trouble with this one then [Case 2](#case-2) will also be
an issue.

#### Case 2
If you get a message similar to:

    ten: error while loading shared libraries: libten.so: cannot open shared object file: No such file or directory

Then the library installation directories `/usr/local/lib/`
and `/usr/local/lib64/` need to be added to the library path,
usually `LD_LIBRARY_PATH`, but for Mac this is `PATH`.  For
a temporary solution we can do:

    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib/:/usr/local/lib64/

This however only sets the variable in the current terminal,
and it'll be reset when it's closed.  For something more
permanent you'll need to add this line to your profile
script[@](#mt-profile).

Try running the test line again, if you get the same error...
then something's really broken, you can submit an issue
[here](https://github.com/raystubbs/ten-cli/issues) and it'll
be worked on.  If you see a different error message then
look through the rest of the cases.


### Hello, World!
For the sake of tradition... let's do this first.  We'll do
two versions, a REPL[@](#df-repl) and a script to demonstrate
both aspects of the CLI.

A REPL can be launched by just running `ten` with no arguments,
you'll be presented with a prompt.  Submitting `"Hello, World!"`
will just print the same value back out:

    ten
    $ "Hello, World!"
    : "Hello, World!"

Now create a script file `hello.ten` containing:

    show( "Hello, World", N )

And run it:

    ten hello.ten
    Hello, World!

The CLI will be used in examples and demonstrations throughout
the guide; and is a useful tool for exploring the semantics
not detailed in this guide.

## Types and Operators
Ten is a dynamically, but strongly, typed language.  This means
that types are checked at runtime rather than compile time, and
variables can hold values of any type; but the language
won't automatically convert between similar types of value.

While most of Ten's type system and the types themselves are
pretty standard; there are a few exceptions of note.

One such exception is the lack of any list or array type;
like Lua, from which Ten derives several ideas, Ten only
has one type of compound value, a hash-map-like structure
called a record.  The reason for this omission is, of course,
simplicity.  Records with numeric keys can, for the most part,
be used in place of these more specialized containers; and
for larger data sets (which records aren't great with) we
can use LISP style linked lists composed of record cells.

Another oddity in Ten's type system is the `udf` (undefined)
value, which is distinct from `nil`.  While many languages have
some form of 'empty value' singleton such as these; not many two
different such values.  And those that do, such as JavaScript,
assign no special semantics to either of these.  In Ten, however,
the `udf` value represents something that's unknown or missing,
while `nil` is just an empty placeholder value.  The distinction
here is important, as the former is usually an indication of some
kind of error, while the latter is just a normal value; and the
semantics are designed accordingly.  While `nil` can be passed
around and used like any other value; `udf` is given special
consideration, it can't be passed as a function call argument
and is restricted in several other ways to allow for early
discovery of related bugs.

Type names are given as symbols with three letter abbreviations
abbreviations of the full type name.  The type name for a value
can be obtained with the `type()` function, for example:

    $ type( 123 )
    : 'Int'
    $ type( 1.2 )
    : 'Dec'

### Undefined
As mentioned above, this is the singleton type for the value `udf`,
used to represent missing or unknown values.  The shortened type
name is `Udf`, and values of this type have special restriction
on how they can be used, to prevent the values themselves from
becoming bugs.

The `udf` value is not allowed to be passed as a function argument,
a record key, or a predicate to any type of conditional expressions;
and can only be passed as an operand to specialized operators, all
of which begin in `!` to clarify that its operands may contain `udf`.

In addition, since the `udf` value is returned for undefined
variable references, and undefined variables themselves can
be said to 'hold' this value; it's given special semantics
when assigned to variables.

When an existing variable is defined to `udf`, the variable itself
becomes undefined, and can thus no longer be captured by closures;
mutations however are not allowed to set a variable's value to `udf`.

### Nil
This is the 'normal' counterpart to `Udf`.  This has a the shortened
type name `Nil` and is the singleton type for the value `nil`.  The
`nil` value represents nothing, an empty value; and is useful as
a placeholder value without the special properties of `udf`.

### Logical
This is your standard boolean type, though named differently.  It
covers the `true` and `false` values.

### Integral
A 32 bit signed integer type.  Represented syntactically in the usual
way, a sequence of contiguous digits.  Ten also allows the digits of
a number to be padded with '\_' after the first digit, so `1_000_000`
is the same as `1000000`.

Ten doesn't have any syntax for numbers in alternative bases.  So
hex, octal, and binary numbers have to be parsed from string with
the `hex()`, `oct()`, and `bin()` functions respectively.

    $ hex"FFF"
    : 4095
    $ oct"777"
    : 511
    $ bin"111"
    : 7

### Decimal
A 64 bit double precision float type.  Represented syntactically in
the usual way; though exponent suffixes aren't supported.  As with
integrals, we can also pad the number with '\_' to help with
readability, so `1_000.0` is the same as `1000.0`.

The above-mentioned functions for parsing numbers of alternate bases
work for parsing decimals as well, just include a decimal point in
the number:

    $ hex"F.F"
    : 15.937500
    $ oct"7.7"
    : 7.875000
    $ bin"1.1"
    : 1.500000

### Symbol
Symbols are similar to strings, but not the same.  Unlike strings,
symbols are interned at compile time (or creation time), for fast
constant time comparison.  This makes them suitable for use as
enumerations or flags, but they aren't as efficient to create as
strings, and require more memory.

Shorter symbols (< 5 bytes) are encoded directly into a value's
payload instead of in an intern pool; so they can be created
really efficiently and require very little memory, so they're
also great for representing multi-byte characters.

Symbols are represented syntactically as a sequence of characters
between single quotes `'...'`.  The terminating quote can, however,
be omitted at the end of a line, in which case the line end
terminates the symbol literal.

    'this is a symbol'
    'this is another

An alternative quoting syntax can be used for multi-line symbols,
or symbols that need to contain a quote character.

    '|this is a symbol with a  ' character|'
    '|this is a symbol with
    two lines|'  

### String
This is your standard string type.  The syntactic representation
is similar to that of symbols, but double quotes are used instead.

    "this is a string"
    "this is another"
    "|this is a string with a " character"|
    "|this is a string with
    two lines|"

### Record
Records are Ten's counterpart to the hash maps of similar
languages.  Record constructors consist of a sequence of
key-value pairs enclosed in curly brackets `{...}`.  The
entries are delimited by `,` comma or `\n` newline, and
one or more of these can appear between entries.  The
beginning and end of the record can also optionally be
padded by some number of delimiters.

    { @'k1': 'v1', @'k2': 'v2' }

Record keys consist of a `@` followed by some primary expression
to be evaluated for the key's value.  In this case we just give
the key literally, but it could also be computed.

We can also use a bit of sugar for symbol keys, replacing the `@`
with a `.` and giving the symbol as an identifier.

    { .k1: 'v1', .k2: 'v2' }

Or we can omit the keys entirely, leaving Ten to assign them
automatically:

    { 'a', 'b' }

Implicit keys are assigned as integral keys beginning at `0`
and increment from left to right, so the above is equivalent
to:

    { @0: 'a', @1: 'b' }

Explicit and implicit keys can be combined in a single record,
however while not enforced, it's good practice to give all
implicit keys first, followed by explicit ones.

    { 'a', 'b', .k1: 'v1', .k2: 'v2' }

Whether this practice is observed or not, explicit keys are
ignored when assigning the values of implicit ones.

Record fields can be accessed with the same kinds of key
markers used in the constructor, only used as infix operators
between the record and key itself.

    def myRec: { .k1: 'v1', .k2: 'v2' }
    def myVal1: myRec.k1
    def myVal2: myRec@'k2'


It's essential to realize that while records _are_ a form of hash
map, they aren't designed to be used in the same way as their
counterparts in other languages.  Records are designed for memory
efficiency, and if used correctly allow for massive savings.  But
if used carelessly a lot of resources will be wasted.

Ten's record design isolates the lookup table into an independent
unit, an index, which can be shared amongst several records.  This
means each record doesn't have to have its own lookup table, the
largest portion of a map; but can instead share this state with
other records that have a similar set of keys, esentially reducing
the record to a relatively small array of values.

![Record Design](RecordDesign.png)

This design is, however, a double edged sword.  Since multiple
records are sharing a single index, each of them must allocate
enough value slots for all keys in the index.  So if the record
don't share the same set of keys, then there ends up being a
lot of empty slots; which amounts to wasted memory.

![Record Unbalanced](RecordUnbalanced.png)

The compiler assigns each record constructor an index, which will
be shared between all the records it constructs.  Since records
will often be used as object or struct like entities, which are
generally uniform amongst all instances; this is a pretty sane
approach.  However as might be expected, there are exceptions;
in some cases the records created by a common constructor will be
more diverse, with very different fields.  For such cases the
record should be 'separated' from its original index with the `sep()`
function.  This cuts ties between the record and its current index,
creating a new index with only the record's subset of keys; and
allowing new keys to be added without polluting the common index.

    def myRec: sep{ .k1: 'v1', .k2: 'v2' }

The `sep()` function takes a record as input, and _marks_ it as
separated, before returning the same record.  This doesn't
expend the work needed to create and populate a new index, instead
after being marked the record will be given its new index on the
next definition, if one ever occurs.  This 'lazy separation' is
ideal since it often can't be anticipated whether a record's
structure will be modified or not.  It's generally good practice
for library routines that return records to the user to mark
them in this way beforehand, to prevent index pollution in case
the user decides the modify them.

### Closure
Closures are first class functions, that can access their lexical
environment; that is, the variables in scope when the closure was
created.

This is Ten's only type of function, or callable object, and we'll
often just call them functions; though in some contexts the
distinction is relevant, since functions are actually another type
of object that serves the same purpose as indices do for records;
they contain the parts of a closure that can be shared between all
instances. But, ironically, raw functions can't be called directly,
and will rarely be seen at all in Ten.

Closure constructors begin with a parameter list, enclosed in `[...]`
with the same delimiter system as used in record constructors.
Following the parameter list comes a single expression, which serves
as the closure's body, and whose result is returned as the result
of a call.

    [ a, b ] a + b

Some number of 'padding' delimiters are allowed to appear between
the parameter list and result expression, so we could also say:

    [ a, b ]
      a + b

Or, using a block expression (which will be detailed later), a
closure can be given the more traditional 'body' as a block of
code:

    [ a, b ] do
      ...
    for a + b


We don't have a special syntax for function definitions, instead
just assign a closure to a variable:

    def add: [ a, b ] a + b

For direct assignments like this, a closure on the right hand side
and a single variable on the left; Ten will set the 'name' of the
closure as the variable's name, which will be used to create
better error messages.

To call a closure we use the 'invisible' call operator, which
as higher precedence than the path operators (`@` and `.`).  This
operator is recognized by the appearance of two consecutive
expressions, without another operator in between them.

    $ def id: [ v ] v
    : udf
    $ id 123
    : 123
    $ id "hello"
    : "hello"
    $ id id
    : <Cls>

For functions that accept multiple arguments, we pass them as
a tuple; which consists of a number of delimited values between
parentheses.

    $ add( 1, 2 )
    : 3

It's also good practice to wrap single arguments that don't have
their own grouping syntax (e.g `"..."`, `{...}`, `[...]`) in
parentheses to highlight the function call:

    $ id( 123 )
    : 123

#### Variadic Parameters
In some cases it may be useful to allow a function to take an
arbitrary number of arguments; for example consider a `sum()`
function, which adds all its arguments to produce the sum. This
can be achieved with a variadic parameter, which can be given
as the last parameter in the list:

    def sum: [ vals... ] addAllTheValues( vals )

Any extra arguments passed to a call to the function, that is
arguments after the last non-variadic parameter's position,
will be put into a record, and passed in place of the `vals`
parameter.  These extra arguments are assigned keys based
on their position after the `vals` parameter's location, so
for example if we call:

    sum( 4, 3, 2, 1 )

Then `4` will be put under the key  `@0`, `3` under the key `@1`,
and so on.  The variadic records created for each function call
all share the same index, so don't pollute it.

#### Recursion
One of the main issues with not having a dedicated function
definition syntax is that recursion can't be done in the usual
way.  If we say something like:

    def foo: [] foo()

Then, since the right hand expression is evaluated before the
definition, `foo` is undefined when the closure is created,
and so isn't captured by the closure.  So `foo()` is undefined
within the closure.

    $ def foo: [] foo()
    : udf
    $ foo()
    Error: Attempt to call non-Cls type Udf
      unit: ???        line: 1    file: <REPL>              
    $

We can overcome this by first defining `foo` to a `nil` value
to allow for capture by a closure, then setting it to the closure
itself:

    def foo: nil
    set foo: [] foo()

But this is awkward and redundant.  Instead Ten has a special `this`
variable which is defined for each closure, and contains a reference
to the current closure being evaluated.  So instead of the above, we
can say:

    def foo: [] this()

And... don't call that function ;)  The `this` variable is read-only,
so attempts at definition or assignment will result in compilation
failure.

### Fiber
Fibers are an essential part of Ten's execution model, though they'll
often be invisible; all code is evaluated on a fiber.  These are light
weight threads that allow for a degree of concurrency, though without
any parallel execution.

Fibers allow the execution state of individual tasks to be isolated
from one another; allowing each task to be paused and continued on
demand, and preventing errors that occur in one task from propagating
to the rest of the program; in this sense serving as a form of
exception handling device.

Unlike records and closures, there's no language level fiber
constructor, instead we use the `fiber()` function to wrap a
task (a closure) in a fiber.

    def f: fiber[ a, b ] a + b

A fiber can be started, or continued, with the `cont()` function.
This accepts any arguments to be forwarded to the fiber as an
additional record parameter, and returns the result of evaluating
the fiber's closure.

    $ cont( f, { 10, 5 } )
    : 15

Once the wrapped closure returns (the task if finished) the
wrapping fiber is set into the `finished` state, and can no
longer be continued, since it has nothing to do.  The state
of a fiber can be obtained with the `state()` function.

    $ state( f )
    : 'finished'

#### Error Handling
One of the most useful features of fibers is that they prevent
internal errors from propagating to the rest of the program.  An
error that prevents a fiber from finishing its task, only halts
that one fiber, and allows the rest of the program to continue
on.

For example say we have the fiber:

    def d: [ a, b ] a / b

Ten throws an error on division by zero:

    $ 15/0
    Error: Division by zero
      unit: ???        line: 1    file: <REPL>

But if we evaluate the same, within its own fiber, then the
error never interferes with the greater program.

    $ cont( d, { 15, 0 } )
    : ()

There's no result because of the error, but no error message
either.  Instead the fiber is put in a failed state.

    $ state( d )
    : 'failed'

And its error value (usually a message) can be obtained with:

    $ errval( d )
    : "Division by  zero"

For more verbosity we can even obtain the stack trace generated
when the error occurred, as a record of trace nodes.

    $ trace( d )
    : { { .line: 1, .file: '<REPL>' } }

This is, of course, more useful for script files; since giving `<REPL>`
as the source file isn't particularly helpful.

In general minor errors should avoid using fibers as exception
handlers, some sort of return code or extra 'error' return (as
is done in Go) should be preferred.  But for more critical errors
or bad input, the `panic()` function can be called to throw a
user error:

    $ def p: fiber[] panic"Something broke"
    : udf
    $ cont( p, {} )
    : ()
    $ state( p )
    : 'failed'
    $ errval( p )
    : "Something broke"


#### Yield and Continue
Their application to exception handling isn't the only useful
property of fibers.  As mentioned before, a fiber can be
started or continued with `cont()`, but fibers can also be
paused on demand, to be continued later.

The `yield()` function stops execution of a fiber, causing
the `cont()` call that had continued the fiber to return
the arguments of the yield, for example:

    $ def y: fiber[] yield( 123 )
    : udf
    $ cont( y, {} )
    : 123

At this point the fiber will be in the stopped state, since it's
execution was stopped by the yield.

    $ state( y )
    : 'stopped'

When the fiber is next continued, the continuation arguments (those
forwarded from the `cont()` call) will serve as the return values
for the previous `yield()` call.  So if we continue this fiber
again with an argument, the argument will serve as the closure's
ultimate return value, and the return of the continuation since
this is the task's return value.

    $ cont( y, 321 )
    : 321

Now the wrapped closure has returned, so the fiber is finished:

    $ state( y )
    : 'finished'

This functionality is especially useful in conjunction with
asynchronous IO, as a task can be paused while waiting for input,
then continued upon receipt of the requested data; allowing other
processing to be done in the meantime.  A task scheduler is
required, however, to use this strategy effectively; and the core
language doesn't provide one since it'd enlarge the implementation
and can't be implemented without some platform specific code.

The choice of a scheduler implementation is thus left to the user,
though the Ten CLI will likely ship with one in the future.

### Other Types
Though we've covered Ten's most prominent types, the language does
have a few other value types not detailed so far.  These types aren't
especially important when programming in Ten, and will rarely be seen
or used.  But they're more important from the perspective of an embedding
application, so will be covered in more detail in the embedding guide.

* Index

  This is the type given to the shared lookup tables used by records.

* Function

  This is the type given to the portion of closures that can be shared
  between instances.

* Pointer

  This is the type given to native memory addresses.  Though Ten
  doesn't know how to do anything with pointers, they can be passed
  to native functions, to which the pointer will be more useful.

* Data

  This is the type of native user objects.  They contain a block
  of raw memory to be used by native functions, and a set of member
  associated member variables.

### Type Checking
Besides the `type()` function for obtaining a value's type name,
Ten also features a specialized `expect()` function for type
checking; which panics if the given value isn't of the expected
type.

    def val: 123
    expect( "val", 'Int', val )

The first argument gives a 'unit' to report as not having the
proper type, in this case just the name of the variable, this
is used in the generated error message.

    $ def val: 123
    : udf
    $ expect( "val", 'Dec', val )
    Error: 
