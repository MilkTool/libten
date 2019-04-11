# Ten Language Guide
This guide is intended as a quick intro to the Ten programming language.
Since the language itself is quite small pretty intuitive, this brief
intro should go a good way toward outlining Ten's capabilities and
what it brings to the table as a new programming language.

## About Ten
Ten is a simple, consistent, and intuitive scripting language loosely
based on Lua and Scheme; but with a few neat (and quite unique)
features of its own.  Like Lua, Ten has only one compound data type:
the record.

Records work kind of like Lua's tables and hash maps from other
languages; only they're optimized for use more as objects or structs
than for containing large data sets.  Unlike more traditional hash
maps, records can share their lookup tables (dubbed indices) with
other records of similar structure, allowing for massive memory
savings as each record needn't have its own map, which is generally
the largest part of such data structures.

In addition to having a unique record system, Ten is also strongly typed
to avoid unexpected conversions, and boasts another interesting
value type: udf (undefined).  Though other languages have a similar
type... its semantics are often fairly useless.  Ten on the other
hand uses `udf` for error propegation, and its semantics are useful
in avoiding subtle bugs since you can't 'do' anything with `udf`
values; all normal operators (those not specificially designed for
dealing with `udf` values) will throw an error if the value is
passed as an operand, and it's illegal to assign an existing variable
to `udf` or to pass it a function call argument.

## Scripts and Delimiters
Ten script files are UTF-8 encoded files with a sequence of expressions
separated by delimiters (either ',' or '\n'); which will be executed in
sequence, with latter expressions able to access the variables defined
before them.

Ten is very consistent about delimiters, there are no special delimiters
for different contexts, and no special rule for 'trailing commas'; instead
for any form list or sequence in the language sub units are separated with
',' or '\n', and can be padded by the same at the start and end of the list,
extra delimiters (after the first) have no effect in any context, and can
be used for styling.

These rules apply to script files, tuples, records, and basically everything
else in the language.

## Assignment
Perhaps the most complex facet of Ten's syntax is its assignment
forms, which are quite flexible.  The language has two assignment
keywords `def` for defining new variables and `set` for mutating
existing ones.  The semantic differences are subtle do to Ten's
dynamic nature, but important all the same.

Variable definitions create new variables which are accessible following
the definitions.  Variables referenced before being defined resolve to
`udf`.  Definitions can also 'undefine' existing varaibles by defining
them to `udf`; this prevents the variable from being captured by
subsequent closures.

Mutations are only allowed to change the value of existing variables
in the current scope.  Changes made by these will be reflected in
any closures that have captured the modified variable, wherease
redefining the variable with a definition will replace the variable
with a new one; having no effect on associated closure upvalues.  Since
setting a variable to `udf` effectively undefines it, attempting to
do so with a `set` will throw an error; this ensures that variables
in parent scopes cannot be undefined by nested ones.

Both keywords otherwise work the same, and both can be used with a
variety of patterns, though the simplest is presented below:

    def foo: 123
    set foo: 321

This is the simplest form of assignment: single assigment.  There
are also a few types of multiple assignment, starting with: tuple
assignment.

    def ( a, b ): ( 1, 2 )
    set ( a, b ): ( 2, 1 )

