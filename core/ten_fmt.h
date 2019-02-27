// This component implements sprintf() like string formatting with
// an extension for formatting Ten values.  It maintaines an internal
// buffer which serves as the destination for all format calls, and
// allows the string to be built up from multiple calls to the format
// functions.  The format functions support all the standard patterns
// with the addition of `%v` to inserting the stringified form of a
// value, `%V` to inserting a value formatted with Ten's syntax,
// `%t` to insert the type of a value, and `%T` to insert the type
// for a tag.
#ifndef ten_fmt_h
#define ten_fmt_h
#include "ten_types.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>

void
fmtInit( State* state );

void
fmtA( State* state, bool append, char const* fmt, ... );

void
fmtV( State* state, bool append, char const* fmt, va_list ap );

size_t
fmtLen( State* state );

char const*
fmtBuf( State* state );

#endif
