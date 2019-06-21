/***********************************************************************
This component implements `sprintf()` like string formatting with an
extension for formatting Ten values.  The component maintains an
internal buffer which serves as the destination for all format call,
and allows output string to be build up from multiple call to the
formatter.

The module supports three extra format specifiers, in addition to the
standard ones.  The `%v` specifier expects a `TVal` value, and converts
the Ten value into a stringified form, omitting quotation marks from
string and symbol contents.  The `%q` specifier is similar to `%v` but
keeps the quotes in the stringified value.  The `%t` specifier adds
the typename of the value to the formatter's output buffer.
***********************************************************************/

#ifndef ten_fmt_h
#define ten_fmt_h
#include "ten_types.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>

void
fmtInit( State* state );

typedef enum {
    FMT_VALS,
    FMT_VARS    
} FmtMode;

void
fmtMode( State* state, FmtMode mode );

char const*
fmtA( State* state, bool append, char const* fmt, ... );

char const*
fmtV( State* state, bool append, char const* fmt, va_list ap );

size_t
fmtLen( State* state );

char const*
fmtBuf( State* state );

#endif