With this we can assign to multiple variables at once, receiving
the values from a tuple (tuples will be detailed later, basically
they're groups of associated values).

But wait... there's more.  We can also do variadic tuple assignment:

    def ( x, xs... ): ( 1, 2, 3, 4 )

Here the first value `1` will be put in `x`, and the rest are loaded
into a new record, which is put in `xs`, with sequential integral
indices, so `xs@0 = 2`, `xs@1 = 3`, `xs@2 = 4`.  This is pretty useful
when we don't know how many values a function will return.  Note that
when using this form with the `set` keyword the variables, including
`xs` must already be defined; but the assignment will put a new record
in it.

Assignment directly from record fields is also allowed:

    def { v1: .k1, v2: .k2 }: { .k1: 1, .k2: 2 }

Here the variable `v1` will get the value associated with `k1` in
the record, and the same for `v2` and `k2`.  Implicit keys are also
allowed:

    def { v1, v2 }: { 1, 2 }

Both the record, and the variable pattern, as assigned sequential
integral keys starting at `0`, from left to right.  So `v1` will
be assigned to the value associated with the record's `0` key, which
is `1` in this case.

Record definitions also have a variadic form, but the semantics
are a bit different:

    def { x: .x, xs... }: { .x: 1, .y: 2, .z: 3 }

Since records aren't necessarily sequential like tuples, the variadic
in this form is set to a new record with all the key-value pairs of
the source record which have not already been assigned to a variable
in this expression.  So in this case `xs = { .y: 2, .z: 3 }` since
`x` was assigned explicitly.

And... we also have all the same forms for field definitions, for
example:

    def r: {}

    def r.k1:   1
    def r@'k2': 2

    def r( .k1, .k2 ): ( 1, 2 )

    def r.s: {}

    def r.s{ .s1: .k1, .s2: .k2 }: { .s1: 1, .s2: 2 }

The semantics for `def` vs `set` keywords are similar for field
definitions, the `set` keyword cannot create or remove existing
fields (by assigning to `udf`), but `def` can.

Like everything else in Ten, assignments are expressions so they must return
a value; all assignment expressions return `udf`.

Assignments are the most complicated part of the language, but
still fairly simple and consistent when you get the idea.

## Values
Ten has a pretty simple dynamic type system, with only a few types as outlined
below.  The type name of a value can be obtained, as a symbol, with the builtin
`type()` function.

**Udf:**
This is the type of the 'undefined' singleton value represented by the keyword
`udf`.  The `udf` value is used for error propagation and has special semantics
designed to catch certain types of bugs prevalent in other languages with an
'undefined' or equivalent value.  This value cannot be passed as a function
call argument (throws an error), nor to any of the normal operators, and as
mentioned in the previous section, variable sets can't put it into a variable.
So it's pretty difficult for a `udf` (which represents a missing or unknown
value) to go unnoticed.  Comparing any value to `udf` with the normal
comparison operators causes an error, so we have a special operator `!=` for
comparing operands that may be `udf`.

**Nil:** This is another singleton type for the `nil` singleton, which represents an
empty value.  This is different from a 'missing' value, and has no special
semantics like `udf`, though it is the only other value besides `false` that
indicates the failure of a conditional test.

**Log:** The type of logical values.  This covers the two truth values represented as
`true` and `false`.  These are used for logical expressions and as tests in
conditionals.

**Int:** A 32bit signed integral type, used for the usual counting and math stuff.

**Dec:** A 64bit double precision floating point number, again, used for numbery stuff.

**Sym:** The symbol type.  These are similar to strings from other languages, but
they're interned for constant time comparison ( so `'foo' = 'foo'`) without
a special string comparison function.  These are useful for representing
names and enumeration values, and of course their more prominent usage
as record keys (strings don't compare this way, so they don't make good
record keys).  We also encode characters as UTF-8 formatted symbols,
this is pretty efficient since shorter symbols (< 5 bytes) can be
encoded directly as an atomic value (no heap space necessary), so
they're fast to create a take very little space.  Symbols are wrapped
in single quotes and end at an end-of-line.  For multi-line symbols
or symbols that need to contain a quote we can use the alternate `'|...|'`
syntax.

**Str:** The string type you'd be familiar with from other languages, unlike some of
those languages, as mentioned above, Ten strings are heap allocated and
don't compare as expected with the normal comparison operators ( so `"foo" = "foo"` might be `false`).  Instead we'd use the a string comparison function
`ccmp( "foo", '=', "foo" )` from UTF-8 string comparison or `bcmp( "foo", '=',
"foo" )` for byte-wise comparison, which are both the same for equality.
Like symbols, we can use the alternate `"|...|"` syntax for multi-line strings
or strings containing a quote mark.

**Rec:** And of course, the record type.  Used for representing complex data structures
as a set of key-value fields.  Record constructors are represented by a set of
entries enclosed in curly braces `{...}`.  Keys can be explicit or implied,
and the two types of entries can be mixed (e.g `{ 1, 2, 3, .k1: 'foo' }`).
Implicit keys are assigned as integrals starting at 0 (on the left) and
increasing right-ward; intermediate explicitly keyed entries do not affect
this count.

**Cls:** The closure type.  These are higher order functions capable of closing around
their definition environment; and the only type of function Ten has.  Closure
constructores begin with a parameter list enclosed in square brackets `[...]`
followed by a single expression, which will be evaluated as the function's
result.

**Fib:** This is the type for fibers, which are a sort of lightweight threads which
don't execute in parallel.  Fibers will be given more detail later; they
have no language level construction syntax.

