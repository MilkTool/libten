// This component implements sprintf() like string formatting with
// an extension for formatting Ten values.  It maintaines an internal
// buffer which serves as the destination for all format calls, and
// allows the string to be built up from multiple calls to the format
// functions.  The format functions support all the standard patterns
// with the addition of `%v` for the stringified form of a value,
// `%q` for the stringified form of a value including the quotes on
// strings and symbols, and `%t` for the type of a value.
#ifndef ten_fmt_h
#define ten_fmt_h
#include "ten_types.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>

void
fmtInit( State* state );

char const*
fmtA( State* state, bool append, char const* fmt, ... );

char const*
fmtV( State* state, bool append, char const* fmt, va_list ap );

size_t
fmtLen( State* state );

char const*
fmtBuf( State* state );

#endif
