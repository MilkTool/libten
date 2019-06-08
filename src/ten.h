/**********************************************************************
This component implements the API presented to users for
interacting with the Ten VM.  The API functions and
structures are declared here, but not described at all as
the API if fully documented in the reference manual, which
can be found in `../docs/manual`.
**********************************************************************/

#ifndef ten_api_h
#define ten_api_h
#include <setjmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>

typedef struct ten_State ten_State;
typedef struct ten_DatInfo ten_DatInfo;

typedef struct {
    char const* tag;
    unsigned    size;
    unsigned    mems;
    void      (*destr)( void* buf );
} ten_DatConfig;

typedef struct ten_PtrInfo ten_PtrInfo;

typedef struct {
    char const* tag;
    void      (*destr)( void* addr );
} ten_PtrConfig;

typedef struct {
    char pri[32];
} ten_Tup;

typedef struct {
    ten_Tup* tup;
    unsigned loc;
} ten_Var;

#define ten_PARAMS ten_State* ten, ten_Tup* args, ten_Tup* mems, void* dat

typedef ten_Tup (*ten_FunCb)( ten_PARAMS );

typedef struct {
    char const*  name;
    char const** params;
    ten_FunCb    cb;
} ten_FunParams;


typedef enum {
    ten_ERR_NONE,
    ten_ERR_FATAL,
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
    ten_ERR_PANIC,
    ten_ERR_ASSERT
} ten_ErrNum;

typedef enum {
    ten_COM_CLS,
    ten_COM_FIB
} ten_ComType;

typedef enum {
    ten_SCOPE_LOCAL,
    ten_SCOPE_GLOBAL
} ten_ComScope;

typedef enum {
    ten_FIB_RUNNING,
    ten_FIB_WAITING,
    ten_FIB_STOPPED,
    ten_FIB_FINISHED,
    ten_FIB_FAILED
} ten_FibState;

typedef struct ten_Trace ten_Trace;
struct ten_Trace {
    char const* unit;
    char const* file;
    unsigned    line;
    ten_Trace*  next;
};

enum {
    ten_EOF = -1,
    ten_PAD = -2
};

typedef struct ten_Source {
    char const* name;
    int        (*next)( struct ten_Source* src );
    void       (*finl)( struct ten_Source* src );
} ten_Source;

typedef void* (*ten_MemFun)( void* udata,  void* old, size_t osz, size_t nsz );
typedef struct ten_Config {
    void*       udata;
    ten_MemFun  frealloc;
    
    bool ndebug;
    
    double memGrowth;
} ten_Config;


typedef struct {
    unsigned major;
    unsigned minor;
    unsigned patch;
} ten_Version;

extern ten_Version const ten_VERSION;

// Ten instance creation and destruction.
ten_State*
ten_make( ten_Config* config, jmp_buf* errJmp );

void
ten_free( ten_State* s );


// Stack manipulation.
ten_Tup
ten_pushA( ten_State* s, char const* pat, ... );

ten_Tup
ten_pushV( ten_State* s, char const* pat, va_list ap );

void
ten_pop( ten_State* s );

ten_Tup
ten_dup( ten_State* s, ten_Tup* tup );

unsigned
ten_size( ten_State* state, ten_Tup* tup );

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

void
ten_copy( ten_State* s, ten_Var* src, ten_Var* dst );

char const*
ten_string( ten_State* s, ten_Tup* tup );

void
ten_loader( ten_State* s, ten_Var* type, ten_Var* loadr, ten_Var* trans );

void
ten_panic( ten_State* s, ten_Var* val );

#define ten_call( S, CLS, ARGS ) \
    ten_call_( S, CLS, ARGS, __FILE__, __LINE__ )

ten_Tup
ten_call_( ten_State* s, ten_Var* cls, ten_Tup* args, char const* file, unsigned line );

void
ten_yield( ten_State* s, ten_Tup* vals );

long
ten_seek( ten_State* s, void* ctx, size_t size );

void
ten_checkpoint( ten_State* s, unsigned cp, ten_Tup* dst );

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

ten_Var*
ten_str( ten_State* s, char const* str );

// Sources.
ten_Source*
ten_fileSource( ten_State* s, FILE* file, char const* name );

ten_Source*
ten_pathSource( ten_State* s, char const* path );

ten_Source*
ten_stringSource( ten_State* s, char const* string, char const* name );


// Compilation.
void
ten_compileScript( ten_State* s, char const** upvals, ten_Source* src, ten_ComScope scope, ten_ComType out, ten_Var* dst );

void
ten_compileExpr( ten_State* s,  char const** upvals, ten_Source* src, ten_ComScope scope, ten_ComType out, ten_Var* dst );

// Execution.
void
ten_executeScript( ten_State* s, ten_Source* src, ten_ComScope scope );

ten_Tup
ten_executeExpr( ten_State* s, ten_Source* src, ten_ComScope scope );


