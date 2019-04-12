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
run is put into a `ten_State` struct, which can be created with:

    ten_State*
    ten_make( ten_Config* config, jmp_buf* errJmp );