**Dat:** User data from the native environment, used for bridging between
the C (or native API) and Ten type systems.

**Ptr:** Similar to 'Dat', but represents only a pointer instead of a block of memory.

There are also a few other value types that Ten knows about, but they're only
really important for embedding Ten in another program, and shouldn't generally
be seen within the Ten VM, so I'll detail them in the embedding guide.

As might be noted, tuples aren't on this list, though we mentioned them earlier
on as being a part of the language.  Though many languages provide tuples as a
first class data type; Ten sticks with the 'record only' approach, and uses
tuples only as a means of grouping associated temporaries.  So tuples can group
a list of arguments to be passed to a function call, or a list of expression
returns, but they can't be put into variables or record fields, and tuple's
can't be nested.

## Conditionals
Though Ten borrows the traditional `if` and `else` keywords, its main
conditional syntax resembles LISP's `cond` form more than the more
common `if-else` chain; meaning the expression can have more than one
alternative:

    if
      n = 1: doSomething()
      n = 2: doSomethingElse()
      n = 3: doOtherThing()
    else doDefaultThing()

The alternative clauses will be attemted one at a time in order, until one
of the predicate expressions returns true; in which case the associated
consequent is evaluated and its result returned as the result of the
conditional.  If none of the predicates result in a truthful value, then
the `else` condition is evaluated as the default consequent.

Conditional predicates interpret any value except `false`, `nil`, and `udf`
as true; `false` and `nil` are seen as false; and `udf` causes an error if
passes as a predicate.

In addition to the main conditional form, Ten has a few binary forms; based
short circuit evaluation of logical operators in other languages.

The AND conditional evaluates its left expression, if it's seen as true
according to the same rule above then the second operand is evaluated and
returned as the result; otherwise the first is returned.

    def a: true &? 1  `returns 1
    def b: nil  &? 2  `returns nil

The OR conditional evaluates its left expression, and if seen as true,
returns its result; otherwise the right is evaluated and returned.

    def a: 123 |? 321  `returns 123
    def b: nil |? 321  `returns 321

And the FIX conditional evaluated its left expression, and if `udf`
then evaluates and returns whats on its right; otherwise returning
the left.

    def a: 123 !? 321  `return 123
    def b: udf !? 321  `return 321

Though these are based on short circuit operators from other languages,
Ten puts them in the category of conditionals due to their odd behavior
compared to other operators; and their loose definition of truthfulness
as compared to Ten's logical operators, which only work on Log types.

It's worth noting that, while Ten's conditional syntax may be a bit
unfamiliar to some; it's flexible enough that we can default back to
the more familiar format, since the else clause can take any expression.

    if thing = 1:
      ...
    else
    if thing = 2:
      ...
    else
      ...


## Code Blocks
Ten's code blocks are a bit of a hybrid between the `let-in` forms from more
functional languages and code blocks `{...}` from imperative languages.  The
syntax is a bit more functional styled, to make the return value of the block
explicit, but side effects are allowed, and not all expressions have to be
assignments.

    do
      def a: 1
      def b: 2
    for a + b

Code blocks take a list of expressions before the `do` and `for` keywords,
after the `for` keyword comes its return expression; which is in the same
scope as the block, so can make use of internal variables.  Code blocks
are mostly used as the bodies to functions or conditional consequents; but
they can also be used to make code more expressive and hygienic by grouping
related portions of code; for example a user prompt can be written cleanly
as:

    def answer: do show"What do you want? " for input()


## Signal Handlers
The original language Rig, which Ten succeeds, had no signals.  And since,
like Ten, it didn't have any sort of `return` keyword for early function
returns; we generally had to avoid designing code to make use of these
mechanics, or use fiber yields to the same effect, which isn't efficient.

Ten introduced signals and handlers to allow for these more flexible control
flow semantics without breaking the 'everything is an expression rule'.

    when
      done( val ): val
      fail( err ): panic( err )
    in
      do
        def foo: bar()
      for
        if foo:
          sig done: foo
        else
          sig fail: "I broke"

These basically serve as a form of hygienic goto, ensuring that the destination
expression can produce a valid return value.  They're also local to the current
function, so attempting to invoke unknown signals will cause a compilation
error.

## Operators
Ten supports most of the usual operators, and a few additional ones. All
binary operators are left associative; and all unary operators have the
same precedence, just above the `^` power operator.  Here's the table,
in order of precedence.

