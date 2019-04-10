#ifndef ten_assert_h
#define ten_assert_h

#define ASSERT_STATE state

#ifdef ten_DEBUG

    #include <stdio.h>
    #include <stdlib.h>
    #include "ten_fmt.h"
    
    #define tenAssert( COND )                                           \
        do {                                                            \
            if( !(COND) ) {                                             \
                fprintf(                                                \
                    stderr, "Assertion failed[%s %u]: %s\n",            \
                    __FILE__, (uint)__LINE__, #COND                     \
                );                                                      \
                abort();                                                \
            }                                                           \
        } while( 0 )

    #define strAssert( COND, STR )                                      \
        do {                                                            \
            if( !(COND) ) {                                             \
                fprintf(                                                \
                    stderr, "Assertion failed[%s %u]: %s\n",            \
                    __FILE__, (uint)__LINE__, STR                       \
                );                                                      \
                abort();                                                \
            }                                                           \
        } while( 0 )
    
    #define fmtAssert( COND, FMT, ARGS... )                             \
        do {                                                            \
            if( !(COND) ) {                                             \
                fmtA( state, false, FMT, ARGS );                        \
                fprintf(                                                \
                    stderr, "Assertion failed[%s %u]: %s\n",            \
                    __FILE__, (uint)__LINE__, fmtBuf( state )           \
                );                                                      \
                abort();                                                \
            }                                                           \
        } while( 0 )

    #define funAssert( COND, FMT, ARGS... )                             \
        do {                                                            \
            if( !(COND) ) {                                             \
                fmtA( state, false, FMT, ARGS );                        \
                fprintf(                                                \
                    stderr, "Assertion failed[%s]: %s\n",               \
                    __func__, fmtBuf( state )                           \
                );                                                      \
                abort();                                                \
            }                                                           \
        } while( 0 )
    
    #define expAssert( COND, RES, FMT, ARGS... )                        \
        ((COND)                                                         \
            ? (RES)                                                     \
            : (                                                         \
                fmtA( state, false, FMT, ARGS ),                        \
                fprintf(                                                \
                    stderr, "Assertion failed[%s %u]: %s\n",            \
                    __FILE__, (uint)__LINE__, fmtBuf( state )           \
                ),                                                      \
                abort(),                                                \
                (RES)                                                   \
            )                                                           \
        )
        
    #define tenAssertNeverReached()                                 \
        do {                                                        \
            fprintf(                                                \
                stderr,                                             \
                "Assertion failed[%s %u]: Broken control flow\n",   \
                __FILE__, (uint)__LINE__                            \
            );                                                      \
            abort();                                                \
        } while( 0 )
#else
    
    #define tenAssert( COND )

    #define strAssert( COND, STR )
    
    #define fmtAssert( COND, FMT, ARGS... )
    
    #define funAssert( COND, FMT, ARGS... )
    
    #define expAssert( COND, RES, FMT, ARGS... ) (RES)
    
    #define tenAssertNeverReached()
#endif

#endif
