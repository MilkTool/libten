#ifndef ten_assert_h
#define ten_assert_h

#define ASSERT_STATE state

#ifndef ten_NDEBUG

    #define tenAssert( COND )                                           \
        do {                                                            \
            if( !(COND) ) {                                             \
                statePushTrace( ASSERT_STATE, __FILE__, __LINE__ );     \
                                                                        \
                stateErrFmtA(                                           \
                    ASSERT_STATE, ten_ERR_ASSERT,                       \
                    "Assertion `"  #COND "` failed"                     \
                );                                                      \
            }                                                           \
        } while( 0 )

    #define strAssert( COND, STR )                                      \
        do {                                                            \
            if( !(COND) ) {                                             \
                statePushTrace( ASSERT_STATE, __FILE__, __LINE__ );     \
                stateErrStr( ASSERT_STATE, ten_ERR_ASSERT, -1 );        \
            }                                                           \
        } while( 0 )
    
    #define fmtAssert( COND, FMT, ARGS... )                             \
        do {                                                            \
            if( !(COND) ) {                                             \
                statePushTrace( ASSERT_STATE, __FILE__, __LINE__ );     \
                stateErrFmt( ASSERT_STATE, ten_ERR_ASSERT, FMT, ARGS ); \
            }                                                           \
        } while( 0 )
    
    #define expAssert( COND, RES, FMT, ARGS... )                        \
        ((COND)                                                         \
            ? (RES)                                                     \
            : (                                                         \
                statePushTrace( ASSERT_STATE, __FILE__, __LINE__ ),     \
                stateErrFmtA( ASSERT_STATE, ten_ERR_ASSERT, FMT, ARGS );\
            )                                                           \
        )
        
    #define tenAssertNeverReached()                                 \
        do {                                                        \
            statePushTrace( ASSERT_STATE, __FILE__, __LINE__ );     \
            stateErrFmtA(                                           \
                ASSERT_STATE, ten_ERR_ASSERT,                       \
                "Control flow is broken somewhere"                  \
            );                                                      \
        } while( 0 )
#else
    #define tenAssert( COND )

    #define strAssert( COND, STR )
    
    #define fmtAssert( COND, FMT, ARGS... )
    
    #define expAssert( COND, RES, FMT, ARGS... )
    
    #define tenAssertNeverReached()  
#endif

#endif