|   Operators                     |         Description                      |
|:-------------------------------:|:-----------------------------------------|
|    `@`, `.`                     | Record field access operators.           |
|                                 | Function calls, the 'invisible' operator.|
|      `^`                        | Power operator.                          |
| `~`, `-`, `!`                   | Unary operators.                         |
| `*`, `/`, `%`                   | Multiplication, division, modulo.        |
|    `+`, `-`                     | Addition, subtraction.                   |
|   `<<`, `>>`                    | Logical left and right shift.            |
| `&`, `\`, <code>&vert;</code>   | AND, XOR, OR bitwise and logical.        |
| `=`, `~=`, `!=`, `<`, `>`, `<=`, `>=` | Comparison operators.              |

A few things to note.

* The `.` path operators expects an identifier as its right operand, which will
  be converted to a symbol key.  So `.thing` is equivalent to `@'thing'`.
* The 'call operator' is actually the absence of an operator, any two
  consecutive primary or path expressions not separated by a delimiter or
  other operators are interpreted as a function call.  So `foo 'thing'` works
  just as well as `foo( 'thing' )`.
* The FIX operator `!` is unique to Ten, it's a unary operator which, given
  a `udf` value, returns `nil` instead; otherwise just returns its operand.
* We use `\` instead of `^` for XOR, since `^` is used for power.
* Logical and bitwise operations use the same operators (`~`, `&`, `\`, `|`),
  for Log operands they perform the logical operation with the logical values
  themselves, but for Int operands the operations are done on their bits.
* Shift operators are always logical, and negative shifts have the same effect
  as a positive shift in the opposite direction.
* The `!=` operator does not mean NOT EQUAL as in other languages, for that
  use `~=`.  It's used for comparison with `udf` values, since the ordinary
  comparison operators will throw an error when one of the operands is `udf`.
* Any operator except the call operator (non-operator) can have a (or more)
  delimiter between the operator and right operand, this allows expressions to
  be broken up into multiple lines.

All operators use strong typing, meaning they won't coerce values from one
type to another.  If two operands of different types are passed to an operator
(besides call and the path operators), or a value of an unexpected type is
given, then an error (not type coercion) occurs.

## Records
Records are the main factor that sets Ten apart from other, similar, languages.
Instead of being implemented as plain hashmaps, records make use of an
internal hashmap which can be shared between multiple instances; and maps
keys to a location within the record's value array.  These internal hashmaps
are called indices (type Idx) and the ability to share them between records
allows for massive memory savings as compared to other languages.

![Record Design](RecordDesign.png)

While this unique record design can allow for huge memory savings if used
correctly... used incorrectly they become a liability.  Since records can
share the same index and must allocate a value slot for each key in the index,
all peer records (with the same index) should keep a similar set of keys;
otherwise the records become unbalanced, and we have wasted slots.

![Unbalanced Record](RecordUnbalanced.png)

Generally speaking records are most effective if used as structs or objects
where a single index will be shared by a large number of records, instead of
as a large data set; in which only the one instance will exist.  More
more specialized collection types can be implemented in terms of records
(for example Ten has functions for dealing with LISP like linked-lists) or
as native data types through Ten's embedding and foreign code API.

Index sharing between records is based on where the records are constructed,
for example given a constructor:

    def r: { .a: 1, .b: 2, .c: 3 }

Any record built at this location, by this constructor, will share the same
index.  It's also possible to separate a record from its shared index, giving
it a copy of the original, to prevent any definitions added to it to affect
its peers.  The `sep()` function takes a single record and returns the same
record, setting an internal flag that marks it for separation.  Separation
is done lazily for the sake of efficiency, so the actual separation won't
occur until a definition is added to the separated record.

It's generally good practice for modules to call `sep()` on records returned
to the user, to prevent contamination of internal indices.

It's also worth noting that record will allocate the smallest number of
value slots possible at any time; so each new definition to the record's
index may require a full record resize... so field definition can be
significantly more expensive in Ten than in similar languages.

Now that the mechanics have been discussed, let's cover the actual syntax.

Record constructors are represented syntactically are key-value pairs
between `{...}` curly braces, with the standard delimiter rules.

    { .k1: 1, .k2: 2, @'k3': 3 }

Keys beginning in a `@` are given as identifiers, which are converted to
symbols; those beginning with `@` are given literally, or as primary
expressions.  Keys can also be omitted to let Ten assign them implicitly:

    { 1, 2, 3 }

Implicit keys are integrals, beginning at `0`, and assigned from left to right
increasing for each implicit key.  For example in the above record the value
`1` will have a key of `@0` and `2` will have a key of `@1`.  Implicit and
explicit keys can be combined in a single record constructor:

    { 1, 2, 3, .k1: 123, .k2: 321 }

Though it's generally good practice to put the implicitly keyed fields at
the start of the record, followed by the explicitly keyed ones; Ten doesn't
enforce this.  Intermediate explicit keys are ignored when assigning the
values of implicit keys.

Fields can be accessed with the same key syntax as used in their construction.

    def r: { 1, 2, 3, .k1: 123 }
    def a: r@0
    def b: r@1
    def c: r@2
    def d: r.k1

Since path operators are evaluated from left to right, chaining also works.

    def r: { .s: { .k: 123 } }
    def v: r.s.k

## Closures
Like many similar languages, Ten uses closures as its only type of function
or executable unit.  These are just first class functions (so they can be passed around like ordinary values) which can access variables from their
definition environment.

Unlike most other languages that do this, however, Ten has no alternate syntax
for function definition.  Functions are defined just by assigning a closure
(created with a closure constructor (lambda expression)) to a variable in the
usual way:

    def add: [ a, b ] a + b

Closure constructors begin with a parameter list enclosed in `[...]` square
brackets.  The usual delimiter rules apply, and the last parameter name
may be followed by an ellipsis `...` to indicate a variadic parameter.
The body of a closure is a single expression, which of course can be a
code block if more logic is needed.

    def add: [ args... ] (args@0 !? 0) + (args@1 !? 0) + (args@2 !? 0)
    def sum: add( 1, 2, 3 )

Extra positional arguments, with positions the same or greater than the
variadic parameter, are put into a new record in the variadic parameter's
variable.  For each call a new record is created, but they all share the
same index, so don't contaminate it.

The lack of a special 'function definition' syntax makes recursion difficult
since the function variable will be `udf` (undefined) at the definition point,
and so can't be captured by the closure.  Rig used a special keyword `rec` for
recursive definitions, but this was cumbersome and not very clean.  Ten instead
defines a special read-only `this` variable withing the scope of each closure,
which always contains the closure itself.  So we can use direct recursion like:

    def fibo: [ n ]
      if n < 2:
        n
      else
        this( n - 1 ) + this( n - 2 )

## Comments, Strings, and Symbols
Ten uses a consistent syntax for all three 'textual' units, just with different
quote types.  Comments use back quotes <code>&#96;</code>, strings use double quotes `"`, and symbols use single quotes `'`.

    `this is a comment`
    "this is a string"
    'this is a symbol'

