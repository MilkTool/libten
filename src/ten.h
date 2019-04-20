// This component defines the API presented to users for
// interacting with the Ten VM.
#ifndef ten_api_h
#define ten_api_h
#include <setjmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>

/* Represents an instance of the Ten runtime.  Since the runtime
 * doesn't keep any global data members, its global state goes in here.
 * A ten_State can be created with <ten_make> and should be released
 * with <ten_free> when no longer needed.
 */
typedef struct ten_State ten_State;

/* Represents a type of user data object, one of these should be
 * initialized with <ten_initDatInfo> before being passed to
 * <ten_newDat> to create a new Data object with the respective
 * info.
 */
typedef struct ten_DatInfo ten_DatInfo;


/* Configuration used to initialize <ten_DatInfo> instances, tells
 * Ten how to create, maintain, and destroy Data objects of the
 * associated type.
 */
typedef struct {
    /* A tag used to differentiate Data objects of different types,
     * the type name of a tagged Data object will have the form
     * 'Dat:tag'.
     */
    char const* tag;
    
    /* The size (in bytes) of the object's data buffer, this is
     * fixed per <ten_DatInfo> to avoid the memory overhead of
     * storing it in each Data object.
     */
    unsigned size;
    
    /* The number of 'members' to allocate for the object, these
     * are Ten variables for storing values associated with the
     * Data object.
     */
    unsigned mems;
    
    /* A function to be called before a Data object is freed by
     * the GC.  This should not free the given `buf` pointer.
     */
    void (*destr)( ten_State* s, void* buf );
} ten_DatConfig;


/* Represents the type of a pointer object.  One of these should
 * be initialized with <ten_initPtrInfo> before being passed to
 * <ten_setPtr> to create a pointer with the respective info.
 */
typedef struct ten_PtrInfo ten_PtrInfo;

/* Configuration for initializing a <ten_PtrInfo>, it tells Ten
 * how to create, maintain, and destroy the associated pointers.
 */
typedef struct {
    /* A tag used to differentiate between different pointer types,
     * the type name of a tagged pointer will have the form 'Ptr:tag'.
     */
    char const* tag;
    
    /* A function to be called when all instances of a given pointer
     * (with the same address and <ten_PtrInfo>) have expired from
     * the VM.
     */
    void (*destr)( ten_State* s, void* addr );
} ten_PtrConfig;

/* An array of internal Ten values, allows the user to reference
 * internal VM state without interfering with the GC or depending
 * on internal structures.
 */
typedef struct {
    char pri[32];
} ten_Tup;


/* A specific variable within a <ten_Tup>, instances of this
 * struct will be passed to most API functions to pass in
 * Ten values.  The `tup` and `loc` fields should be field
 * in by the user with a tuple and the variable's position
 * within the tuple respectively.
 */
typedef struct {
    ten_Tup* tup;
    unsigned loc;
} ten_Var;

/* Expands to the parameter list of a <ten_FunCb> callback.
 */
#define ten_PARAMS ten_State* ten, ten_Tup* args, ten_Tup* mems, void* dat

/* Callback type for native functions to be used in Ten code.
 */
typedef ten_Tup (*ten_FunCb)( ten_PARAMS );


/* Parameters for bulding a Ten function from a native callback.
 */
typedef struct {
    /* The name of the function, used for error reporting and such,
     * can be omited with NULL.
     */
    char const*  name;
    
    /* A NULL terminated list of parameter names, used for error
     * reporting and such.  This is required since it's also used
     * to determine the number of parameters expected by the
     * callback.
     * The last parameter name can end in '...' to indicate that
     * it's a variadic parameter.
     */
    char const** params;
    
    /* The actual function callback.
     */
    ten_FunCb cb;
} ten_FunParams;


typedef enum {
    ten_ERR_NONE,
    #ifdef ten_TEST
        ten_ERR_TEST,
    #endif
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
    ten_ERR_PANIC
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

typedef struct ten_Source {
    char const* name;
    int        (*next)( struct ten_Source* src );
    void       (*finl)( struct ten_Source* src );
} ten_Source;

typedef void* (*FreallocFun)( void* udata,  void* old, size_t osz, size_t nsz );
typedef struct ten_Config {
    void*       udata;
    FreallocFun frealloc;
    
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

ten_Tup
ten_top( ten_State* s );

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


#define ten_call( S, CLS, ARGS ) \
    ten_call_( S, CLS, ARGS, __FILE__, __LINE__ )

void
ten_panic( ten_State* s, ten_Var* val );

ten_Tup
ten_call_( ten_State* s, ten_Var* cls, ten_Tup* args, char const* file, unsigned line );


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

void
ten_freeSource( ten_State* s, ten_Source* src );


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

void
ten_setUdf( ten_State* s, ten_Var* dst );

bool
ten_isNil( ten_State* s, ten_Var* var );

bool
ten_areNil( ten_State* s, ten_Tup* tup );

void
ten_setNil( ten_State* s, ten_Var* dst );

ten_Var*
ten_nilType( ten_State* s );

// Logical values.
bool
ten_isLog( ten_State* s, ten_Var* var );

void
ten_setLog( ten_State* s, bool log, ten_Var* dst );

bool
ten_getLog( ten_State* s, ten_Var* var );

ten_Var*
ten_logType( ten_State* s );

// Integral values.
bool
ten_isInt( ten_State* s, ten_Var* var );

void
ten_setInt( ten_State* s, long in, ten_Var* dst );

long
ten_getInt( ten_State* s, ten_Var* var );

ten_Var*
ten_intType( ten_State* s );

// Decimal values.
bool
ten_isDec( ten_State* s, ten_Var* var );

void
ten_setDec( ten_State* s, double dec, ten_Var* dst );

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

// Data objects.
bool
ten_isDat( ten_State* s, ten_Var* var, ten_DatInfo* info );

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

void*
ten_getDatBuf( ten_State* s, ten_Var* dat );

ten_DatInfo*
ten_addDatInfo( ten_State* s, ten_DatConfig* config );

ten_Var*
ten_datType( ten_State* s, ten_DatInfo* info );

#endif
