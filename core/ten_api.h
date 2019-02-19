// This component defines the API presented to users for
// interacting with the Ten VM.
#ifndef ten_api_h
#define ten_api_h

typedef struct {
    int TODO;
} ten_Config;

typedef struct {
    int TODO;
} ten_Tup;

typedef struct {
    int TODO;
} ten_Var;


typedef struct ten_Core ten_Core;

ten_Core*
ten_newCore( ten_Config* config, jmp_buf* errJmp );

void
ten_delCore( ten_Core* core );

#endif