// Singleton values.
bool
ten_isUdf( ten_State* s, ten_Var* var );

bool
ten_areUdf( ten_State* s, ten_Tup* tup );

ten_Var*
ten_udfType( ten_State* s );

bool
ten_isNil( ten_State* s, ten_Var* var );

bool
ten_areNil( ten_State* s, ten_Tup* tup );

ten_Var*
ten_nilType( ten_State* s );

// Logical values.
bool
ten_isLog( ten_State* s, ten_Var* var );

bool
ten_getLog( ten_State* s, ten_Var* var );

ten_Var*
ten_logType( ten_State* s );

// Integral values.
bool
ten_isInt( ten_State* s, ten_Var* var );

long
ten_getInt( ten_State* s, ten_Var* var );

ten_Var*
ten_intType( ten_State* s );

// Decimal values.
bool
ten_isDec( ten_State* s, ten_Var* var );

double
ten_getDec( ten_State* s, ten_Var* var );

ten_Var*
ten_decType( ten_State* s );

// Symbol values.
bool
ten_isSym( ten_State* s, ten_Var* var );

void
ten_setSym( ten_State* s, char const* sym, size_t len, ten_Var* dst );

char const*
ten_getSymBuf( ten_State* s, ten_Var* var );

size_t
ten_getSymLen( ten_State* s, ten_Var* var );

ten_Var*
ten_symType( ten_State* s );

// Pointer values.
bool
ten_isPtr( ten_State* s, ten_Var* var, ten_PtrInfo* info );

void
ten_setPtr( ten_State* s, void* addr, ten_PtrInfo* info, ten_Var* dst );

void*
ten_getPtrAddr( ten_State* s, ten_Var* var );

ten_PtrInfo*
ten_getPtrInfo( ten_State* s, ten_Var* var );

ten_PtrInfo*
ten_addPtrInfo( ten_State* s, ten_PtrConfig* config );

ten_Var*
ten_ptrType( ten_State* s, ten_PtrInfo* info );

// Strings objects.
bool
ten_isStr( ten_State* s, ten_Var* var );

void
ten_newStr( ten_State* s, char const* str, size_t len, ten_Var* dst );

char const*
ten_getStrBuf( ten_State* s, ten_Var* var );

size_t
ten_getStrLen( ten_State* s, ten_Var* var );

ten_Var*
ten_strType( ten_State* s );

// Index objects.
bool
ten_isIdx( ten_State* s, ten_Var* var );

void
ten_newIdx( ten_State* s, ten_Var* dst );

ten_Var*
ten_idxType( ten_State* s );

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

ten_Var*
ten_recType( ten_State* s );

// Function objects.
bool
ten_isFun( ten_State* s, ten_Var* var );

void
ten_newFun( ten_State* s, ten_FunParams* p, ten_Var* dst );

ten_Var*
ten_funType( ten_State* s );

// Closure objects.
bool
ten_isCls( ten_State* s, ten_Var* var );

void
ten_newCls( ten_State* s, ten_Var* fun, ten_Var* dat, ten_Var* dst );

void
ten_getUpvalue( ten_State* s, ten_Var* cls, unsigned upv, ten_Var* dst );

void
ten_setUpvalue( ten_State* s, ten_Var* cls, unsigned upv, ten_Var* src );

ten_Var*
ten_clsType( ten_State* s );

// Fiber objects.
bool
ten_isFib( ten_State* s, ten_Var* var );

void
ten_newFib( ten_State* s, ten_Var* cls, ten_Var* tag, ten_Var* dst );

ten_FibState
ten_state( ten_State* s, ten_Var* fib );

ten_Tup
ten_cont( ten_State* s, ten_Var* fib, ten_Tup* args );

ten_Var*
ten_fibType( ten_State* s );

// Data objects.
bool
ten_isDat( ten_State* s, ten_Var* var, ten_DatInfo* info );

void*
ten_newDat( ten_State* s, ten_DatInfo* info, ten_Var* dst );

void
ten_getDat( ten_State* s, void* buf, ten_Var* dst );

void
ten_setMember( ten_State* s, ten_Var* dat, unsigned mem, ten_Var* val );

void
ten_getMember( ten_State* s, ten_Var* dat, unsigned mem, ten_Var* dst );

ten_DatInfo*
ten_getDatInfo( ten_State* s, ten_Var* dat );

void*
ten_getDatBuf( ten_State* s, ten_Var* dat );

ten_DatInfo*
ten_addDatInfo( ten_State* s, ten_DatConfig* config );

ten_Var*
ten_datType( ten_State* s, ten_DatInfo* info );

ten_Tup
ten_members( ten_State* s, ten_Var* dat );

// Errors.

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

void
ten_propError( ten_State* s, ten_Var* fib );

jmp_buf*
ten_swapErrJmp( ten_State* s, jmp_buf* errJmp );

#endif
