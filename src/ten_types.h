// This file defines the types used globally throughout
// the language implementation.  Other files will define
// there own local types.  Most of the structs that we
// typedef here will also be defined elsewhere, we just
// put all the typedefs together here to avoid having to
// repeat them in other files since C99 doesn't allow
// redefinitions of typedefs.

#ifndef ten_types_h
#define ten_types_h
#include <stdint.h>

// State structures for each VM component, and for the VM as a whole.
typedef struct State    State;
typedef struct ComState ComState;
typedef struct SymState SymState;
typedef struct PtrState PtrState;
typedef struct GenState GenState;
typedef struct EnvState EnvState;
typedef struct FmtState FmtState;
typedef struct StrState StrState;
typedef struct IdxState IdxState;
typedef struct RecState RecState;
typedef struct FunState FunState;
typedef struct ClsState ClsState;
typedef struct FibState FibState;
typedef struct UpvState UpvState;
typedef struct DatState DatState;
typedef struct LibState LibState;
typedef struct ApiState ApiState;

// These are all the types of heap allocated objects.
typedef struct String   String;
typedef struct Index    Index;
typedef struct Record   Record;
typedef struct Function Function;
typedef struct Closure  Closure;
typedef struct Fiber    Fiber;
typedef struct Upvalue  Upvalue;
typedef struct Data     Data;

// Types of lookup tables.
typedef struct NTab NTab;
typedef struct STab STab;

// Code generator type.
typedef struct Gen Gen;

// Primitive types, just give them shorter names.
typedef unsigned char      uchar;
typedef unsigned short     ushort;
typedef unsigned int       uint;
typedef unsigned long      ulong;
typedef unsigned long long ullong;
typedef long long          llong;

// Type used for bytecode instructions.
typedef unsigned short instr;
#define IN_OPC_MAX (127)
#define IN_OPR_MAX (511)
#define inMake( OPC, OPR ) ((OPR) << 7 | (OPC))
#define inGetOpc( IN )     ((IN) & 0x7F)
#define inGetOpr( IN )     ((IN) >> 7)

// These are the types we use to represent the primitive values.
typedef void*    ObjT;
typedef _Bool    LogT;
typedef int32_t  IntT;
typedef double   DecT;
typedef ullong   SymT;
typedef ullong   PtrT;
typedef ullong   TupT;
typedef ullong   RefT;

typedef enum {
    REF_GLOBAL,
    REF_UPVAL,
    REF_LOCAL,
    REF_CLOSED
} RefTag;

#define REF_MAX (0xFFFFFFFFFC)
#define refMake( TAG, LOC ) ((RefT)(LOC) << 2 | (TAG))
#define refGetTag( REF )    ((REF) & 0x3)
#define refGetLoc( REF )    ((REF) >> 2)

