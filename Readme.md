# Ten
A faster and better Rig.  The core of the language will be pretty
much the same, just a few minor changes.  But the implementation
will be redesigned for better efficiency with what I've learned
from Rig.  Also, we get rid of the `rec` keyword, that leaves us
with exactly... TEN keywords: def, set, do, for, if, else, true,
false, nil, udf.

## NaN Encoding
Will use NaN encoding for more efficient value representation, this
does mean a slight modification to the language as we need to reduse
the size of ints to 32 bits; but this may be better for overal
efficiency anyway; especially for 32bit systems.

## Index Implementation
I'm still not sure whether the standard hash table implementation
or Rig's way of counting steps is more efficient.  I'll run some
benchmarks to decide which to use in the new Rig.  I think with some
optimization I can probably get Rig's way to be more efficient, as
its cache usage should be better.

## Objects
The current implementation of Rig has quite a bit of unnecessary
overhead for GC objects.  We have a pointer to the next object,
a GC mark, and a type tag.  On 64bit platforms this would usually
amount to about 12bytes (8 for the address and 4 for the rest).
We can reduce this to a single 8byte pointer by encoding the other
fields in the unused bits of the pointer.  For 32bit machines the
overhead will stay about the same, but it'll be reduced significantly
for 64bit architectures.  For smaller 32bit devices that don't use
all bits of a 32bit address, we can do the same encoding to save
an extra word per object.

## Records
The current record implementation is also fairly wasteful.  It
employs two pointers, one to the index and another to its value
buffer.  The index pointer is necessary for obvious reasons, but
the value buffer could be appended to the end of the record, if
records were made immutable.  But mutability is also important.
But we already have a mechanism for making record structure imutable,
and it takes up another part of the record.  The `lock` field tells
us that the record is locked, and can no longer be changed.  But if
we instead have two record types, one for unlocked records; those
which are still expected to change structure, and another for locked
records.  We can remove the `lock` field from both record types, and
and the `val` field from locked records, to be replaced with a
flexible array at the end of the record.  We can also encode the
`cap` in the unused bits of the `idx` pointer; records aren't meant
to get extremely big, so if we give them a reasonable size limit
say `2^16` then it'll be reasonably safe to keep them in the front
of a pointer.

With these changes we'd reduce the record size from about 24bytes
on 64bit machines to 8 for locked and 16 for unlocked records.  Pretty
significant.

## Virtual Registers
The way Rig does virtual registers is neat since it keeps the cache
clean and avoids the cost of a lot of dereferences.  Unfortunately
it also requires a lot of extra compute as we have to save and restore
the registers for each call; this extra overhead basically negates
the advantage of this style of registers, so the point that Rig
function calls basically perform on par, but not better, than Lua's.
If we reduce this set to only the registers used most frequently
then performance will likely improve significantly.

Say we keep the IP and SP, and maybe pointers to the constants and
locals buffers; and keep the rest in a stack of activation records.
This will increase the memory used by the stack as the size of each
activation record will increase; but the benefit will likely be worth
it as we'll generally never have a huge number of call frames on the
stack.

## Instruction Set
The current instruction set is pretty inefficient.  It uses labels
for jumps, which isn't necessarily bad; but it doesn't take advantage
of what's good about them since all operands are encoded as 16bit
values; which the interpretter has to parse for any instruction that
takes an operand.  The new instruction set will take advantage of
its implementation as a stack machine; with the first few operand
values of each category (constant, local, label, etc) encoded in
the instruction itself.  The compiler can also keep track of which
local variables contain, don't contain, or might contain upvalues
to make access more efficient; avoiding a runtime conditional.  The
new VM will also use 16bit instructions instead of the current 8,
this doesn't cost much but can boost performance significantly.  We
can also optimize the interpretter depending on if computed gotos
are supported.  For example for compilers without support we may
want a reduced instruction set to be on the safe side; even though
the switch may itself be optimized into a goto table.  So for
builds with computed goto support we may take the first 4 or 5 bits
as the instruction, with the rest as operand or specializations; but
for others we could take the first 8 or 9.

## Rig, Pri, Usr
The current Rig implementation distinguishes between Rig functions,
primitive functions, and user functions; and between primitive data
objects and user data objects.  This differentiation adds complexity,
introduces inconsistency, and adds overhead as we have to constantly
check which type we're dealing with.  Native functions shouldn't
really be poking around in Rig internals anyway.  So instead let's
just have Rig functions and native functions.  Data objects and
pointers will always represent native data.  And the library and
prelude will go through the standard Rig API; this adds a bit of
overhead but simplifies things considerably and should reduce code
size.

## Padding
Rig's 'padding' feature is pretty useless.  Should get rid of it
and replace with variatic parameters.  Can define an automatic
variable `vargs`; which will be filled with a record of extra
arguments.

## Recursion
The `rec` keyword for recursion maybe isn't the best solution.  It
prevents us from recursing on unnamed closures.  Instead each closure
can be given an automatic `self` parameter which refers to the called
closure; this won't cost any extra space since the closure will already
be on the stack; and will speed up recursion by changing an upvalue
access to a local access.  Besides, the `rec` keyword puts us over 10
keywords...
