// This component defines the API presented to users for
// interacting with the Ten VM.
#ifndef ten_api_h
#define ten_api_h
#include <setjmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>

typedef struct {
    char pri[512];
} ten_State;

typedef struct {
    char pri[64];
} ten_DatInfo;

typedef struct {
    char const* tag;
    unsigned    size;
    unsigned    mems;
    void        (*destr)( ten_State* s, void* buf );
} ten_DatConfig;

typedef struct {
    char pri[64];
} ten_PtrInfo;

typedef struct {
    char const* tag;
    void        (*destr)( ten_State* s, void* addr );
} ten_PtrConfig;

typedef struct {
    char pri[32];
} ten_Tup;

typedef struct {
    ten_Tup* tup;
    unsigned loc;
} ten_Var;

typedef void
(*ten_FunCb)( ten_State* core, ten_Tup* args, ten_Tup* mems, void* dat );

typedef struct {
    char const*  name;
    char const** params;
    ten_FunCb    cb;
} ten_FunParams;

typedef enum {
    ten_ERR_NONE,
    #ifdef ten_TEST
        ten_ERR_TEST,
    #endif
    ten_ERR_MEMORY,
    ten_ERR_SYSTEM,
    ten_ERR_RECORD,
    ten_ERR_STRING,
    ten_ERR_FIBER,
    ten_ERR_CALL,
    ten_ERR_SYNTAX,
    ten_ERR_LIMIT,
    ten_ERR_COMPILE,
    ten_ERR_USER,
    ten_ERR_TYPE,
    ten_ERR_ARITH,
    ten_ERR_ASSIGN,
    ten_ERR_TUPLE,
    ten_ERR_PANIC
} ten_ErrNum;

typedef enum {
    ten_COM_CLS,
    ten_COM_FIB
} ten_ComType;

typedef struct ten_Trace ten_Trace;
struct ten_Trace {
    char const* file;
    unsigned    line;
    ten_Trace*  next;
};

typedef struct ten_Source {
    int  (*next)( struct ten_Source* src );
} ten_Source;

typedef struct ten_Config {
    void* udata;
    void* (*frealloc)( void* udata,  void* old, size_t osz, size_t nsz );
    
    double memGrowth;
} ten_Config;

// Initialization and finalization.
void
ten_init( ten_State* s, ten_Config* config, jmp_buf* errJmp );

void
ten_finl( ten_State* s );


// Stack manipulation.
ten_Tup
ten_pushA( ten_State* s, char const* pat, ... );

ten_Tup
ten_pushV( ten_State* s, char const* pat, va_list ap );

ten_Tup
ten_top( ten_State* s );

void
ten_pop( ten_State* s );

ten_Tup
ten_dup( ten_State* s, ten_Tup* tup );

// Global variables.
void
ten_def( ten_State* s, ten_Var* name, ten_Var* val );

void
ten_set( ten_State* s, ten_Var* name, ten_Var* val );

void
ten_get( ten_State* s, ten_Var* name, ten_Var* dst );

// Types.
void
ten_type( ten_State* s, ten_Var* var, ten_Var* dst );

void
ten_expect( ten_State* s, char const* what, ten_Var* type, ten_Var* var );

// Misc.
bool
ten_equal( ten_State* s, ten_Var* var1, ten_Var* var2 );

// Temporary values.
ten_Var*
ten_udf( ten_State* s );

ten_Var*
ten_nil( ten_State* s );

ten_Var*
ten_log( ten_State* s, bool log );

ten_Var*
ten_int( ten_State* s, long in );

ten_Var*
ten_dec( ten_State* s, double dec );

ten_Var*
ten_sym( ten_State* s, char const* sym );

ten_Var*
ten_ptr( ten_State* s, void* ptr );

// Compilation.
void
ten_compileFile( ten_State* s, FILE* file, ten_ComType out, ten_Var* dst );

void
ten_compilePath( ten_State* s, char const* path, ten_ComType out, ten_Var* dst );

void
ten_compileScript( ten_State* s, char const* script, ten_ComType out, ten_Var* dst );

void
ten_compileExpr( ten_State* s, char const* expr, ten_ComType out, ten_Var* dst );

// Execution.
void
ten_executeFile( ten_State* s, FILE* file, ten_Var* dst );

void
ten_executePath( ten_State* s, char const* path, ten_Var* dst );

void
ten_executeScript( ten_State* s, char const* script, ten_Var* dst );

void
ten_executeExpr( ten_State* s, char const* expr, ten_Var* dst );


// Singleton values.
bool
ten_isUdf( ten_State* s, ten_Var* var );

void
ten_setUdf( ten_State* s, ten_Var* dst );

bool
ten_isNil( ten_State* s, ten_Var* var );