An alternate syntax can also be used for each unit type, which is not line
terminated and allows for inclusion of quote characters within the text.

    `|this is a comment|`
    "|this is a string|"
    '|this is a symbol|'

The nice thing about this syntax is its consistency, but an added benefit
to allowing line terminated strings is that we can do things like:

    def text: cat(
      "this is a pretty long body of
      "text, but we take advantage of
      "line terminated strings to make
      "it a bit less cumbersome to write
      "without having to trim whitespace
      "from the beginning of each line
    )

Ten doesn't support any of the escape sequences common in other
languages, strings and symbols are completely literal.  To add
special character we'd use concatenation.

    def s: cat( "Line 1", N, "Line 2", N, "Line 3" )

The prelude defines the following variables for use in this way:

* `N` - `\n`
* `R` - `\r`
* `L` - `\r\n`
* `T` - `\t`

Other characters can be converted from their numeric values with `uchar()`.

## The Prelude
Ten's prelude is a builtin library of functions (and variables) which are
put directly into the global namespace.  In the original language (Rig)
the prelude was optional, but the language was really almost useless
without it; and I wanted to make Ten's prelude something that could be
relied on to be available in any context of the language, basically as
part of the language itself.

By necessity the prelude is quite small and only implements minimal
functionality since we need to keep the Ten runtime small and portable.
More extensive functionality is expected to be implemented in external
libraries, and Ten's official CLI (Command Line Interface) will include
a few of the more useful libraries as builtin libraries.

### Imports
To keep Ten as portable as possible, we've avoided reliance on a
the availability of a filesystem in the core language runtime, so
module imports are deferred to external implementation.  It is,
however, beneficial to have a common interface for package imports,
so that's what the prelude provides.