#define refSet( REF, VAL )                                                  \
do {                                                                        \
    RefT tag = refGetTag( REF );                                            \
    RefT loc = refGetLoc( REF );                                            \
                                                                            \
    switch( tag ) {                                                         \
        case REF_GLOBAL: {                                                  \
            TVal* ptr = envGetGlobalByLoc( state, loc );                    \
            tenAssert( ptr );                                               \
            if( tvIsUdf( *ptr ) )                                           \
                stateErrFmtA(                                               \
                    state, ten_ERR_ASSIGN,                                  \
                    "Mutation of undefined variable"                        \
                );                                                          \
            Upvalue* upv = tvGetObj( *ptr );                                \
            if( tvIsObj( *ptr ) && datGetTag( (void*)upv ) == OBJ_UPV )     \
                upv->val = (VAL);                                           \
            else                                                            \
                *ptr = (VAL);                                               \
        } break;                                                            \
        case REF_UPVAL: {                                                   \
            tenAssert( loc < regs.cls->fun->u.vir.nUpvals );                \
            Upvalue** upvals  = regs.cls->dat.upvals;                       \
            if( !upvals[loc] || tvIsUdf( upvals[loc]->val ) )               \
                stateErrFmtA(                                               \
                    state, ten_ERR_ASSIGN,                                  \
                    "Mutation of undefined varaible"                        \
                );                                                          \
            upvals[loc]->val = (VAL);                                       \
        } break;                                                            \
        case REF_LOCAL: {                                                   \
            tenAssert(                                                      \
                loc < regs.cls->fun->u.vir.nLocals + regs.cls->fun->nParams + 1 \
            );                                                              \
            if( tvIsUdf( regs.lcl[loc] ) )                                  \
                stateErrFmtA(                                               \
                    state, ten_ERR_ASSIGN,                                  \
                    "Mutation of undefined varaible"                        \
                );                                                          \
            regs.lcl[loc] = (VAL);                                          \
        } break;                                                            \
        case REF_CLOSED: {                                                  \
            tenAssert(                                                      \
                loc < regs.cls->fun->u.vir.nLocals + regs.cls->fun->nParams + 1 \
            );                                                              \
            tenAssert( tvIsObj( regs.lcl[loc] ) );                          \
            tenAssert( datGetTag( tvGetObj( regs.lcl[loc] ) ) == OBJ_UPV ); \
            Upvalue* upv = tvGetObj( regs.lcl[loc] );                       \
            if( tvIsUdf( upv->val ) )                                       \
                stateErrFmtA(                                               \
                    state, ten_ERR_ASSIGN,                                  \
                    "Mutation of undefined varaible"                        \
                );                                                          \
            upv->val = (VAL);                                               \
        } break;                                                            \
        default:                                                            \
            tenAssertNeverReached();                                        \
        break;                                                              \
    }                                                                       \
} while( 0 )

#define refDef( REF, VAL )                                                  \
do {                                                                        \
    RefT tag = refGetTag( REF );                                            \
    RefT loc = refGetLoc( REF );                                            \
                                                                            \
    switch( tag ) {                                                         \
        case REF_GLOBAL: {                                                  \
            TVal* ptr = envGetGlobalByLoc( state, loc );                    \
            tenAssert( ptr );                                               \
            *ptr = (VAL);                                                   \
        } break;                                                            \
        case REF_UPVAL: {                                                   \
            tenAssertNeverReached();                                        \
        } break;                                                            \
        case REF_LOCAL: {                                                   \
            tenAssert(                                                      \
                loc < regs.cls->fun->u.vir.nLocals + regs.cls->fun->nParams + 1 \
            );                                                              \
            regs.lcl[loc] = (VAL);                                          \
        } break;                                                            \
        case REF_CLOSED: {                                                  \
            tenAssert(                                                      \
                loc < regs.cls->fun->u.vir.nLocals + regs.cls->fun->nParams + 1 \
            );                                                              \
            regs.lcl[loc] = (VAL);                                          \
        } break;                                                            \
        default:                                                            \
            tenAssertNeverReached();                                        \
        break;                                                              \
    }                                                                       \
} while( 0 )

