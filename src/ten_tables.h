// This defines all the constant tables to be used throughout the
// rest of the implementation.
#ifndef ten_tables_h
#define ten_tables_h
#include <stddef.h>
#include "ten_types.h"

// These are tables of prime numbers to be used for sizing
// the buffers of lookup tables.
extern uint const   fastGrowthMapCapTable[];
extern size_t const fastGrowthMapCapTableSize;
extern uint const   slowGrowthMapCapTable[];
extern size_t const slowGrowthMapCapTableSize;

#endif
