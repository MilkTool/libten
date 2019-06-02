# Ten Documentation
This is the base directory for Ten's core documentation.  Its most
relevant subdirectories are `manual` which contains the language
reference manual and `articles` containing various supplementary
articles describing various other aspects of the language and its
implementation.

## Reference Manual
Ten's reference manual, along with the reference VM implementation,
defines the language.  So any alternative implementations of the Ten
programming language should comply with the language description
given in the manual.  If an aspect of the language is left unclear
in the manual; then any alternative implementations should match the
behavior of the reference implementation found in `../src/`.

## Articles
Some aspects of Ten's implementation, or the language semantics or
reasoning; may be difficult to explain in plain old comments.  So
I've removed a lot of commentary on various aspects of the implementation
and moved the explanations to separate documents.  Other articles might
also be written to rationalize or explore certain aspects of the language
or implementation.

- [Records](articles/Records.md)

    This article covers the relationship between Ten's records and
    indices, and the details on how the language handles the
    sharing of indices between records.

- [Re-Entry](articles/Re-Entry.md)

    This article explains Ten's mechanism for native function re-entry,
    and how it's implemented.

## Implementation
Ten's implementation code is organized fairly cleanly.  There are
several _components_ of the Ten runtime; these are semi-independent
parts of the runtime that maintain their own state, which can't
be seen directly by other components.  Each component is implemented
in a single file `ten_???.c` and its interface (used by other components)
in another `ten_???.h` file.  The following are Ten's implementation
components.

- [ten_cls](../src/ten_cls.h)
- [ten_com](../src/ten_com.h)
- [ten_dat](../src/ten_dat.h)
- [ten_env](../src/ten_env.h)
- [ten_fib](../src/ten_fib.h)
- [ten_fmt](../src/ten_fmt.h)
- [ten_fun](../src/ten_fun.h)
- [ten_gen](../src/ten_gen.h)
- [ten_idx](../src/ten_idx.h)
- [ten_lib](../src/ten_lib.h)
- [ten_ptr](../src/ten_ptr.h)
- [ten_rec](../src/ten_rec.h)
- [ten_str](../src/ten_str.h)
- [ten_sym](../src/ten_sym.h)
- [ten_upv](../src/ten_upv.h)

In addition to these components, the code base contains several other
files (either as `*.c` and `*.h` pairs or just `*.h` files) which provide
facilities used elsewhere in the implementation.  We also have some
`*.inc` files, which contain incomplete code intended to be included
elsewhere; where the includer is expected to 'finish' the templates
by providing the missing context.
