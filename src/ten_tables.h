/***********************************************************************
This is a collection of tables used elsewhere in the code base.
Optimizing these tables allows for the optimization of the growth
rate of various abstract structures used in Ten, and thus improve
the performance of the VM itself.  These tables haven't yet been
optimized in any way.
***********************************************************************/

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

// This table is for sizing record value arrays.
extern uint const   recCapTable[];
extern size_t const recCapTableSize;

#endif