#define refUpv( REF, UPV )                                             \
do {                                                                        \
    RefT tag = refGetTag( REF );                                            \
    RefT loc = refGetLoc( REF );                                            \
                                                                            \
    switch( tag ) {                                                         \
        case REF_GLOBAL: {                                                  \
            TVal* ptr = envGetGlobalByLoc( state, loc );                    \
            tenAssert( ptr );                                               \
            Upvalue* upv = tvGetObj( *ptr );                                \
            if( tvIsObj( *ptr ) && datGetTag( (void*)upv ) == OBJ_UPV ) {   \
                (UPV) = upv;                                                \
            }                                                               \
            else                                                            \
            if( tvIsUdf( *ptr ) ) {                                         \
                (UPV) = NULL;                                               \
            }                                                               \
            else {                                                          \
                upv = upvNew( state, *ptr );                                \
                *ptr = tvObj( upv );                                        \
                (UPV) = upv;                                                \
            }                                                               \
        } break;                                                            \
        case REF_UPVAL: {                                                   \
            tenAssert( loc < regs.cls->fun->u.vir.nUpvals );                \
            Upvalue** upvals  = regs.cls->dat.upvals;                       \
            (UPV) = upvals[loc];                                            \
        } break;                                                            \
        case REF_LOCAL: {                                                   \
            tenAssert(                                                      \
                loc < regs.cls->fun->u.vir.nLocals + regs.cls->fun->nParams + 1 \
            );                                                              \
                                                                            \
            Upvalue* upv = upvNew( state, regs.lcl[loc] );                  \
            regs.lcl[loc] = tvObj( upv );                                   \
            (UPV) = upv;                                                    \
        } break;                                                            \
        case REF_CLOSED: {                                                  \
            tenAssert(                                                      \
                loc < regs.cls->fun->u.vir.nLocals + regs.cls->fun->nParams + 1 \
            );                                                              \
            tenAssert( tvIsObj( regs.lcl[loc] ) );                          \
            tenAssert( datGetTag( tvGetObj( regs.lcl[loc] ) ) == OBJ_UPV ); \
            (UPV) = tvGetObj( regs.lcl[loc] );                              \
        } break;                                                            \
        default:                                                            \
            tenAssertNeverReached();                                        \
        break;                                                              \
    }                                                                       \
} while( 0 )


// Due to the way we encode short symbols directly into a value
// as the OR of its characters... we'd get a lot of collision
// if we just use the raw value as a hash key, since symbols
// with the same prefix would collide often.  So we define a
// static hash function for symbols, here, this'll be used in
// tvHash() defined below.
#define bhash( SALT, BYTE ) ( (SALT)*37 + (BYTE) )
#define ahash( BYTES )                      \
    bhash(                                  \
        bhash(                              \
            bhash(                          \
                bhash(                      \
                    bhash( 0LLU, BYTES[0] ),\
                    BYTES[1]                \
                ),                          \
                BYTES[2]                    \
            ),                              \
            BYTES[3]                        \
        ),                                  \
        BYTES[4]                            \
    )

#define shash( SYM ) ahash( ((char*)&(SYM)) )


// This type is used to attach a tag with some extra
// data to the unused bits of a pointer to save some
// memory.  We have two definitions, since we'll want
// an actual struct when debugging to make things
// easier, and the tagged pointer will usually take
// about the same amount of memory as the struct on
// a 32bit system; and doesn't come with the extra
// compute overhead.
#define TP_TAG_MAX (0xFFFF)
#define TP_PTR_MAX (0xFFFFFFFFFFFF)
#ifndef ten_NO_POINTER_TAGS

    typedef ullong TPtr;
    
    #define tpMake( TAG, PTR ) ((ullong)(TAG) << 48 | (ullong)(PTR))
    #define tpGetTag( TPTR )   ((ushort)((TPTR) >> 48))
    #define tpGetPtr( TPTR )   ((void*)((TPTR) << 16 >> 16))

#else

    typedef struct {
        void*  ptr;
        ushort tag;
    } TPtr;

    #define tpMake( TAG, PTR ) (TPtr){ .tag = (ushort)(TAG), .ptr = (void*)(PTR) }
    #define tpGetTag( TPTR )   ((TPTR).tag)
    #define tpGetPtr( TPTR )   ((TPTR).ptr)
    
#endif


// This is the tagged value type used to represent Ten's values,
// it can also be defined as either a struct or a NaN encoded
// double float.  Basically how this works is we take advantage
// of the fact that floating point numbers have a lot of different
// binary values that all map to NaN (Not a Number).  So when the
// float is NaN (not being used as a number), we can use it to
// represent our other types.  A double precision float is
// represented as 64bits mapped as follows:
//
// [s][eeeeeeeeeee][ffffffffffffffffffffffffffffffffffffffffffffffffffff]
//  1       11                               52
//
// The highest bit indicates the number's sign, then 11 bits of exponent,
// and 52 bits of fraction.  The actual meaning of these isn't so
// important as the way NaN is represented.  A double float is considered
// to be a NaN value if all exponent bits [e] are set to 1, and the
// fraction bits [f] aren't all zero.  So if we aren't representing a
// number then we set these bits accordingly, and we now have 52 bits
// to store our other types.  Since some of these types will be pointers
// to heap objects, we need at least enough bits to represent a valid
// address, in addition to the bits we use to indicate the value type.
// Fortunately most systems will never use more than 48 bits in an
// address... but to be safe Ten dedicates the whole 52 bits to the
// address by including the sign bit in its tag.
//
// So if the sign bit [s] is set, then the value is interpretted as a
// pointer to an object on the heap, the object's header will contain
// further type info.  If the sign bit is cleared then the fraction
// bits represent an immediate value along with its type tag; we give
// the highest 4 bits to the tag and the low 48 to the value, and
// just define the type tags so that none of them are zero to keep
// the fraction from ever being so (if the fraction is zero then
// the number represents infinity instead of NaN).

