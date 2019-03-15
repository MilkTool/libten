#include "ten_tables.h"

uint const fastGrowthMapCapTable[] = {
    7, 13, 29, 71, 151, 419, 1019, 2129, 4507, 10069, 24007
};
size_t const fastGrowthMapCapTableSize = 11;


uint const slowGrowthMapCapTable[] = {
    7, 13, 17, 29, 47, 59, 79, 113, 139, 173, 211, 311, 421,
    613, 821, 1201, 1607, 2423, 3407, 5647, 7649, 9679, 14243
};
size_t  const slowGrowthMapCapTableSize = 23;
