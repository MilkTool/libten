TVal* iv = regs.sp - opr*2 - 1;
tenAssert( tvIsObj( *iv ) );
tenAssert( datGetTag( tvGetObj( *iv ) ) == OBJ_IDX );
Index*  idx = tvGetObj( *iv );
Record* rec = recNew( state, idx );
*iv = tvObj( rec );

TVal (*pairs)[2] = (TVal (*)[2])(iv + 1);

for( uint i = 0 ; i < opr ; i++ ) {
    if( tvIsUdf( pairs[i][1] ) )
        stateErrFmtA(
            state,
            ten_ERR_RECORD,
            "Passed `udf` to record constructor"
        );
    
    recDef( state, rec, pairs[i][0], pairs[i][1] );
}

regs.sp -= opr*2;