void
ten_setNil( ten_State* s, ten_Var* dst );

// Logical values.
bool
ten_isLog( ten_State* s, ten_Var* var );

void
ten_setLog( ten_State* s, bool log, ten_Var* dst );

bool
ten_getLog( ten_State* s, ten_Var* var );

// Integral values.
bool
ten_isInt( ten_State* s, ten_Var* var );

void
ten_setInt( ten_State* s, long in, ten_Var* dst );

long
ten_getInt( ten_State* s, ten_Var* var );

// Decimal values.
bool
ten_isDec( ten_State* s, ten_Var* var );

void
ten_setDec( ten_State* s, double dec, ten_Var* dst );

double
ten_getDec( ten_State* s, ten_Var* var );

// Symbol values.
bool
ten_isSym( ten_State* s, ten_Var* var );

void
ten_setSym( ten_State* s, char const* sym, size_t len, ten_Var* dst );

char const*
ten_getSymBuf( ten_State* s, ten_Var* var );

size_t
ten_getSymLen( ten_State* s, ten_Var* var );

// Pointer values.
bool
ten_isPtr( ten_State* s, ten_Var* var );

void
ten_setPtr( ten_State* s, void* addr, ten_PtrInfo* info, ten_Var* dst );

void*
ten_getPtrAddr( ten_State* s, ten_Var* var );

ten_PtrInfo*
ten_getPtrInfo( ten_State* s, ten_Var* var );

char const*
ten_getPtrType( ten_State* s, ten_Var* var );

void
ten_initPtrInfo( ten_State* s, ten_PtrConfig* config, ten_PtrInfo* info );

// Strings objects.
bool
ten_isStr( ten_State* s, ten_Var* var );

void
ten_newStr( ten_State* s, char const* str, size_t len, ten_Var* dst );

char const*
ten_getStrBuf( ten_State* s, ten_Var* var );

size_t
ten_getStrLen( ten_State* s, ten_Var* var );

// Index objects.
bool
ten_isIdx( ten_State* s, ten_Var* var );

void
ten_newIdx( ten_State* s, ten_Var* dst );

// Record objects.
bool
ten_isRec( ten_State* s, ten_Var* var );

void
ten_newRec( ten_State* s, ten_Var* idx, ten_Var* dst );

void
ten_recSep( ten_State* s, ten_Var* rec );

void
ten_recDef( ten_State* s, ten_Var* rec, ten_Var* key, ten_Var* val );

void
ten_recSet( ten_State* s, ten_Var* rec, ten_Var* key, ten_Var* val );

void
ten_recGet( ten_State* s, ten_Var* rec, ten_Var* key, ten_Var* dst );

// Function objects.
bool
ten_isFun( ten_State* s, ten_Var* var );

void
ten_newFun( ten_State* s, ten_FunParams* p, ten_Var* dst );

// Closure objects.
bool
ten_isCls( ten_State* s, ten_Var* var );

void
ten_newCls( ten_State* s, ten_Var* fun, ten_Var* dat, ten_Var* dst );

// Fiber objects.
bool
ten_isFib( ten_State* s, ten_Var* var );

void
ten_newFib( ten_State* s, ten_Var* cls, ten_Var* dst );

ten_Tup
ten_cont( ten_State* s, ten_Var* fib, ten_Tup* args );

void
ten_yield( ten_State* s, ten_Tup* vals );

void
ten_panic( ten_State* s, ten_Var* val );

ten_Tup
ten_call( ten_State* s, ten_Var* cls, ten_Tup* args );

ten_ErrNum
ten_getErrNum( ten_State* s, ten_Var* fib );

void
ten_getErrVal( ten_State* s, ten_Var* fib, ten_Var* dst );

char const*
ten_getErrStr( ten_State* s, ten_Var* fib );

ten_Trace*
ten_getTrace( ten_State* s, ten_Var* fib );

void
ten_clearError( ten_State* s, ten_Var* fib );

// Data objects.
bool
ten_isDat( ten_State* s, ten_Var* var );

void*
ten_newDat( ten_State* s, ten_DatInfo* info, ten_Var* dst );

void
ten_setMember( ten_State* s, ten_Var* dat, unsigned mem, ten_Var* val );

void
ten_getMember( ten_State* s, ten_Var* dat, unsigned mem, ten_Var* dst );

ten_Tup
ten_getMembers( ten_State* s, ten_Var* dat );

ten_DatInfo*
ten_getDatInfo( ten_State* s, ten_Var* dat );

char const*
ten_getDatType( ten_State* s, ten_Var* dat );

void*
ten_getDatBuf( ten_State* s, ten_Var* dat );

void
ten_initDatInfo( ten_State* s, ten_DatConfig* config, ten_DatInfo* info );


#endif
