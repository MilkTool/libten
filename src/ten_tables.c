#include "ten_tables.h"
#include "ten_macros.h"

uint const fastGrowthMapCapTable[] = {
    7, 13, 29, 71, 151, 419, 1019, 2129, 4507, 10069, 24007
};
size_t const fastGrowthMapCapTableSize = elemsof(fastGrowthMapCapTable);


uint const slowGrowthMapCapTable[] = {
    7, 13, 17, 29, 47, 59, 79, 113, 139, 173, 211, 311, 421,
    613, 821, 1201, 1607, 2423, 3407, 5647, 7649, 9679, 14243
};
size_t const slowGrowthMapCapTableSize = elemsof(slowGrowthMapCapTable);

uint const recCapTable[] = {
    1, 2, 3, 4, 5, 6, 7, 14, 21, 28, 35, 42, 49, 56, 112, 168,
    224, 280, 336, 392, 448, 896, 1344, 1792, 2240, 2688, 3136,
    3584, 7168, 10752, 14336, 17920, 21504, 25088, 50176, 75264,
    100352, 125440, 150528, 175616, 200704, 401408, 602112, 802816,
    1003520, 1204224, 1404928, 1605632, 3211264, 4816896, 6422528,
    8028160, 9633792, 11239424, 12845056, 25690112, 38535168,
    51380224, 64225280, 77070336, 89915392, 102760448, 205520896,
    308281344, 411041792, 513802240, 616562688, 719323136, 822083584
};
size_t const recCapTableSize = elemsof(recCapTable);