#ifndef ten_NO_NAN_TAGS
    
    
    typedef union {
        double num;
        ullong nan;
    } TVal;
    
    #define NAN_BITS (0x7FFLLU << 52)
    #define TAG_BITS (0xFLLU << 48)
    #define VAL_BITS (0xFFFFFFFFFFFFLLU)
    #define OBJ_BITS (0xFFFFFFFFFFFFFLLU)
    #define SIGN_BIT (1LLU << 63)
    
    #define tvObj( OBJ ) \
        (TVal){.nan = SIGN_BIT | NAN_BITS | (ullong)(OBJ)}
    #define tvUdf() \
        (TVal){.nan = NAN_BITS | (ullong)VAL_UDF << 48 | 0LLU }
    #define tvNil() \
        (TVal){.nan = NAN_BITS | (ullong)VAL_NIL << 48 | 0LLU }
    #define tvLog( LOG ) \
        (TVal){.nan = NAN_BITS | (ullong)VAL_LOG << 48 | !!(LOG) }
    #define tvDec( DEC ) \
        (TVal){.num = (DEC)}
    #define tvInt( INT ) \
        (TVal){.nan = NAN_BITS | (ullong)VAL_INT << 48 | (uint32_t)(IntT)(INT) }
    #define tvSym( SYM ) \
        (TVal){.nan = NAN_BITS | (ullong)VAL_SYM << 48 | (SymT)(SYM) }
    #define tvPtr( PTR ) \
        (TVal){.nan = NAN_BITS | (ullong)VAL_PTR << 48 | (PtrT)(PTR) }
    #define tvTup( TUP ) \
        (TVal){.nan = NAN_BITS | (ullong)VAL_TUP << 48 | (TupT)(TUP) }
    #define tvRef( REF ) \
        (TVal){.nan = NAN_BITS | (ullong)VAL_REF << 48 | (RefT)(REF) }
    
    #define tvIsObj( TVAL ) \
        (!tvIsDec( TVAL ) && ((TVAL).nan & SIGN_BIT))
    #define tvIsUdf( TVAL ) \
        (!tvIsDec( TVAL ) && ((TVAL).nan & TAG_BITS) >> 48 == VAL_UDF)
    #define tvIsNil( TVAL ) \
        (!tvIsDec( TVAL ) && ((TVAL).nan & TAG_BITS) >> 48 == VAL_NIL)
    #define tvIsLog( TVAL ) \
        (!tvIsDec( TVAL ) && ((TVAL).nan & TAG_BITS) >> 48 == VAL_LOG)
    #define tvIsInt( TVAL ) \
        (!tvIsDec( TVAL ) && ((TVAL).nan & TAG_BITS) >> 48 == VAL_INT)
    #define tvIsDec( TVAL ) \
        (((TVAL).nan << 1 >> 1) <= NAN_BITS)
    #define tvIsSym( TVAL ) \
        (!tvIsDec( TVAL ) && ((TVAL).nan & TAG_BITS) >> 48 == VAL_SYM)
    #define tvIsPtr( TVAL ) \
        (!tvIsDec( TVAL ) && ((TVAL).nan & TAG_BITS) >> 48 == VAL_PTR)
    #define tvIsTup( TVAL ) \
        (!tvIsDec( TVAL ) && ((TVAL).nan & TAG_BITS) >> 48 == VAL_TUP)
    #define tvIsRef( TVAL ) \
        (!tvIsDec( TVAL ) && ((TVAL).nan & TAG_BITS) >> 48 == VAL_REF)
    
    #define tvGetTag( TVAL ) \
        (tvIsDec( TVAL ) ? VAL_DEC : ((TVAL).nan & TAG_BITS) >> 48)
    #define tvGetVal( TVAL ) \
        (tvIsDec( TVAL ) ? (TVAL).nan : (ullong)((TVAL).nan & VAL_BITS))
    #define tvGetObj( TVAL ) \
        ((ObjT)((TVAL).nan & OBJ_BITS))
    #define tvGetLog( TVAL ) \
        ((TVAL).nan & VAL_BITS)
    #define tvGetInt( TVAL ) \
        ((IntT)(uint32_t)((TVAL).nan & VAL_BITS))
    #define tvGetDec( TVAL ) \
        ((TVAL).num)
    #define tvGetSym( TVAL ) \
        ((SymT)((TVAL).nan & VAL_BITS))
    #define tvGetPtr( TVAL ) \
        ((PtrT)((TVAL).nan & VAL_BITS))
    #define tvGetTup( TVAL ) \
        ((TupT)((TVAL).nan & VAL_BITS))
    #define tvGetRef( TVAL ) \
        ((RefT)((TVAL).nan & VAL_BITS))
    
    #define tvEqual( TV1, TV2 ) \
        ((TV1).nan == (TV2).nan)
    
    #define tvHash( TVAL ) \
        (tvIsSym( TVAL ) ? shash( (TVAL).nan ) : (TVAL).nan )

    #define tvMark( TVAL )                                          \
        switch( tvGetTag( TVAL ) ) {                                \
            case VAL_OBJ:                                           \
                stateMark( state, tvGetObj(TVAL) );                 \
            break;                                                  \
            case VAL_SYM:                                           \
                if( state->gcFull )                                 \
                    symMark( state, tvGetSym(TVAL) );               \
            break;                                                  \
            case VAL_PTR:                                           \
                if( state->gcFull )                                 \
                    ptrMark( state, tvGetPtr(TVAL) );               \
            break;                                                  \
            default:                                                \
                tenAssert( tvGetTag( TVAL ) < VAL_LAST );           \
            break;                                                  \
        }
