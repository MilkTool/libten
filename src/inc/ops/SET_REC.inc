if( !tvIsObj( regs.sp[-1] ) || datGetTag( tvGetObj( regs.sp[-1] ) ) != OBJ_REC )
    stateErrFmtA(
        state, ten_ERR_ASSIGN,
        "Record assignment from non-Rec value"
    );

Record* rec = tvGetObj( regs.sp[-1] );
TVal   (*pairs)[2] = (TVal (*)[2])(regs.sp - 1 - 2*opr);
for( uint i = 0 ; i < opr ; i++ ) {
    TVal var = pairs[i][0];
    TVal key = pairs[i][1];
    TVal val = recGet( state, rec, key );
    
    tenAssert( tvIsRef( var ) );
    RefT  ref = tvGetRef( var );
    refSet( ref, val );
}

regs.sp -= 2*opr;
regs.sp[-1] = tvUdf();
