#include "ten_str.h"
#include "ten_state.h"
#include "ten_assert.h"
#include <string.h>

void
strInit( State* state ) {
    state->strState = NULL;
}

#ifdef ten_TEST
void
strTest( State* state ) {
    char const raw1[] = "Hello, World!";
    String*    str1   = strNew( state, raw1, sizeof(raw1)-1 );
    size_t     len1   = strLen( state, str1 );
    
    tenAssert( len1 == sizeof(raw1)-1 );
    tenAssert( !memcmp( strBuf( state, str1 ), raw1, len1 ) );
    
    // Allocate a few strings to add them to the GC pool.
    strNew( state, "Test1", 5 );
    strNew( state, "Test2", 5 );
    strNew( state, "Test3", 5 );
    strNew( state, "Test4", 5 );
    strNew( state, "Test5", 5 );
    strNew( state, "Test6", 5 );
}
#endif

String*
strNew( State* state, char const* src, size_t len ) {
    Part strP;
    String* str = stateAllocObj( state, &strP, sizeof(String)+len+1, OBJ_STR );
    str->len = len;
    memcpy( str->buf, src, len );
    str->buf[str->len] = '\0';
    
    stateCommitObj( state, &strP );
    return str;
}

String*
strCpy( State* state, String* str ) {
    Part cpyP;
    String* cpy = stateAllocObj( state, &cpyP, sizeof(String)+str->len+1, OBJ_STR );
    cpy->len = str->len;
    memcpy( cpy->buf, str->buf, str->len );
    cpy->buf[cpy->len] = '\0';
    
    stateCommitObj( state, &cpyP );
    return cpy;
}

String*
strCat( State* state, String* str1, String* str2 ) {
    size_t  len = str1->len + str2->len;
    
    Part catP;
    String* cat = stateAllocObj( state, &catP, sizeof(String)+len+1, OBJ_STR );
    
    cat->len = len;
    memcpy( cat->buf, str1->buf, str1->len );
    memcpy( cat->buf + str1->len, str2->buf, str2->len );
    cat->buf[cat->len] = '\0';
    
    stateCommitObj( state, &catP );
    return cat;
}

String*
strSub( State* state, String* str, llong loc, llong len ) {
    
    // If `loc` is negative then wrap to end.
    if( loc < 0 )
        loc += str->len;
    
    // If `len` is negative then change signs and
    // subtract from `loc`.  This starts the sub-
    // string `len` bytes before the original `loc`,
    // and terminates where at the original `loc`.
    if( len < 0 ) {
        len  = -len;
        loc -= len;
    }
    
    // Check bounds.
    llong end = loc + len;
    if( loc < 0 || loc >= str->len || end < 0 || end > str->len )
        stateErrFmtA( state, ten_ERR_STRING, "Out of range" );
    
    Part subP;
    String* sub = stateAllocObj( state, &subP, sizeof(String)+len+1, OBJ_STR );
    sub->len = len;
    memcpy( sub->buf, str->buf + loc, len );
    
    stateCommitObj( state, &subP );
    return sub;
}

char const*
strBuf( State* state, String* str ) {
    return str->buf;
}

size_t
strLen( State* state, String* str ) {
    return str->len;
}