#else

    typedef struct {
        union {
            ullong val;
            double num;
        } u;
        int tag;
    } TVal;
    
    #define tvObj( OBJ ) \
        (TVal){.tag = VAL_OBJ, .u = {.val = (ullong)(OBJ) }}
    #define tvUdf() \
        (TVal){.tag = VAL_UDF, .u = {.val = 0LLU}}
    #define tvNil() \
        (TVal){.tag = VAL_NIL, .u = {.val = 0LLU}}
    #define tvLog( LOG ) \
        (TVal){.tag = VAL_LOG, .u = {.val = !!(LOG)}}
    #define tvDec( DEC ) \
        (TVal){.tag = VAL_DEC, .u = {.num = (DEC)}}
    #define tvInt( INT ) \
        (TVal){.tag = VAL_INT, .u = {.val = (IntT)(INT) }}
    #define tvSym( SYM ) \
        (TVal){.tag = VAL_SYM, .u = {.val = (SymT)(SYM)}}
    #define tvPtr( SYM ) \
        (TVal){.tag = VAL_PTR, .u = {.val = (PtrT)(SYM)}}
    #define tvTup( TUP ) \
        (TVal){.tag = VAL_TUP, .u = {.val = (TupT)(TUP)}}
    #define tvRef( REF ) \
        (TVal){.tag = VAL_REF, .u = {.val = (RefT)(REF)}}
    
    #define tvIsObj( TVAL ) \
        ((TVAL).tag == VAL_OBJ)
    #define tvIsUdf( TVAL ) \
        ((TVAL).tag == VAL_UDF)
    #define tvIsNil( TVAL ) \
        ((TVAL).tag == VAL_NIL)
    #define tvIsLog( TVAL ) \
        ((TVAL).tag == VAL_LOG)
    #define tvIsInt( TVAL ) \
        ((TVAL).tag == VAL_INT)
    #define tvIsDec( TVAL ) \
        ((TVAL).tag == VAL_DEC)
    #define tvIsSym( TVAL ) \
        ((TVAL).tag == VAL_SYM)
    #define tvIsPtr( TVAL ) \
        ((TVAL).tag == VAL_PTR)
    #define tvIsTup( TVAL ) \
        ((TVAL).tag == VAL_TUP)
    #define tvIsRef( TVAL ) \
        ((TVAL).tag == VAL_REF)
    
    #define tvGetTag( TVAL ) \
        ((TVAL).tag)
    #define tvGetVal( TVAL ) \
        ((TVAL).u.val)
    #define tvGetObj( TVAL ) \
        ((ObjT)(TVAL).u.val)
    #define tvGetLog( TVAL ) \
        ((TVAL).u.val)
    #define tvGetInt( TVAL ) \
        ((IntT)(TVAL).u.val)
    #define tvGetDec( TVAL ) \
        ((TVAL).u.num)
    #define tvGetSym( TVAL ) \
        ((SymT)(TVAL).u.val)
    #define tvGetPtr( TVAL ) \
        ((PtrT)(TVAL).u.val)
    #define tvGetTup( TVAL ) \
        ((TupT)(TVAL).u.val)
    #define tvGetRef( TVAL ) \
        ((RefT)(TVAL).u.val)
    
    #define tvEqual( TV1, TV2 ) \
        ((TV1).tag == (TV2).tag && (TV1).u.val == (TV2).u.val)
    
    #define tvHash( TVAL ) \
        (tvIsSym( TVAL )? shash( (TVAL).u.val ) : (TVAL).tag*(TVAL).u.val)

    #define tvMark( TVAL )                                          \
        switch( (TVAL).tag ) {                                      \
            case VAL_OBJ:                                           \
                stateMark( state, tvGetObj(TVAL) );                 \
            break;                                                  \
            case VAL_SYM:                                           \
                if( state->gcFull )                                 \
                    symMark( state, tvGetSym(TVAL) );               \
            break;                                                  \
            case VAL_PTR:                                           \
                if( state->gcFull )                                 \
                    ptrMark( state, tvGetPtr(TVAL) );               \
            break;                                                  \
            default:                                                \
            break;                                                  \
        }
