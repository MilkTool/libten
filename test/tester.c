#include "../src/ten_state.h"
#include "../src/ten.h"

#include <stdlib.h>

#ifndef TEST_PATH
    #define TEST_PATH
#endif

char const* tests[] = {
    TEST_PATH "assign.ten",
    TEST_PATH "variadic.ten",
    TEST_PATH "conditional.ten",
    TEST_PATH "signal.ten",
    TEST_PATH "closure.ten",
    TEST_PATH "literal.ten",
    TEST_PATH "block.ten",
    TEST_PATH "convert.ten",
    TEST_PATH "operator.ten",
    TEST_PATH "type.ten",
    TEST_PATH "iter.ten",
    TEST_PATH "misc.ten",
    TEST_PATH "loop.ten",
    TEST_PATH "list.ten",
    TEST_PATH "fiber.ten",
    TEST_PATH "compile.ten",
    TEST_PATH "recurse.ten",
    TEST_PATH "stress.ten",
    NULL
};

int
main( void ) {
    stateTest();
    
    ten_State* ten = NULL;
    ten_Config cfg = { .debug = true };
    
    jmp_buf errJmp;
    int sig = setjmp( errJmp );
    if( sig ) {
        ten_ErrNum  err = ten_getErrNum( ten, NULL );
        char const* msg = ten_getErrStr( ten, NULL );
        fprintf( stderr, "Error: %s\n", msg );
        
        ten_Trace* tIt = ten_getTrace( ten, NULL );
        while( tIt ) {
            char const* fiber = "???";
            if( tIt->fiber )
                fiber = tIt->fiber;
            char const* file = "???";
            if( tIt->file )
                file = tIt->file;
            
            fprintf( stderr, "  %s#%u (%s)\n", file, tIt->line, fiber );
            tIt = tIt->next;
        }
        
        ten_free( ten );
        exit( 1 );
    }
    ten = ten_make( &cfg, &errJmp );
    
    for( uint i = 0 ; tests[i] != NULL ; i++ ) {
        fprintf( stderr, "Running '%s'...\n", tests[i] );
        ten_executeScript( ten, ten_pathSource( ten, tests[i] ), ten_SCOPE_LOCAL );
        fprintf( stderr, "  Passed\n" );
    }
    
    ten_free( ten );
    return 0;
}
