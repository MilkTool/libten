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

typedef void
(*ten_FunCb)( ten_Core* core, ten_Tup* args, ten_Tup* mems, void* dat );

typedef enum {
    ten_ERR_NONE,
    #ifdef ten_TEST
        ten_ERR_TEST,
    #endif
    ten_ERR_MEMORY,
    ten_ERR_RECORD,
    ten_ERR_STRING,
    ten_ERR_FIBER,
    ten_ERR_CALL,
    ten_ERR_SYNTAX,
    ten_ERR_LIMIT,
    ten_ERR_COMPILE
} ten_ErrNum;

typedef struct ten_Trace ten_Trace;
struct ten_Trace {
    char const* file;
    unsigned    line;
    ten_Trace*  next;
};

typedef struct ten_Source {
    int  (*next)( struct ten_Source* src );
    int  first;
    uint line;
} ten_Source;

typedef struct ten_Config {
    void* udata;
    void* (*frealloc)( void* udata,  void* old, size_t osz, size_t nsz );
    
    double memGrowth;
} ten_Config;

ten_Core*
ten_newCore( ten_Config* config, jmp_buf* errJmp );

void
ten_delCore( ten_Core* core );

#endif
