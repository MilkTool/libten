// This defines all the constant tables to be used throughout the
// rest of the implementation.
#ifndef ten_tables_h
#define ten_tables_h
#include "ten_types.h"

// These are tables of prime numbers to be used for sizing
// the buffers of lookup tables.
uint const* const fastGrowthMapCapTable;
size_t const      fastGrowthMapCapTableSize;
uint const* const slowGrowthMapCapTable;
size_t const      slowGrowthMapCapTableSize;

#endif
