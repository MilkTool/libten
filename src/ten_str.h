#ifndef ten_str_h
#define ten_str_h
#include "ten_types.h"
#include <stddef.h>

struct String {
    size_t len;
    char   buf[];
};

#define strSize( STATE, STR ) (sizeof(String) + (STR)->len + 1)
#define strTrav( STATE, STR ) 
#define strDest( STATE, STR ) 

void
strInit( State* state );

#ifdef ten_TEST
void
strTest( State* state );
#endif

String*
strNew( State* state, char const* src, size_t len );

String*
strCpy( State* state, String* str );

String*
strCat( State* state, String* str1, String* str2 );

String*
strSub( State* state, String* str, llong loc, llong len );

char const*
strBuf( State* state, String* str );

size_t
strLen( State* state, String* str );

#endif
