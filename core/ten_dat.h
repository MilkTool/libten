#ifndef ten_dat_h
#define tan_dat_h
#include "ten_types.h"
#include "ten_api.h"

typedef struct {
    SymT   type;
    size_t size;
    uint   nMems;
    void   (*destr)( ten_Core* core, void* ptr );
} DatInfo;

struct Data {
    DatInfo* info;
    TVal*    mems;
    char     data[];
};

#define datSize( STATE, DAT ) (sizeof(Data) + (DAT)->info->size)
#define datTrav( STATE, DAT ) (datTraverse( STATE, DAT ))
#define datDest( STATE, DAT ) (datDestruct( STATE, DAT ))

void
datInit( State* state );

Data*
datNew( State* state, DatInfo* info );

void
datTraverse( State* state, Data* data );

void
datDestruct( State* state, Data* data );

#endif
