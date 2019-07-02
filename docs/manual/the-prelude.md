# <a name="4">4 - The Prelude</a>
This section of the manual describes Ten's prelude library,
which consists of a small set of builtin functions that are
loaded into the global scope of every Ten instance.

This library provides only the bare essentials needed to
make Ten useful without compromising portability; anything
extra can, and should be, provided by third pary libraries.
Ten will never ship with a large standard library, the
runtime is small and will stay that way, and that's what
makes Ten appealing as an embeddable language.

## <a name="4.1">4.1 - Type Checking</a>
The nature of dynamic languages in general allows us to ignore
a lot of details regard which types we're working with in many
situations.  But while Ten is a dynamic language, it's generally
still good practice to do proper type checking where possible,
at least at interface boundaries; so the prelude provides a few
utilities for doing just this.

### <a name="fun-type">`type( val )`</a>
Returns the type of a value as its three letter abbreviation
in a symbol.  Possible values include: `Udf`, `Nil`, `Log`, `Int`,
`Dec`, `Sym`, `Ptr`, `Str`, `Idx`, `Rec`, `Fun`, `Cls`, `Fib`,
`Dat`.

In addition to these, some types (records, datas, and pointers)
can be tagged with additional type information.  If the given
value is tagged thusly then the type is returned as `BASE:TAG`,
where `BASE` is one of the above type names and `TAG` is whatever
the value is tagged with.  For records, the tag is just given
as a special field `.tag`, so for example:

    $ type{ .tag: "ARecordThing" }
    : 'Rec:ARecordThing'

### <a name="fun-expect">`expect( what, type, val )`</a>
If `type` only specifies a base type then ensures that `val` has
the base type specified, so for example if `type = 'Rec'` and
`type( val ) = 'Rec:ARecordThing'` then the test would still pass.

If `type` specifies both a base type and a tag, then the function
ensures that the `type`, and `type( val )` are exactly equal.

If the type check fails then the function panics with a message
mentioning that `what` has the wrong type.  For example:

    $ def val: 123
    : udf
    $ expect( "val", 'Dec', val )
    Error: Wrong type Int for 'val', need Dec
      unit: ???        line: 1    file: <REPL>

## <a name="4.2">4.2 - Type Conversion</a>
These functions allow for type conversion between atomic types,
with the `str()` and `sym()` functions allowing for conversion
from any type, since it's just a matter of stringification; though
most object types are just stringified to a modified form of their
type name.

### <a name="fun-log">`log( val )`</a>
Convert an atomic value to a `Log` value.  This doesn't use
the rules of truthiness that Ten's conditionals use; instead
conversion from a string or symbol is only allowed if their
contents are either `true` or `false` exactly, in which case
the value is just parsed to the respective value.  For number
types `0` is converted to `false` and anything else to `true`.
Other types can't be converted to `Log` via this function.

Returns `udf` if the given value couldn't be converted.

### <a name="fun-int">`int( val )`</a>
Convert an atomic value to an `Int` value.  `Log` values are
converted to `1` if `true` or `0` if `false`.  `Dec` values
are directly cast, with an undefined result if the value is
too large.  Strings and symbols are parsed for an integral,
with undefined behavior if the parsed number is too large.
Any other types can't be converted with this function.

Returns `udf` if the given value couldn't be converted.

### <a name="fun-dec">`dec( val )`</a>
Convert an atomic value to a `Dec` value.  `Log` values are
converted to `1.0` if `true` or `0.0` if `false`.  `Int`
values are directly cast.  Strings and symbols are parsed
with an undefined result if the parsed number is too large
or too small.

Returns `udf` if the given value couldn't be converted.

### <a name="fun-sym">`sym( val )`</a>
Convert a Ten value to a `Sym` value.  This just stringifies
the value with the runtime's internal formatter, and wraps it
in a symbol.

### <a name="fun-str">`str( val )`</a>
Convert a Ten value to a `Str` value.  This just stringifies
the value with the runtime's internal formatter, and wraps it
in a string object.

## <a name="4.3">4.3 - Number Parsing</a>
Since Ten doesn't support alternate bases for number literals,
they need to be parsed from string via one of the following.

### <a name="fun-hex">`hex( str )`</a>
Parse a hexadecimal string to one of Ten's number types.  If
the given string includes a decimal point `.` and can be parsed
as a number then it'll be parsed as a `Dec`.  If it doesn't include
a `.` but can be parsed as an integral, then is parsed as an `Int`.
Panics if the parsed integral is too large or too small.  Returns
`udf` if the string includes invalid characters.

