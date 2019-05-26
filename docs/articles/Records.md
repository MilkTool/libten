# Records
Records are Ten's only data structuring mechanism.  These are a form
of hashmap as used by most other dynamic languages, but not quite
the same thing since Ten's records are optimized for usage similar
to structs or objects in other languages; and aren't quite as
flexible are regular hashmaps when it comes to representing larger or
unique collections of data.

Most dynamic programming languages use some form of hashmap for
structuring data.  While this approach is quite flexible, and allows
us to avoid much of the scaffolding necessary in more static languages,
the memory overhead of so many hashmaps can be unreasonable, so Ten
takes a slightly different approach.

Instead of representing a record as a full hashmap, each record just has
an array of field values (no keys), and a pointer to an index, which can
be shared between multiple records and serves as a common lookup table
for the fields of an associated record.  The index is a map between the
keys of its associated records, and the slot allocated for the respective
value within each record.  So the value associated with a key for a
particular record can be accessed as the value in the mapped slot of that
record's value array.

![Record-Index Interaction][record-index-interaction.svg]

The above diagram demonstrates a sequence of interactions with `Record 1`
and `Record 2`, both of which share a common `Index`.  While both records
define fields `.foo` and `.bar`, the second record doesn't define `.baz`
but must allocate a slot for the field anyway since it's defined in the
common index.

## Record Implementation
Record objects are represented in the Ten runtime with the struct:

    struct Record {
        TPtr idx;
        TPtr vals;
    };

The `TPtr` type represents a tagged pointer, capable of holding both a 48 bit
pointer and 16 bits of auxiliary data.  Storing two fields in a single 64 bit
word will improve memory efficiency on most 64 bit platforms, but might _add_
overhead on 32 but systems; so this 'packing' can be disabled by defining
`ten_NO_POINTER_TAGS`, and should generally be disabled for 32 bit builds.

The `idx` field holds a pointer to the record's index along with a tag
to indicate if the record has been marked for separation (any bit in
the TPtr's tag is set); more on record separation below.

The `vals` field defines a dynamic array of field values, with the
pointer portion containing a `TVal*` to the value array, and the tag
giving the array size as a row of the
[`recCapTable`][recCapTable] in [`ten_tables.h`][ten_tables.h].

The `sep()` function available in Ten's prelude can be called on a record
to mark it for separation, since the actual separation occurs lazily, this
just sets a bit in `idx`'s tag (any bit as it's currently implemented).
Record separation cuts the tie between a record and its current index,
replacing the old index with a new index with only the subset of original
keys that are used by the separated record.  The actual separation of a
record will occur on the first definition made to the record after it's
been marked for separation.  This ability to disassociate a particular
record from a common index will be necessary at times to avoid contaminating
a widely used index with fields specific only to a single record.

Ten's compiler will associate a common index to any records constructed with
the same lexical constructor.  For example if we define a function:

    def cons: [ car, cdr ] { .car: car, .cdr: cdr }

Each call to `cons` will create a new record,  but all such records will
be associated with the same index unless separated at a later point.  This
system plays will with common programming practices in most paradigms, since
objects or data structures created at the same point in a program will often
be used in a similar fashion and make use of the same set of keys, though
there are of course exceptions.

The full record implementation can be found in
[`ten_rec.h`][ten_rec.h].

## Index Implementation
Indices are just regular hashmaps that map an arbitrary Ten value to a
location within the `vals` array of associated records.  Though the
index can be implemented as any form of associative array; the current
design uses a form of open addressing hash map with a limited number
of polls.

The theoretical advantages to this design is that it has good data locality
so lookups should generally be faster than a separate chaining implementation.
Open addressing hashmaps are also usually more efficient with memory than
their separate chaining counterparts.  One of the typical disadvantages with
open addressing map implementations is that the lack of separate item buckets
forces the map to check every array slot before determining that it doesn't
contain an item.  Ten mitigates this, and improves caching for lookups, by
polling the map array linearly with a step size of 1 and a limited number of
steps based on the log of the map's size; if an item hasn't been found within
allotted step limit then the lookup will fail.  The actual efficiency of Ten's
index implementation is yet to be tested against other options.

Ten's index objects are represented by the structure:

    struct Index {
        uint nextLoc;

        uint stepTarget;
        uint stepLimit;

        struct {
            size_t row;
            size_t cap;
            TVal*  keys;
            uint*  locs;
        } map;

        struct {
            uint   row;
            uint*  buf;
        } refs;
    };

The `nextLoc` field keeps track of the next slot location to be allocated,
it'll increase by 1 for each allocation, and never decrease.

The `stepTarget` and `stepLimit` fields control the polling step limit.
The `stepLimit` keeps track of the current step limit, which will be the
largest distance between one of the map's entries and its ideal location,
it'll be adjusted as new entries are added, but will tend toward `stepTarget`
since the index will grow whenever a definition puts `stepLimit > stepTarget`.

The `map` structure implements the map array itself, with `cap` giving the
current capacity of the map and `row` giving a row within the
[`fastGrowthMapCapTable`][fastGrowthMapCapTable] in
[`ten_tables.h`][ten_tables.h].  Both of these are required,
though they'll reflect the same value in smaller tables, because the
growth algorithm will resort to exponential growth when the `row` goes
beyond the number of items available in the table.  The `keys` and
`loc`ations are stored within separate arrays for better lookup caching.

The index keeps track of the number of records for which a particular field
(slot) is defined in the `refs` array; when this reaches `0` it can be
recycled to associate it with a new key.  The `refs.row` keeps track of
the size of the array as a row of the
[`recCapTable`][recCapTable] in
[`ten_tables.h`][ten_tables.h].

The full index implementation can be found in
[`ten_idx.h`][ten_idx.h].

## Advantages
The main advantage of Ten's record system is that it allows for good memory
efficiency when records are used as nodes or objects (i.e a lot of
records will share the same fields).  Since this is a pretty typical use for
map-like objects in dynamic languages and most programming paradigms, it
allows for significant memory savings without much additional effort from the
programmer.  For example in a previous example function:

    def cons: [ car, cdr ] { .car: car, .cdr: cdr }

Each `cons()` call creates a new record, but all such records share the
same index.  The records themselves will be relatively small, consuming
only ~34 bytes on a 64 bit system; this makes it practical to implement
a lot of data structures in terms of records rather than defining a new
language level type for each.  For example Ten's prelude includes a few
functions for dealing with LISP style lists with nodes as above.

## Disadvantages
One of the major disadvantages to Ten's record system is that it isn't
especially suitable for representing unique hashmaps since the record-index
separation adds additional memory overhead in the form of the record's value
array, and this overhead will only be offset when multiple record instances
are created with the same index.

[record-index-interaction.svg]: ../assets/record-index-interaction.svg
[ten_tables.h]:                 ../../src/ten_tables.h
[ten_rec.h]:                    ../../src/ten_rec.h
[ten_idx.h]:                    ../../src/ten_idx.h
[fastGrowthMapCapTable]:        ../../src/ten_tables.h#L10
[recCapTable]:                  ../../src/ten_tables.h#L16
