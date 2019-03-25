TVal* vals = regs.sp - opr;
if( opr )
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

tenAssert( tvIsObj( regs.sp[-1] ) );
tenAssert( datGetTag( tvGetObj( regs.sp[-1] ) ) == OBJ_REC );
Record* rec  = tvGetObj( regs.sp[-1] );

uint cnt = 0;
do {
    regs.sp++;
    regs.sp[-1] = regs.sp[-2];
    regs.sp[-2] = recGet( state, rec, tvInt( cnt ) );
    cnt++;
} while( !tvIsUdf( regs.sp[-2] ) );

regs.sp[-1] = tvTup( cnt + opr );
