tenAssert( tvIsObj( regs.sp[-1] ) );
tenAssert( datGetTag( tvGetObj( regs.sp[-1] ) ) == OBJ_REC );
Record* rec  = tvGetObj( regs.sp[-1] );

uint cnt = 0;
TVal val = recGet( state, rec, tvInt( cnt ) );
while( !tvIsUdf( val ) ) {
    if( cnt + opr >= TUP_MAX )
        stateErrFmtA(
            state, ten_ERR_TUPLE,
            "Max tuple size exceeded"
        );
    
    regs.sp++;
    regs.sp[-1] = regs.sp[-2];
    regs.sp[-2] = recGet( state, rec, tvInt( cnt ) );
    
    cnt++;
}
regs.sp[-1] = tvTup( cnt + opr );
