/***********************************************************************
This file just includes `math.h` for now.  It'll be extended later
to declare the subset of math functions used by Ten's implementation
if the `ten_LIBM` macro isn't defined, which indicates that internal
implementations of these functions should be used.  These internal
implementations don't yet exist, so `ten_LIBM` must be defined for
compilation.
***********************************************************************/

#ifndef ten_math_h
#define ten_math_h

#ifdef ten_LIBM
    #include <math.h>
#else
    #error "Internal math functions haven't been implemented yet"
#endif

#endif
