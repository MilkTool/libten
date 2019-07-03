#include <ten.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int
main( int argc, char const** argv ) {
    if( argc < 2 ) {
        fprintf( stderr, "Usage: %s tests...\n", argv[0] );
        exit( 1 );
    }
    
    // Initialize the runtime.
    ten_State* volatile ten = NULL;
    jmp_buf             jmp;
    if( setjmp( jmp ) ) {
        //ten_ErrNum  err = ten_getErrNum( ten, NULL );
        char const* msg = ten_getErrStr( ten, NULL );
        fprintf( stderr, "Error: %s\n", msg );
        
        ten_Trace* tIt = ten_getTrace( ten, NULL );
        while( tIt ) {
            char const* unit = "???";
            if( tIt->unit )
                unit = tIt->unit;
            char const* file = "???";
            if( tIt->file )
                file = tIt->file;
            
            fprintf(
                stderr,
                "  unit: %-10s line: %-4u file: %-20s\n",
                unit, tIt->line, file
            );
            tIt = tIt->next;
        }
        
        ten_free( ten );
        exit( 1 );
    }
    ten = ten_make( NULL, &jmp );
    
    // Run initialization script.
    ten_Source* initSrc = ten_pathSource( ten, "init.ten" );
    ten_executeScript( ten, initSrc, ten_SCOPE_GLOBAL );

    // Run the tests.
    for( unsigned i = 1 ; argv[i] != NULL ; i++ ) {
        ten_Source* testSrc = ten_pathSource( ten, argv[i] );
        printf( "\n\n" );
        printf( "File: %s\n", argv[i] );
        printf( "==========================================\n" );
        
        ten_executeScript( ten, testSrc, ten_SCOPE_LOCAL );
    }
    
    ten_free( ten );
    return 0;
}
