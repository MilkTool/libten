#include "../ten_state.h"
#include "../ten_api.h"

#include <stdlib.h>

char const* tests[] = {
    "assign.ten",
    "variadic.ten",
    "conditional.ten",
    "signal.ten",
    "closure.ten",
    "literal.ten",
    "block.ten",
    "convert.ten",
    "operator.ten",
    "type.ten",
    "iter.ten",
    "misc.ten",
    "loop.ten",
    "list.ten",
    "fiber.ten",
    "compile.ten",
    NULL
};

int
main( void ) {
    stateTest();
    
    ten_State  s;
    ten_Config c = { .debug = true };
    
    jmp_buf errJmp;
    int err = setjmp( errJmp );
    if( err ) {
        char const* msg = ten_getErrStr( &s, NULL );
        fprintf( stderr, "Error: %s\n", msg );
        
        ten_Trace* tIt = ten_getTrace( &s, NULL );
        while( tIt ) {
            char const* fiber = "???";
            if( tIt->fiber )
                fiber = tIt->fiber;
            char const* file = "???";
            if( tIt->file )
                file = tIt->file;
            
            fprintf( stderr, "  %s#%u (%s)\n", tIt->file, tIt->line, tIt->fiber );
            tIt = tIt->next;
        }
        
        ten_finl( &s );
        exit( 1 );
    }
    ten_init( &s, &c, &errJmp );
    
    for( uint i = 0 ; tests[i] != NULL ; i++ ) {
        fprintf( stderr, "Running '%s'...\n", tests[i] );
        ten_executePath( &s, tests[i], ten_SCOPE_LOCAL );
        fprintf( stderr, "  Finished\n" );
    }
    
    ten_finl( &s );
    return 0;
}