### <a name="fun-oct">`oct( str )`</a>
Parse a octal string to one of Ten's number types.  If the given
string includes a decimal point `.` and can be parsed as a number
then it'll be parsed as a `Dec`.  If it doesn't include a `.` but
can be parsed as an integral, then is parsed as an `Int`.  Panics
if the parsed integral is too large or too small.  Returns `udf`
if the string includes invalid characters.

### <a name="fun-bin">`bin( str )`</a>
Parse a binary string to one of Ten's number types.  If the given
string includes a decimal point `.` and can be parsed as a number
then it'll be parsed as a `Dec`.  If it doesn't include a `.` but
can be parsed as an integral, then is parsed as an `Int`.  Panics
if the parsed integral is too large or too small.  Returns `udf`
if the string includes invalid characters.

## <a name="4.4">4.4 - Iteration</a>
Since Ten doesn't have any language level loops, the prelude provides
functions for some of the more common tasks.  These traverse the
given iterators, which are represented as zero parameter closures that
should return the next value (or values) of a stream on each call,
or all `nil` values once the full stream has been consumed.

In addition to the looping functions, the prelude provides constructors
for various types of iterators and utilities for trimming the edges of
an iterator.

### <a name="fun-each">`each( iter, what )`</a>
Traverses `iter`, for each set of values calls `what` with the
values passed as arguments.  The arity of `what` should match
the number of values returned by each call to the iterator.

    each( irange( 0, 100 ), [ int ] show( int, N ) )

### <a name="fun-fold">`fold( iter, agr, how )`</a>
Aggregates the values of `iter` into a single return value.  If the
iterator is empty then returns `agr`; otherwise the first call to
`how` is passed `agr` along with the first value of the stream, and
from there on (for each additional value of the iterator), the
`how` function is called with its last return passed as the first
argument and the next stream value as its second.  The function
returns that result of its last call to `how`.

### <a name="fun-skip">`skip( iter, num )`</a>
Given an `iter`ator, this function returns an equivalent iterator
`num` values ahead of the given one.  The function _may_ create and
return a new iterator, or may return the same iterator after discarding
the first `num` values.  So the original reference to `iter` should
be discarded after this call.

### <a name="fun-limit">`limit( iter, lim )`</a>
Given an `iter`ator, this function returns an equivalent iterator
consisting of only the next `lim` values.  The function _may_ create
and return a new iterator, or may modify and return the given `iter`
so references to the original iterator should be discarded after
this call.

### <a name="fun-keys">`keys( rec )`</a>
Constructs a key iterator over a record.  If the record is modified
before such an iterator is fully consumed, then behavior is undefined
for the rest of the traversal.

### <a name="fun-vals">`vals( rec )`</a>
Constructs a value iterator over a record.  If the record is modified
before such an iterator is fully consumed, then behavior is undefined
for the rest of the traversal.

### <a name="fun-pairs">`pairs( rec )`</a>
Constructs a pair iterator over a record.  If the record is modified
before such an iterator is fully consumed, then behavior is undefined
for the rest of the traversal.  Each call to the iterator returns a
`( key, val )` tuple.

### <a name="fun-seq">`seq( vals... )`</a>
Constructs an iterator over the given arguments.

### <a name="fun-rseq">`rseq( { vals... } )`</a>
An alternative to `seq()` without the size limitation of tuples,
pass the values in a record instead of a tuple.

### <a name="fun-bytes">`bytes( str )`</a>
Constructs a byte iterator over the given string.  Each call
returns the next byte in the string as an `Int`.

### <a name="fun-chars">`chars( str )`</a>
Constructs a character iterator over the given string.  Each
call returns the next UTF-8 character in the string as a symbol;
the iterator will panic if it encounters invalid UTF-8 formatting.

### <a name="fun-split">`split( str, sep )`</a>
Constructs an iterator over the segments of a string separated
by `sep`.  For each call returns the next substring.

### <a name="fun-items">`items( list )`</a>
Constructs an iterator over the given LISP styled linked list,
represented as `{ .car: val, .cdr: rest }`.

### <a name="fun-drange">`drange( start, end ? step )`</a>
Constructs a decimal range iterator which traverses the numbers
from `start` to `end`, advancing by `step` for each call.  If
`step` is omitted then defaults to `1.0` or `-1.0`, whichever
advances toward `end`.  Panics if `step` is given but doesn't
advance toward `end`.

### <a name="fun-irange">`irange( start, end ? step )`</a>
Constructs a integral range iterator which traverses the numbers
from `start` to `end`, advancing by `step` for each call.  If
`step` is omitted then defaults to `1` or `-1`, whichever
advances toward `end`.  Panics if `step` is given but doesn't
advance toward `end`.