The `require()` and `import()` functions take a module identifier string
as input and try to load the respective module.  On failure `require()`
will cause a panic, and `import()` will just return `udf`.

    def csv: require"lib:csv"

Module identifiers have the form `type:path`, where the `type` tells
Ten which strategy to use for loading the module, and the path is
what's passed to the strategy itself to tell it which module to load.

An additional `loader()` function allows for the installation of a
custom module loading strategy.  This includes a type, loader,
and translator.

    def type:  ...
    def loadr: ...
    def trans: ...
    loader( type, trans, loadr )

The type must be a symbol that matches the `type` portion of the module ID,
indicating that this strategy should be used to load modules of that type.

The loader is a closure which takes a normalized module path as input,
and returns the loader module.  This can be any Ten value.  Normally
modules are represented as records with methods as fields, but textual
modules might be loaded as strings instead.

The translator is a closure which takes the module path as parameter, and
should return a normalized version of the path, or `udf` for a failed import.
The normalized version of of the path should be something that uniquely
identifies the module, like an absolute file path.  The loaded modules
will be cached with their normalized paths as keys, to avoid reloading
modules that have already been imported.  The translator is optional,
if omitted then the raw module path will be passed to the loader.

> **ten-load** [🔗](https://github.com/raystubbs/ten-load)
>
> This relatively featureless module system may seem uninspired, but it's
> done this way for the sake of portability, and really works quite well
> in conjunction with other projects.
>
> For example [ten-load](https://github.com/raystubbs/ten-load) which is
> included in the official CLI supports quite sophisticated module loading,
> with cycle detection and semantic versioning, shared object native
> modules, data/text modules, and language specific string modules.

### Misc
The prelude includes a few miscellaneous functions mostly unrelated
to the others.  These are described here.

The `type()` function takes any Ten value as input, and returns its
type name.  Type names are symbols that give, in most cases, the three
latter name of a type.  Some types can have extended names, for example
tagged records, pointers, or data objects.  Type names for these are
given as the normal type name followed by `:` and the tag or subtype
name.

    type( nil )             -> 'Nil'
    type( true )            -> 'Log'
    type( 123 )             -> 'Int'
    type( 1.2 )             -> 'Dec'
    type( 'a' )             -> 'Sym'
    type( NULL )            -> 'Ptr'
    type( "a" )             -> 'Str'
    type( {} )              -> 'Rec'
    type( { .tag: 'Tag' } ) -> 'Rec:Tag'
    type( data )            -> 'Dat:Tag'
    type( []() )            -> 'Cls'
    type( fiber[]() )       -> 'Fib'

The `expect()` function is related to `type()` in using the same type
name format.  It's used for type checking:

    def val: "Hello, World!"
    expect( "val", 'Str', val )

If `val` isn't of the expected type then the function will panic with
an error message telling the user that `"val"` is of the wrong type.
The first argument just tells the function what is being type checked,
so it can report it in the error.

The `assert()` function, as might be expected, asserts a condition.
This uses the same rules of truthfulness as Ten conditionals, so
`nil` and `false` are untrue, while everything else is true.  The
second argument is reported in in the panic message on assertion
failure.

    assert( thing == 123, "some assertion message" )


The `collect()` function forces a garbage collection cycle, it takes
no arguments and has no returns.

    collect()

The `clock()` function returns the current CPU clock time in seconds
as a Dec value.

    def start: clock()
    ...
    def delay: clock() - start

And the `rand()` function returns a random Int.

    def r: rand()

As mentioned in a previous section, the `sep()` function separates
a record from its index.

### Type Conversion
The prelude provides `log()`, `int()`, `dec()`, `sym()`, and `str()`
functions for explicit conversion between the languages atomic types...
and strings.  The `log()` function parses keyword values from symbol
and string types, or interprets `0` as false and anything else as true
for number types.  The `sym()` and `str()` functions use the
stringifacation of any of the other types as their returns.  And the
`int()` and `dec()` functions can parse their numbers from strings or
symbols, or convert `true` to `1` and `false` to `0` of the respective
number type.

    log( 1 )        -> true
    int"123"        -> 123
    dec'1.2'        -> 1.2
    str( 123 )      -> "123"

These all return `udf` if the argument can't be converted.

### Alternate Bases
Ten doesn't have any special syntax for representing bases other than
decimal.  Instead, since these types of numbers are genereally used
as constants, it should be sufficient to parse the values from strings
to be stored as constants for later use.  To this end the prelude offers
the `hex()`, `oct()`, and `bin()` functions for parsing strings in
the respecive base.  They all return `udf` for bad string formatting.

    hex"EEE"  -> 3822
    oct"666"  -> 438
    bin"111"  -> 7

### Iteration
Ten uses zero-parameter closures as iterators, so the iterator
constructors in the prelude all return these.  Iterators return
`udf` after their last value.

#### Record Iterators
The `keys()` constructor returns an iterator that traverses all the
keys in a record.  Effects on the iterator are undefined if the
record is modified after its creation.

The `vals()` constructor returns an iterator that traverse the values
of a record.  Again, effects are undefined if the record is modified
after its creation.

The `pairs()` constructor returns an iterator over the key-value pairs
of a record. Effects are undefined if the record is modified after
its creation.  For each call to the iterator, the key-value pair is
returned as a `( key, val )` tuple.

    def r: { ... }
    def v: vals( r )
    def k: keys( r )
    def p: pairs( r )

#### Sequence Iterators
The `seq()` function creates a sequence iterator, which traverese the
arguments passed to the constructor.

    def s: seq( 1, 2, 3 )
    assert( s() = 1, "" )
    assert( s() = 2, "" )
    assert( s() = 3, "" )
    assert( s() != udf, "" )

#### String Iterators
Ten strings are can represent anything, they're just strings of bytes.
But they're often used to represent text, so we have two iterator types:
character iterators and byte iterators.  The `chars()` constructor
creates a character iterator over the string, which return each UTF-8
character as a symbol.  And the `bytes()` constructor builds a byte
iterator, which traverses the bytes of a string, returning each as
an Int with its byte value.

#### List Iterators
The `items()` iterator traverses a LISP like list of cons cells, made
up of records of the form: `{ .car: val, .cdr: rest }`.

    def i: items{ .car: 1, .cdr: { .car: 2, .cdr: { .car: 3, .cdr: nil } } }

#### Range Iterators
The `irange()` and `drange()` functions build iterators to traverse a range
of numbers given a start, end, and optional step offset.  As may be obvious,
the `irange()` iterator returns Int values, and `drange()` iterator returns
Dec values.

    def i: irange( 1, 10 )
    def d: drange( 0.0, -50.0, -0.5 )

Both iterator types include the start, but not the end number in their traversal.

### User Interaction
The prelude provides `show()`, `warn()`, and `input()` functions for
interacting with the user through the standard IO streams.  The `show()`
function prints the concatenation of its arguments to stdout, while
the `warn()` function does the same to stderr.  The `input()` function
reads a line of user input as a string.

    show( "Tell me something...", N )
    def answer: input()

### Unicode Conversion
We represent characters as UTF-8 encoded symbols in Ten, but sometimes
we may need to convert to or from the unicode character number.  To
do this we have the `uchar()` function, which converts an Int character
code to its respective UTF-8 symbol, and `ucode()` which converts the
other way, from symbol to code.

    ucode'ぁ'        -> 12353
    uchar( 12353 )   -> 'ぁ'


### String Utilities
The prelude's `cat()` function takes some number of values, and
concatenates their stringified forms into a single output string.

    cat( "Hello, ", "World", '!' ) -> "Hello, World!"

The `join()` function is similar, but instead of taking the strings
as arguments, it expects an iterator, from which it'll take all the
values to concatenate into the output string.

    cat( seq( 1, 2, 3 ) ) -> "123"

Since Ten strings can represent either text, or any other type of
serialized data, we need two comparison functions, one for comparing
by character and another for byte-wise comparison.  The `ccmp()`
function compares strings by UTF-8 character, and `bcmp()` compares
their bytes.  Both of these take their comparison operator as a symbol:

    ccmp( "ぁ", '<', "ぃ" )   -> true
    bcmp( "abc", '>', "cba")  -> false

### Looping
Since Ten doesn't have any language level loop forms, the prelude
provides two of the more common loop types as functions.

The `each()` function loops over its input iterator, calling the
body closure for each value.

    each( seq( 3, 2, 1 ), [ n ] show( n, " beers", N ) )

The `fold()` function aggregates a sequence of values into a
single return.  On its first iteration it combines the aggregate
with the first value in the sequence with the aggregator callback,
and subsequent iterations combine the result of the last iteration
with the next value from the iterator.

    def sum: fold( seq( 1, 2, 3 ), 0, [ agr, nxt ] agr + nxt ) -> 6


### List Utilities
Since Ten's record design doesn't do well with larger collections of
data, the prelude provides some functions for dealing with data
in LISP style linked lists, composed of cons cells.  And thanks to
Ten's shared indices, these cells can be represented relatively
efficiently as records.

The `cons()` function just takes two values (the car and cdr) and
combines them into a list cell of the form: `{ .car: val, .cdr: rest }`.

The `list()` function takes a sequence of values as arguments, and combines
them into a list of cells in the above fashion.

The `explode()` function creates a list from the values of an iterator.

    cons( 1, cons( 2, nil ) ) -> { .car: 1, .cdr: { .car: 2, .cdr: nil } }
    list( 1, 2 )              -> { .car: 1, .cdr: { .car: 2, .cdr: nil } }
    explode( seq( 1, 2 ) )    -> { .car: 1, .cdr: { .car: 2, .cdr: nil } }


### Fiber Utilities
Ten's fibers are like little threads that can't execute in parallel.  They
allow us to separate the execution state (including the errors) of different
tasks to keep issues from affecting the rest of the program, or to allow the
task to be paused and continued freely.

The `fiber()` function creates a new fiber, given an 'entry closure', which
represents the task to be executed.

    def fib: fiber[ a, b ] a / b

Optionally, and for the sake of error reporting, a fiber can be given a tag:

    def fib: fiber( [ a, b ] a / b, 'myFiber' )

The tag should be a word or name to describe the fiber, it'll be given in
the stack trace when an error occurs in the fiber.

The `cont()` function starts or continues the execution of a fiber, and allows
us to pass a list of arguments as continuation arguments.  For the first
continuation these will serve as the arguments to the closure wrapped by
the fiber; but for subsequent continuations they're returned by the `yield()`
call that had paused the fiber's execution.

    cont( fib, { 1, 0 } )

This will call `[ a, b ] a / b` with `a = 1` and `b = 0`, so a division by
zero error will occur, failing the fiber.  The fiber's current state can be
retrieved as a symbol with the `state()` function:

  state( fib ) -> 'failed'

The state function returns one of the following:

* `'running'`  - The fiber is the current one executing.
* `'waiting'`  - The fiber was executing, but paused to continue the current one.
* `'stopped'`  - The fiber has been paused with a call to `yield()`, or was never continued.
* `'finished'` - The fiber has finished its task, its closure has returned.
* `'failed'`   - The fiber failed, some kind of error occured while executing.

The `yield()` function, called from within a fiber continuation, allows
us to pause execution of the fiber until it's continued again; possibly
with more execution arguments.

    def fib: fiber[ p1 ]
      do
        show( "Received ", p1, N )
        def p2: yield()
        show( "Received ", p2, N )
      for()

    cont( fib, { 1 } )
    cont( fib, { 2 } )

This prints:

    Received 1
    Received 2

The yield value can also take arguments, which are returned by the `cont()`
function:

    def fib: fiber[] yield( 123 )
    def val: cont( fib, {} )
    assert( val = 123, "" )


The `errval()` and `trace()` functions return the value of whatever
error caused the fiber to fail (`udf` if the fiber isn't failed) and
the stack trace produced by the failure.  The stack trace is given
as a record of the form: `{ { .fiber: 'myFib', .file: 'myScript.ten', .line: 10 }... }`, with one entry per stack level; it even produces
entries for functions called by C code.

### Compilation
The `script()` and `expr()` functions allow for compiling code at
runtime.  The `script()` function compiles its input string, which
can consist of one or more expressions delimited as usual, into a
closure that returns no values (an empty tuple).  The `expr()`
function on the other hand compiles a single expression from the
start of the string, whose evaluation is returned as the closure's
return value.

Both functions accept a record of upvalues along with the string
to be compiled, these are bound to the closures free variables of
the same names.

    def e: expr( { .a: 1, .b: 2 }, "a + b" )
    assert( e() = 3, "" )

    def r: {}
    def s: script( { .r: r }, "def r.v: 123 " )
    assert( r.v = 123, "" )

## That's All Peeps
That about sums up the language in 1K lines of markdown. It's
quite simple, but also really practical.  Besides its simplicity Ten
has also been designed from the ground up to be embedded into other
applications, so check out the (TODO)[embedding guide](EmbeddingGuide.md)
if you'd like to see how that's done.
