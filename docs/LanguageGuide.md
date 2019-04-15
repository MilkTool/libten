# Ten Language Guide
This guide is intended as a quick intro to the Ten programming
language.  Since the language itself is quite small and pretty
intuitive, this brief intro should go a good way toward outlining
Ten's capabilities and style.

* [About Ten](#about-ten)
* [Setting Up](#setting-up)
  - [Installation](#installation)
  - [Troubleshooting](#troubleshooting)
    - [Case 1](#case-1)
    - [Case 2](#case-2)
  - [Hello, World!](#hello-world)
* [Values](#values)
    - [Undefined](#undefined)
    - [Nothing](#nothing)
    - [Logical](#logical)
    - [Integral](#integral)
    - [Decimal](#decimal)
    - [Symbol](#symbols)
    - [String](#strings)
    - [Record](#records)
    - [Closure](#closures)
    - [Fiber](#fibers)
    - [Other Types](#other-types)
* [Expressions](#expressions)
  - [Arithmetic Operators](#arithmetic-operators)
  - [Logical Operators](#logical-operators)
  - [Relational Operators](#relational-operators)
* [Variables](#variables)
    - [Definition](#definition)
        * [Pattern Matching](#pattern-matching)
        * [Variadic Assignment](#variadic-assignment)
    - [Mutation](#mutation)
* [Functions](#functions)
    - [Function Scope](#function-scope)
    - [Function Calls](#function-calls)
    - [Variadic Parameters](#variadic-parameters)
    - [Recursion](#recursion)
* [Tuples](#tuples)
* [Records](#records)
  - [Fields](#fields)
  - [Indices](#indices)
* [Conditional Expressions](#conditional-expressions)
  - [Case Analysis](#case-analysis)
  - [Replacement](#replacement)
  - [Short-Circuit Logic](#short-circuit-logic)
* [Block Expressions](#block-expressions)
* [Signals and Handlers](#signals-and-handlers)
* [Iteration](#iteration)
  - [Iterators](#iterators)
  - [Loops](#loops)
* [Type Conversion](#type-conversion)
* [Type Checking](#type-checking)
* [Number Bases](#number-bases)
* [Strings](#strings)
* [Text](#text)
* [Lists](#lists)
* [Fibers](#fibers)
* [Error Handling](#error-handling)
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

## Values
...
