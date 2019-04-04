TVal src = regs.sp[-1];
if( !tvIsObj( src ) || datGetTag( tvGetObj( src ) ) != OBJ_REC )
    stateErrFmtA(
        state, ten_ERR_ASSIGN,
        "Record assignment from non-Rec value"
    );

TVal dst = regs.sp[-2LL*opr - 2];
if( !tvIsObj( dst ) || datGetTag( tvGetObj( dst ) ) != OBJ_REC )
    stateErrFmtA(
        state, ten_ERR_ASSIGN,
        "Record assignment to non-Rec value"
    );

TVal (*pairs)[2] = (TVal (*)[2])(regs.sp - 1 - 2*opr);

Record* dRec = tvGetObj( dst );
Record* sRec = tvGetObj( src );
for( uint i = 0 ; i < opr ; i++ ) {
    TVal dKey = pairs[i][0];
    TVal sKey = pairs[i][1];
    recDef( state, dRec, dKey, recGet( state, sRec, sKey ) );
}

regs.sp -= 1 + 2*opr;
regs.sp[-1] = tvUdf();
