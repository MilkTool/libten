TVal* sv = regs.sp - 1;
if( !tvIsObjType( *sv, OBJ_REC ) )
    stateErrFmtA(
        state, ten_ERR_TYPE,
        "Record expansion attempted on non-Rec type %t",
        *sv
    );
Record* src = tvGetObj( *sv );

TVal* vals = regs.sp - opr;
if( opr > TUP_MAX )
    stateErrFmtA(
        state, ten_ERR_TUPLE,
        "Tuple exceeds max size"
    );

for( uint i = 0 ; i < opr ; i++ ) {
    if( tvIsTup( vals[i] ) )
        stateErrFmtA(
            state, ten_ERR_TUPLE,
            "Nested tuple"
        );
}

uint cnt = 0;
TVal val = recGet( state, src, tvInt( cnt ) );
while( !tvIsUdf( val ) ) {
    regs.sp++;
    regs.sp[-1] = regs.sp[-2];
    regs.sp[-2] = val;
    cnt++;
    val = recGet( state, src, tvInt( cnt ) );
}

regs.sp[-1] = tvTup( cnt + opr );
