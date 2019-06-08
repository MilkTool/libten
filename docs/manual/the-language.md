# <a name="3">3 - The Language</a>
This section describes the syntax and semantics of Ten, excluding
the various functions available in its prelude; which while also
an inherent part of the language, are detailed in a separate section.

Syntax will be explained in terms of an
[Extended Backus-Naur Form](https://en.wikipedia.org/wiki/Extended_Backus%E2%80%93Naur_form)
(EBNF), with `{...}` indicating a pattern that can be repeated
zero or more times, `[...]` indicating an optional pattern, and
`(...)` a single required instance of the nested pattern.

We use `&` in place of `,` or whitespace for concatenation, but keep
`|` for alternation.  Additionally we use `:` to indicate application
of the generic or template pattern on the right, to the pattern on the
left, and in definitions to indicate such a pattern.  For example:

    some: pat = (pat){pat}

Defines a template for one or more instances of `pat` in sequence,
and can be applied as:


    as = some: "a"

We'll also use `?...?` to provide textual description of a pattern
in cases where such a description would save space and time, without
a significant cost to understanding.

## <a name="3.1"> 3.1 - Tokens</a>
Ten defines the following keywords, which are reserved and thus cannot
be used as identifiers in any context:

    keyword = "def"  | "set" | "sig" | "do"  | "for"  | "if" | "else"
            | "when" | "in"  | "udf" | "nil" | "true" | "false"

The following tokens also have special meaning in the language.

    LF      = ? ASCII character 10, line feed or new line ?
    special =  "("  |  ")"  |  "{"  |  "}"  |  "["  |  "]"  |  ","  | LF
            |  "@"  |  "."  |  "^"  |  "~"  |  "!"  |  "*"  |  "/"  | "%"
            |  "+"  |  "-"  |  "<<" |  ">>" |  "&"  |  "|"  |  "\"  | "<"
            |  ">"  |  "<=" |  ">=" |  "!=" |  "~=" |  "="  |  "&?" | "|?"
            |  "!?" |  "..."

And other ASCII whitespace characters, besides line feed, are
significant only as padding between other tokens, and is otherwise
ignored.

Identifiers can consist of any contiguous sequence of word
characters not beginning in a digit, not included in the set
of keywords, and followed by a non-word character.

    alpha_chr = ? any ASCII alphabetic character ?
    digit_chr = ? any ASCII decimal digit character ?
    word_chr  = alpha_chr | digit_chr | "_"
    word      = (alpha_chr | "_") & { word_chr }
    ident     = word - keyword


The singleton values `udf` and `nil` along with the logical values
`true` and `false` are represented literally as their respective
keywords.

    nil = "nil"
    udf = "udf"
    log = "true" | "false"

Integral literals are given as a sequence of digits, and decimals
as the same with an intermediate or trailing decimal point `.`.
Underscores can also be inserted after the first digit for formatting.

    int = digit_chr & { digit_chr | '_' }
    dec = int & "." & { digit_chr | '_' }

While the above are Ten's syntactic rules for these literals, there's
an additional constraint on the size of the represented value; the
compiler will fail if the given value won't fit in the respective number
type.

Symbols, strings, and comments have similar syntax, varying only in
the type of quote used; this generic syntax can be defined as:

    utf8_chr          = ? any UTF-8 character ?
    quoted: quote_chr = quote_chr & { utf8_chr - quote_chr } & ( quote_chr | LF )
                      | quote_chr & "|" & { utf8_chr - "|" | "|" & ( utf8_chr - quote_chr ) } & "|" & quote_chr

This basically means that a `quoted` generic can be any sequence of
characters between the opening `quote_chr` and either a closing
`quote_chr` or a line feed (new line) character; or it can begin in
a `quote_chr` followed by `|`, in which case it'll only terminate
in `|` followed by `quote_chr` and can contain newlines and `quote_chr`
characters.

And we define:

    sym     = quoted: "'"
    str     = quoted: '"'
    comment = quoted: "`"

So symbols are quoted with single quotes `'`, strings with double
quotes `"`, and comments with back quotes <code>&#96;</code>. The
quoting characters aren't included in the symbol and string contents,
and comments are ignored in full.  So the following are equivalent
symbols.

    'hello'
    'hello
    '|hello|'

Ten doesn't have string escape sequences as most other languages do,
the quoted contents are taken completely literally.

## <a name="3.2"> 3.2 - Sequences</a>
Ten, unlike most languages, is very consistent about delimiters
and how they're used.  A delimiter can be either `,`, `LF`, or
some combination of the two.

    delim = ("," | LF) & {"," | LF}

So commas and newlines are equivalent in all contexts, though
we'll conventionally only ever use up to one comma, while we
may use multiple newlines for styling.

We use the above delimiters for any syntax form that contains
a sequence of other forms, and in any such sequence 'padding'
delimiters are allowed to appear at the start or end of the
sequence; though they're only required in between elements.

In addition, sequences will always be enclosed between two
other tokens; so we can define the following generic form:

    seq: start sub end = start & [delim] & {sub & delim} [sub] & end

This form will be used extensively in defining the rest of the
language as most other forms consist of some kind of sequence.

## <a name="3.3">3.3 - Variables</a>
Variables are reserved places in memory where values can be stored.
Since Ten is dynamically typed, variables are untyped, so any value
can be put in any variable, regardless of type.

Ten has four types of variables: global variables, local variables,
closed variables, and record fields.

A single identifier can denote a reference to a global, local, or closed
variable, depending on lexical context:

    ref = ident

Record fields are referenced via path expressions, which consist
of a call expression (which should evaluate to a record), followed
by a chain of keys.

    key  = "." & [delim] & ident | "@" & [delim] & primary
    path = call & key & {key}

Where `primary` is defined as:

    const   = nil | udf | int | dec | sym | str
    primary = const | ref | tup | rec | cls | block | cond | switch

Keys tell Ten _what_ field to reference in the record.  Fields can
be keyed by any type of value except `udf` if given as a primary
expression after `@`.  For the sake of convenience a key can also
be given as an identifier after `.`, in which case the identifier
is quoted implicitly as a symbol, so `.ident` is equivalent to
`@'ident'`.

Global variables are those variables accessible in any context of
the program, unless overshadowed by a local or closed variable.
Prelude functions are defined as globals, and REPL implementations
will define variables in the root scope as globals, but the global
pool isn't writable in most contexts.  The compilation scope
determines whether root level variables in a given chunk of code
will be defined as globals, and this is controlled by Ten's native
API and the host application, but generally speaking most code
shouldn't be compiled with a global scope, so it can read global
variables, and mutate their values, but can't define new ones.

Local variables are any variables defined in function or block
before being referenced; up until these variables are captured
by a closure.  Ten variables are lexically scoped, so any variables
defined in the context where a closure is created, can be accessed
by the closure; but this binding of the variable to a closure
changes its type.

Any variables captured by a closure, that is, variables that are
referenced without being defined locally, become closed variables.
These can start out as either globals or locals, but once referenced
they're moved into a 'box' on the heap (dubbed an upvalue), and
from then on are shared between the defining scope and the scopes
of any capturing closures.

Any variables referenced before they're defined, either in the
immediate local scope or a parent context, expand to `udf`; so
non-existent variables are semantically equivalent to existing
variable defined to `udf`.  Likewise, defining an existing
variable to `udf` effectively 'deletes' it, making it inaccessible
until redefined, and preventing capture by any closures, which don't
capture undefined variables.

A given variable name can either be redefined or mutated to change
its effective value.  Redefining a variable replaces the current
variable (not its value), severing any connection the old variable
had with capturing closures, and reverting it back to a local or
global (depending on definition scope).  If the defined variable
doesn't exist in the immediate scope (not a parent scope) then
a new variable is just defined in the immediate scope, and any
variables in the parent scope are left unchanged; so child scopes
can't define or redefine variables in parent scopes.  Mutations can,
however, reach across child-parent scope boundaries and change the
value of parent variables, but can't redefine parent variables or
undefine.

Definitions and  mutations have similar syntactic forms, just different
keywords:

    def = "def" & [delim] & dst_pattern & ":" & [delim] & expression
    set = "set" & [delim] & dst_pattern & ":" & [delim] & expression

The `dst_pattern` determines what variable, or variables, will
be defined or mutated.  This is perhaps the most complex facet
of Ten's syntax.

    dst_var      = ident | ident & "..."
    dst_key      = key | key & "..."
    dst_var_pair = ident & ":" & [delim] & key | ident | ident & "..."
    dst_key_pair = key & ":" & [delim] & key | key | key & "..."
    dst_pattern  = ident
                 | seq: "(" dst_var ")"
                 | seq: "{" dst_var_pair "}"
                 | primary & key
                 | primary & seq: "(" dst_key ")"
                 | primary & seq: "{" dst_key_pair "}"

So we can either do single assignments for variables or record
fields:

    def myVar:       123
    def myRec.myKey: 123

Or we can use a tuple pattern:

    def ( a, b ):         ( 123, 321 )
    def myRec( .a, .b ):  ( 123, 321 )

Which allows for multiple definitions at once by pattern matching
against a tuple.  This form can also have a variadic element:

    def ( a, b, r... ):         ( 123, 321, 567, 765 )
    def myRec( .a, .b, .r... ): ( 123, 321, 567, 765 )

Any extra values in the tuple will be put in a new record, which
will be assigned to `r`.  The new record will give the extra
fields keys starting with `@0` and incrementing for each additional
value added.  For mutations (`set`s) the variadic record will still
be created, but the destination variable or field must already be
defined.

We can also do pattern matching on records:

    def { a, b, c: .c }:          { 1, 2, .c: 3 }
    def myRec{ .a, .b, .c: .c }:  { 1, 2, .c: 3 }

If implicit key assignments in the record pattern follow the
same rules as for record constructors; starting at `@0` and
incrementing for each unspecified key.  Record patterns can
also have variadic elements:

    def { a, b, rest... }:          { 1, 2, 3, 4, 5, .thing1: 321, .thing2: 123 }
    def myRec{ .a, .b, .rest... }:  { 1, 2, 3, 4, 5, .thing1: 321, .thing2: 123 }

But the semantics are a bit different, the variadic record will be
filled with all the fields of the source that weren't explicitly
assigned to a variable.

In both pattern types only one variadic element can be specicied,
and it must come as the last item in the pattern.

## <a name="3.4">3.4 - Blocks</a>
A block is a sequence of expressions, executed in sequence, followed
by a result expression.  Some alternate names are `do-for` block,
and `do-for` expression; due to the keywords surounding the block.

    block = seq: "do" expression "for" & [delim] & expression

Block expressions have their own local scope, so definitions made
within a block aren't accessible outside of it; though the result
expression after the terminating `for` is evaluated in the same
scope, and so shares these variables.  The ultimate result of the
block expression is, obviously, the evaluation of the result
expression.

The syntax used for Ten's blocks are somewhat of a hybrid between
the `let-in` expressions common in functional languages, and imperative
code blocks; they allow us to sequence expressions while also returning
a result.

Here are a few code examples:

    def myVar: do
      def a: 1
      def b: 2
    for a + b

    def ( a, b ): do
      def a: 1
      def b: 2
    for ( a, b )

## <a name="3.5">3.5 - Conditionals</a>
Conditional expressions allow a program to make decisions regarding
which expression to evaluate, based on a given set of predicates.  These
might also be called `if-else` expressions.

Though Ten borrows the familiar `if-else` keywords, its conditional
expressions more resemble LISP's `cond` expression than more the
more familiar `if-else` chains common in imperative languages.  The
syntax is defined as:

    alt  = expression & ":" & [delim] & expression
    cond = seq: "if" alt "else" [delim] expression

Which looks something like:

    if
      predicate1: consequent1
      predicate2: consequent2
      predicate3: consequent3
    else default

Each predicate is evaluated in order until one of them results
in a truthful value; then the respective consquent expression is
evaluated, and its result taken as the ultimate result of the
conditional expression.  If none of the provided predicates return
something truthful, the the default expression given after the
`else` keyword is evaluated (this can be called the else clause).

For conditionals with only one predicate it may be preferable to
style the expression like so:

    if predicate:
      consequent
    else
      default

They can also be chained to look more like the imperative style,
but doing this is overly verbose, so should generally be avoided.


## <a name="3.6">3.6 - Signals</a>
Signals are Ten's take on hygienic gotos, they provide the flexibility
of early returns and other powerful control flow statements, but in
more flexible and consistent way.

Signals can only be invoked within the `in` expression of a signal
switch, which tells the expression what to do when a given signal
is received.

    sig = "sig" & [delim] & ident & ":" & [delim] & expression

    sparam  = ident | ident & "..."
    sparams = seq: "(" sparam ")"
    handler = ident & sparams & ":" & [delim] & expression
    switch  = seq: "when" handler "in" [delim] expression

The signal switch defines a set of handlers that are in scope within
the switch's `in` expression, and the signal expression invokes a
signal, jumping to the named handler, and passing the specified
arguments.

    when
      done( val ): val
      fail( err ): panic( err )
    in
      sig done: 123

If a signal is invoked somewhere within the `in` expression, then
control will jump to the respective handler, and the ultimate result
of the switch will be the result of evaluating the handler expression.

If no signal is triggered then the result is the result of the `in`
expression.

Signal handlers are function local, so they don't extend to nested
closures, but they can be stacked, with inner handler names overshadowing
outer names.

## <a name="3.7">3.7 - Constructors</a>
Ten has three language level constructors for building records,
closures, and tuples.  While records and closures are actual Ten
object types... tuples are a bit of an oddity.  There is no tuple
value type in Ten; tuples only exist on the temporary stack, and
their only purpose is in grouping multiple values together to be
passed as function arguments, returns, or as the source expression
for multiple assignments.

### <a name="3.7.1">3.7.1 - Closures</a>
Closures are Ten's only callback object type.  There are a few ways
to build these, including compiling them from source or creating
them from native functions; but the simplest is to use the language
level constructor.  The syntax for this is defined as:

    param  = ident | ident & "..."
    cls = seq: "[" param "]" [delim] expression

So the constructor consists of a parameter list enclosed in square
brackets `[...]`, followed by a result expression.  Like so:

    [ a, b ] a + b

A variadic parameter can also be specified as the last item
in the list.  The semantics for this are the same as for assignment,
a new record will be created with any extra arguments passed to
the function; and this record will be passed in the place of the
variadic parameter.

Ten has no special syntax for function definitions, instead it's just
a matter of assigning a closure to a variable:

    def add: [ a, b ] a + b

For direct definition to a closure constructor, the compiler can assign
the variable's name to the closure to be reported in errors for debugging
purposes.

Due to Ten's lack of a special function definition form recursing by
name isn't possible in most cases, since the function being defined
can't itself be captured; so each closure automatically defines a
special (and immutable) `this` variable, which refers to the current
closure.  All Ten code is executed as part of a closure call, so this
variable will always be defined, even at the root of scripts.


### <a name="3.7.2">3.7.2 - Records</a>
Record constructors are represented as a sequence of key-value
pairs between `{...}` curly braces; with the key for each entry
optionally omitted.

    pair = key & ":" & [delim] & expression | expression | "..." & primary
    rec  = seq: "{" pair "}"

If the key is omitted, then one will be implicitly assigned.  Implicit
keys begin at `@0` with the first omitted key and increment for each
instance thereafter; intermediate pairs with explicit keys aren't
considered in this traversal.

The last item in a record constructor can can be a record expansion,
which consists of `...` followed by an expression, which should evaluate
to a record.  The fields of the given record will be added to the record
being constructed.  If the expanded record contains fields that have
already been explicitly defined in the constructor, then the explicit
definitions are given precedence.

A record constructor is allowed to define the same key multiple times,
in which case all expressions will be evaluated, but later assignments
overwrite previous ones within the same constructor.

    { .k1: 'v1', .k2: 'v2' }        ` explicit keys
    { 'v1', 'v2', 'v3' }            ` implicit keys
    { .k1: 'v1', 'v2' }             ` both
    { .k1: 'v1', ...rest }          ` expansion


### <a name="3.7.3">3.7.3 - Tuples</a>
As mentioned previously, tuples are a bit of an oddity; not quite
a type of value, but almost.  They exist only on the stack, and
serve as a means of grouping multiple values to be passed together
as a single operand.

Tuple constructors consist of a sequences of expressions enclosed
in `(...)`.  And single value tuples are semantically equivalent
to single values, so parentheses can also be used to control
evaluation order without any special distinction.

    texp = expresssion | "..." & expression
    tup  = seq: "(" texp ")"

Tuple constructors can optionally provide a record expansion as their
last entry as `...` followed by an expression, which should evaluate
to a record.  This adds the values from the record, with keys `@0`,
`@1`, ..., until the first undefined field; to the end of the tuple in
order.

Tuples have a size limit of 32 items, which can be adjusted at compile time.
A reasonable tuple size limit is necessary to allow for efficient frame
allocation and stack management.  Attempts to create tuples larger than
this limit, including by record expansion, will cause an error.

## <a name="3.8">3.8 - Calls</a>
Function/closure calls in Ten are invoked with the 'invisible' call
operator.  This is a binary operator implied by the appearance of a
primary expression or path expression, immediately followed by a
primary expression without any explicit operators in between.

    call = primary
         | path
         | call & primary

So the following are interpreted as function calls:

    foo bar
    foo 123
    foo"123"
    foo{ 1, 2, 3 }
    foo( 1, 2, 3 )
    bar.foo()

Though it's good practice to put values without their own quoting
mechanism in parentheses; and multiple values can only be passed
as tuples.

Ten performs tail call optimization for any call that'll be evaluated
as the last operation of the parent call; but will never do so if the
call is enclosed in a tuple; so if for some reason a tail call isn't
desired then wrap it in parentheses.

### <a name="3.8.1">3.8.1 - Recursion</a>
Since Ten expresses all functions as lambdas, or anonymous closures,
and closures don't capture undefined variables; we can't do recursion
in the usual way, by calling the function-being-defined by name.  For
example if we say:

    def foo: [] foo()

The variable `foo` will be undefined, or reference a different variable,
within the closure's scope since the new `foo` variable won't be defined
until after the closure's construction.

We could get around this by defining the variable first, before constructing
the closure:

    def foo: nil
    set foo: [] foo()

But this becomes pretty verbose and awkward, so instead Ten defines a
special implicit variable called `this` for every closure, which refers
to the current closure.  Since all Ten code is wrapped in a closure at
some point, there will always be a `this` variable in scope, at any point
in a Ten program; but be careful with this as recursive modules or scripts
probably aren't a great idea.


## <a name="3.9">3.9 - Arithmetic</a>
Ten supports the standard set of arithmetic operators: `+`, `-`,
`*`, `/`, `%`.  The `-` operator can be used as either a prefix
or infix operator for negation and subtraction respectively.  In
addition to these the `^` operator is for exponentiation.

All arithmetic operators work on both `Int` and `Dec` values,
but not a combination of the two.  So either both operands have
to be integrals or both must be decimals, as Ten does not perform
any type coercion.

All binary operators are left associative in Ten, and `^` has highest
precedence of the arithmetic operators.  All unary operators share
the same precedence, just lower than `^`, and these are followed by
the product operators `*`, `/`, and `%`; then the summation operators
`+` and `-`.

    power = call
        | power & "^" & call

    unary = power
          | "~" & unary
          | "!" & unary
          | "-" & unary

    product = unary
            | product & "*" & unary
            | product & "/" & unary
            | product & "%" & unary

    summation = product
              | summation & "+" & product
              | summation & "-" & product


## <a name="3.10">3.10 - Logic</a>
Ten uses the same operators for bit boolean logic as for bit logic.
These all accept either Int or Log values, if given Int values then
the operations are performed per bit, while for Log values they're
performed per value.  The logical operators are NOT `~`, AND `&`,
XOR `\`, and OR <code>&vert;</code>.

Ten replaces the conventional `^` for XOR with `\`, which makes sense
since it's used as the mathematical symbol for set difference... and
it looks like a slanted OR ;)

The precedence for logical operators, except the unary `~`, is
just lower than arithmetic operators.

    logical = summation
            | logical & "&" & summation
            | logical & "\" & summation
            | logical & "|" & summation

## <a name="3.11">3.11 - Shifts</a>
Shift operators are for Int values only.  These perform logical
bit shifts on the bits on the Int value.  The shift operators are
`<<` for left shift and `>>` for right shift.  Negative shifts are
the equivalent of a positive shift in the opposite direction.

    shift = logical
          | shift & ">>" & logical
          | shift & "<<" & logical

## <a name="3.12">3.12 - Comparisons</a>
Ten has the usual comparison operators: `<`, `>`, `<=`, `>=`.  But
uses `=` instead of the more common `==` and `~=` in place of `!=`.
In addition, since the normal equality operators would fail on
attempts to compare with `udf`; the `!=` operator performs a comparison
that allows `udf` as operands.

    comparison = shift
               | comparison & ">"  & shift
               | comparison & "<"  & shift
               | comparison & ">=" & shift
               | comparison & "<=" & shift
               | comparison & "="  & shift
               | comparison & "~=" & shift
               | comparison & "!=" & shift

The range based comparisons only accept for numeric values, and both
operands have to be of the same numeric type.

## <a name="3.13">3.13 - Replacement</a>
Ten's replacement operators are equivalent to the short circuit logical
operators of other languages; renamed to reflect their more general
utility; since in Ten these operators use the same rules for truthiness
as conditionals, meaning their inputs don't have to be logical values.
These operators check their left hand operand for a specific condition,
if met then the right hand expression is evaluated and its
result returned, otherwise the left operand is returned without
evaluating the right expression.  The right operand replaces the left
if the condition is met.  These operators include `&?` and <code>&vert;?</code>
which are Ten's replacement for C's `&&` and `||` respectively.  Ten
also has `!?` which evaluates the right expression only if the left is `udf`.

    replacement = comparison
                | comparison & "&?" & expression
                | comparison & "|?" & expression
                | comparison & "!?" & expression

## <a name="3.14">3.14 - Assertion</a>
Ten's assertion operator `->` provides a nice syntax for confirming
the value of an expression.  This is an alternative to the prelude's
`assert()` function, which reports the line on which the assertion
occurs as an error message if the left operand isn't equal to the
right.

Unlike the equality operators `=` and `~=`, the assertion operator
works on tuples, compares strings by contents rather than identity,
and allows `udf` values.  Without these semantics the operator's
usefulness would be limited, since things like:

    myStr -> "some string"

Wouldn't work properly.  I haven't figured out a good way to compare
records in a useful way here; so for now we still compare records by
identity.


    assertion = comparison
              | comparison & "->" & comparison

## <a name="3.15">3.15 - Fix</a>
The `!` FIX operator is another unary operator unique to Ten.  It's
designed to make working with `udf` values a bit easier in some situations
by converting them to `nil`.  If the given operand is not `udf` then
this operator does nothing, just returning the given operator; otherwise,
if the operand is `udf`, then it returns `nil` in its place.

## <a name="3.16">3.16 - Scripts</a>
Ten scripts are UTF-8 or ASCII encoded text in some medium, which
contains a sequence of Ten expressions to be evaluated in order.
Scripts can also optionally be prefixed with a
[unicode BOM](https://en.wikipedia.org/wiki/Byte_order_mark)
and a
<a href="https://en.wikipedia.org/wiki/Shebang_(Unix)">Unix shebang</a>
which, if provided, are ignored by the compiler.

If the medium of a Ten script is a file, then its filename extension
should be `.ten`.

    expression = assertion

    BOM    = ? unicode byte order mark ?
    BANG   = ? line beginning in `#!`  ?
    EOF    = ? end of file or script   ?
    script = [BOM] & [BANG] & seq: "" expression EOF

[Next: 4 - The Prelude](the-prelude.md)
