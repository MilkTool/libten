# Ten
Ten is a small scripting language designed for simplicity and consistency.  It
omits several features common to other languages in favor of a simpler, more
consistent syntax and intuitive semantics.

The language borrows a lot from Lua, including its minimalism; but Ten takes
this even further to omit language level looping constructs and a few other
common features.  Like Lua, Ten has only one compound data type, the record.

## Features
Some of Ten's more interesting features include:

* **Small, Simple, Elegant!**

  One of Ten's key design philosophies is that elegance is achieved through
  simplicity, so the language is really simple; and will stay that way.  Ten
  is flexible enough to allow for a powerful ecosystem to grow around it; but
  the core language should change very little as it ages.

* **Fast, Efficient, Awesome!**

  Not only is Ten small, it's also pretty fast, and is great at using memory
  efficiently.  Ten uses an efficient single pass compiler to save time and
  space, and represents values as NaN tagged 64bit floats to avoid the extra
  space needed for tagging.  It also uses an efficient record design that
  saves memory by allowing multiple records to share a single lookup table.

* **Portable, Embeddedable, Sweetness!**

  Ten's core runtime is implemented in portable C99 code, so it can run on
  virtually any hardware and only depends on a C standard library.
  Furthermore it uses no global state, so multiple runtime instances can
  exist simultaneously.  And last but not least, it has a pretty nice embedding
  API that's designed to be ABI compatible with future releases.


## Resources

* (TODO)[Language Guide](LanguageGuide)

  A quick and dirty guide to get you started with Ten.

* (TODO)[Embedding Guide](EmbeddingGuide)

  All you need to know about embedding Ten in other applications.

* (TODO)[Library Reference](LibReference)

  Concise reference for Ten's builtin library.

* (TODO)[API Reference](ApiReference)

  Concise reference for Ten's embedding API.

## Syntax

**Records**

Records are Ten's only compound data type, they're similar to Python dicts
or Lua tables; but are designed to be used more like structs than hash maps.

    def myRec: { .key1: 1, .key2: 2, @'key3': 3 }
    def val1: myRec.key1
    def val2: myRec@'key2'


**Closures**

Closures are Ten's only type of function, or functional unit.  Unlike other
similar languages Ten doens't have a special 'function definition' syntax;
function definitions are just assignments to closures.

    def add: [ a, b ] a + b
    def sum: add( 1, 2 )

Unlike its predecesor Rig, Ten supports variadic parameters, the extra arguments
are put in a record.

    def add: [ args... ] args@0 + args@1 + args@2
    def sum: add( 1, 2, 3 )

**Assignment**

Ten is pretty good at assignment.  It supports variable and field assignment
from tuples or records as well as normal single assignment.

    def ( v1, v2 ): ( 1, 2 )
    def { v1, v2 }: { 1, 2 }

    def { v1: .k1, .v2: .k2 }: { .k1: 1, .k2: 2 }

    def myRec@( .k1, .k2 ): ( 1, 2 )

We can also use variadic magic in assignments.

    def ( vals... ): ( 1, 2, 3 )
    def val1: vals@0
    def val2: vals@1
    def val3: vals@2

**Conditionals**

Though it uses the traditional `if-else` keywords, Ten's conditionals are more
like LISP `cond` forms, allowing for several cases intead of just two.

    if
      thing = 1: show( "Thing is 1", N )
      thing = 2: show( "Thing is 2", N )
      thing = 3: show( "Thing is 3", N )
    else
      show( "Thing is something else", N )

We can also use the more traditional looking formatting though.

    if thing = 1:
      show( "Thing is 1", N )
    else
    if thing = 2:
      show( "Thing is 2", N )
    ...
    else
      show( "Thing is something else", N )

And of course, everything is an expression in Ten, so...

    def max: if a > b: a else b


**Code Blocks**

Ten's code blocks are something of a hybrid between traditional imperative
code blocks and the more functional 'let-in' form.

    def answer:
      do
        show"Enter a number: "
        def raw: input()
      for int( raw )

**Signal Handlers**

These aren't traditional signals, but they give Ten scripts a way to
jump around and allow for early returns without breaking the 'everything is
an expression' rule.

    def result:
      when
        return( val ): val
        failed( err ): panic( err )
      in
        do
          def bar: foo()
        for
          if bar:
            sig return( bar )
          else
            sig failed( "It's broken" )
