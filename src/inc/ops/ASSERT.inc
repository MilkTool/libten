TVal* tup2 = --regs.sp;
uint  cnt2 = 1;
if( tvIsTup( tup2[0] ) ) {
    cnt2     = tvGetTup( tup2[0] );
    tup2    -= cnt2;
    regs.sp -= cnt2;
}

TVal* tup1 = --regs.sp;
uint  cnt1 = 1;
if( tvIsTup( tup1[0] ) ) {
    cnt1     = tvGetTup( tup1[0] );
    tup1    -= cnt1;
    regs.sp -= cnt1;
}

if( cnt1 != cnt2 )
    goto fail;

for( uint i = 0 ; i < cnt1 ; i++ ) {
    if( tvIsObjType( tup1[i], OBJ_STR  ) && tvIsObjType( tup2[i], OBJ_STR ) ) {
         String* str1 = tvGetObj( tup1[i] );
         String* str2 = tvGetObj( tup2[i] );
         if( str1->len != str2->len || memcmp( str1->buf, str2->buf, str1->len ) )
            goto fail;
    }
    else
    if( !tvEqual( tup1[i], tup2[i] ) )
        goto fail;
}

*(regs.sp++) = tvUdf();


EXIT;

fail: {

    VirFun*     fun   = &regs.cls->fun->u.vir;
    uint        place = regs.ip - fun->code;
    LineInfo*   line  = findLine( state, fun, place );
    if( line ) {
        char const* text  = line->text;
        while( isblank( *text ) )
            text++;
        
        stateErrFmtA(
            state,
            ten_ERR_ASSERT,
            "Assertion Failed: %s",
            text
        );
    }
    else {
        stateErrFmtA(
            state,
            ten_ERR_ASSERT,
            "Assertion Failed"
        );
    }
}