## <a name="4.5">4.5 - Strings and Text</a>
While Ten's strings are really just immutable byte arrays, one of
their most common applications is to represent text.  So while the
prelude provides functions for raw string manipulation regardless
of text encoding; it also provides a few function for dealing with
strings as text, more specifically, as UTF-8.  Besides those functions
covered here, also check out the constructors for the following string
iterators:

* [`bytes( str )`](#fun-bytes)
* [`chars( str )`](#fun-chars)
* [`split( str, sep )`](#fun-split)


### <a name="fun-ucode">`ucode( chr )`</a>
Convert a UTF-8 character symbol to its unicode index.

### <a name="fun-uchar">`uchar( int )`</a>
Convert a unicode index to its UTF-8 character symbol.

### <a name="fun-cat">`cat( vals... )`</a>
Stringify and concatenate the given values.

### <a name="fun-join">`join( iter, sep )`</a>
Stringify and concatenate all the values of the given `iter`ator,
inserting `sep` between each pair of adjacent values.

### <a name="fun-bcmp">`bcmp( str1, opr, str2 )`</a>
Compare two strings as byte arrays.  Returns `true` or `false`.
The `opr` should be a comparison operator within a symbol, any
such operator except `!=` can be passed.

    $ bcmp( "abc", '<', "cba" )
    : true

### <a name="fun-ccmp">`ccmp( str1, opr, str2 )`</a>
Compare two strings as UTF-8 text.  Returns `true` or `false`.
The `opr` should be a comparison operator within a symbol, any
such operator except `!=` can be passed.

    $ ccmp( "ぁ", '<', "ぃ" )
    : true

### <a name="fun-bsub">`bsub( str, n )`</a>
If `n > 0` takes the first `n` bytes of the string, if `n < 0`
then takes the last `-n` bytes; copying them into a new string.
If `n = 0` then just returns an empty string.

### <a name="fun-csub">`csub( str, n )`</a>
If `n > 0` takes the first `n` UTF-8 characters of the string,
if `n < 0` then takes the last `-n` characters; copying them into a
new string. If `n = 0` then just returns an empty string.  May
panic if the given string isn't UTF-8 encoded.

### <a name="fun-blen">`blen( str )`</a>
Returns the length of a string in bytes.

### <a name="fun-clen">`clen( str )`</a>
Returns the length of a string in UTF-8 characters.

### <a name="var-N">`N`</a>
The ASCII line feed character as a symbol.

### <a name="var-R">`R`</a>
The ASCII carriage return character as a symbol.

### <a name="var-L">`L`</a>
The ASCII carriage return character, followed by the line feed character
as a symbol.

### <a name="var-T">`T`</a>
The ASCII horizontal tab character.

## <a name="4.6">4.6 - Linked Lists</a>
Ten's only data structuring type is the record, but records are designed
to be relatively efficient with memory; so can be used freely as nodes
in linked structures.  The prelude provides a few utilities for creating
and manipulating LISP style linked lists, with nodes of the form
`{ .car: val, .cdr: rest }`.

### <a name="fun-cons">`cons( car, cdr )`</a>
Creates a cons node `{ .car: car, .cdr: cdr }` with the same
index used in the nodes generated by `list()` and `explode()`.

### <a name="fun-list">`list( vals... )`</a>
Organizes the given values into a list of the above-mentioned form.

### <a name="fun-explode">`explode( iter )`</a>
Explodes the given `iter`ator into a list of the above-mentioned form.

## <a name="4.7">4.7 - User IO</a>
The prelude doesn't provide file IO to maintain portability, but
it does offer a few functions for interacting with the standard
IO streams; which should be functional in some form or other on
most systems, even those without any filesystem support.

### <a name="fun-show">`show( vals... )`</a>
Stringify and output the given `vals` to `stdout`.

### <a name="fun-warn">`warn( vals... )`</a>
Stringify and output the given `vals` to `stderr`.

### <a name="fun-input">`input()`</a>
Read a line of input from `stdin`.

## <a name="4.8">4.8 - Fibers</a>
Functions for creating and manipulating fibers.

### <a name="fun-fiber">`fiber( cls ? tag )`</a>
Create a fiber with entry point `cls`.  If a `tag` is provided then
it'll be printed in each stack frame of the fiber's trace.

### <a name="fun-cont">`cont( fib, { args... } )`</a>
Continua a fiber with the given `args`.

### <a name="fun-yield">`yield( vals... )`</a>
Yield the current fiber with the given values.

### <a name="fun-yield">`panic( err )`</a>
Fail the current fiber with the given error value.

### <a name="fun-state">`state( fib )`</a>
Return the given fiber's current state as one of:

* `running`
* `waiting`
* `stopped`
* `failed`
* `finished`

### <a name="fun-errval">`errval( fib )`</a>
Return the fiber's current error value.

### <a name="fun-trace">`trace( fib )`</a>
Return the fiber's current stack trace as a record of the form
`{ { .unit: FIBER_TAG, .file: SOURCE_FILE, .line: LINE_NUMBER }... }`.

## <a name="4.9">4.9 - Records</a>
Record utilities.

### <a name="fun-sep">`sep( rec )`</a>
Marks the given record for separation from its index.  See
[2.2 - Records](basic-concepts.md#2.2).

## <a name="4.10">4.10 - Compiling</a>
These are functions for runtime code compilation.

### <a name="fun-script">`script( upvals, script )`</a>
Compile the given `script` string as a Ten script, binding the given
`upvals` to the closure's free variables.

### <a name="fun-expr">`expr( upvals, expr )`</a>
Compile the given `expr` string as a Ten expression, binding the given
`upvals` to the closure's free variables.

## <a name="4.11">4.11 - Modules</a>
Since Ten avoids depending on file IO, it doesn't, and can't, implement
module loading itself.  Instead the prelude provides a standard
interface for module imports, but leaves it to the user or third party
software to implement the actual loading.  Check out the [Ten Module
Loader](ten-module-loader.md) section of this manual, which details
the use of `libtml`, the module loader used in Ten's official command
line interface.

### <a name="fun-require">`require( mid )`</a>
Import the module matching the given `mid`.  The `mid` (module ID)
should be given as a string of the form `TYPE:NAME`, where `TYPE`
gives the module path, which is used in determining which loader to
employ to open the module, and `NAME` gives some arbitrary module
name or path.  Panics if the module can't be loaded.


### <a name="fun-import">`import( mid )`</a>
Import the module matching the given `mid`.  The `mid` (module ID)
should be given as a string of the form `TYPE:NAME`, where `TYPE`
gives the module path, which is used in determining which loader to
employ to open the module, and `NAME` gives some arbitrary module
name or path.  Returns `udf` if the module can't be loaded.


### <a name="fun-loader">`loader( type, loadr ? trans )`</a>
Installs a module loader to tell Ten how to load a module of type
`type` (a symbol).  The `loadr` should be given as a closure that
accepts a module name as input.  The optional `trans` closure can
be provided to normalize a given module ID before attempting an
import; since Ten caches modules based on their module ID, this allows
multiple different module IDs that refer to the same thing be normalized
into a common form before loading.

## <a name="4.12">4.12 - Pipelining</a>

### <a name="fun-pipe">`pipe( val, pipeline... )`</a>
Pipes `val` through the given pipeline of closures, returning the result
of the last closure in the line.

    def result: pipe( 1, [ v ] v + 1, [ v ] v * 2 )
    result -> 4

### <a name="fun-rpipe">`rpipe( val, { pipeline... } )`</a>
This does the same as [`pipe()`](#fun-pipe) except the pipeline
is passed as a record instead of variadic arguments.  Note that
the given record might not be copied, so any subsequent changes
may be reflected in the pipeline; so the given record shouldn't
be modified after being passed to `rpipe()`.

### <a name="fun-pump">`pump( iter, pipeline... )`</a>
This function is similar to [`pipe()`](#fun-pipe) but instead of
passing a single value through the pipeline, it wraps an iterator
with the line to create a new iterator with additional middleware.

The resulting iterator lazilly processes each value of the given
`iter`, passing the value through the pipeling and returning the
final result.  If at any point in the pipeline a closure returns
`udf` for a value, that value is dropped from the flow and won't
be processed by the rest of the line, instead the `iter` will be
re-invoked and the new value passed through the line; effectively
filtering out the previous value.

The returned pump iterator will consume values from `iter` either
until `nil` is returned by the original iterator, or the last
closure in the pipeline returns `nil`, since iterators must continue
to return `nil` after the first.  Note that this means that any but
the first closure in the pipeline may receive a `nil` value,
and that any but the last may return a `nil` value without terminating
the stream; but this should generally be avoided to prevent confusion.

### <a name="fun-rpump">`rpump( iter, { pipeline... } )`</a>
This does the same as [`pump()`](#fun-pump) except the pipeline
is passed as a record instead of variadic arguments.  Note that
the given record might not be copied, so any subsequent changes
may be reflected in the pipeline; so the record shouldn't be
modified after being passed to `rpump()`.

## <a name="4.13">4.13 - Misc</a>

### <a name="fun-assert">`assert( cond, str )`</a>
Panics if the given condition is falsey.  The `false` and `nil` values
are interpreted as falsey, and any others as truthy.

### <a name="fun-rand">`rand()`</a>
Return a pseudo random number between `0.0` and `1.0`.

### <a name="fun-clock">`clock()`</a>
Return the current CPU time in seconds.

[Next: 5 - The API](the-api.md)