#endif

#define tvIsObjType( TVAL, TYPE ) \
    (tvIsObj( TVAL ) && datGetTag( tvGetObj( TVAL ) ) == (TYPE))

// Type tags used directly in the TVal.  The VAL_OBJ should
// have the zero tag since it'll never be used in a NaN
// encoded value; for which the tag must always be non-zero.
typedef enum {
    VAL_OBJ,
    VAL_SYM,
    VAL_PTR,
    VAL_UDF,
    VAL_NIL,
    VAL_LOG,
    VAL_INT,
    VAL_DEC,
    VAL_TUP,
    VAL_REF,
    VAL_LAST
} ValTag;

// Type tags used to differentiate between different types
// of heap objects.
typedef enum {
    OBJ_STR = VAL_LAST,
    OBJ_IDX,
    OBJ_REC,
    OBJ_FUN,
    OBJ_CLS,
    OBJ_FIB,
    OBJ_UPV,
    OBJ_DAT,
    #ifdef ten_TEST
        OBJ_TST,
    #endif
    OBJ_LAST
} ObjTag;


// The Tup struct is used to represent a reference to a sequence
// of values in a way that's GC safe and allows the value array
// to be reallocated.  So we store a pointer to the array's base,
// an offset to the start of the array, and the size of the array.
#define TUP_MAX (32)
typedef struct {
    TVal** base;
    uint   offset;
    uint   size;
} Tup;

#define tupAt( TUP, LOC ) ( *(*(TUP).base + (TUP).offset + (LOC)) )

#endif
