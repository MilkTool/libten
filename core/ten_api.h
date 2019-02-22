// This component defines the API presented to users for
// interacting with the Ten VM.
#ifndef ten_api_h
#define ten_api_h
#include <setjmp.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char pri[64];
} ten_DatInfo;

typedef struct {
    char pri[64];
} ten_PtrInfo;

typedef struct ten_Core ten_Core;

typedef struct {
    char pri[32];
} ten_Tup;

typedef struct {
    ten_Tup* tup;
    unsigned loc;
} ten_Var;

typedef void (*ten_FunCb)( ten_Core* core, ten_Tup* args, ten_Var* udat );

typedef enum {
    ten_ERR_MEMORY,
    ten_ERR_RECORD,
    ten_ERR_STRING,
    ten_ERR_ASSERT
} ten_ErrNum;

typedef struct {
    int TODO;
} ten_Trace;

typedef struct ten_Config {
    void* udata;
    void* (*frealloc)( void* udata,  void* old, size_t osz, size_t nsz );
    
    double memLimitGrowth;
} ten_Config;

ten_Core*
ten_newCore( ten_Config* config, jmp_buf* errJmp );

void
ten_delCore( ten_Core* core );

#endif
