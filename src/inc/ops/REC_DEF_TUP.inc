uint  cnt = 1;
TVal* vals = regs.sp - 1;
if( tvIsTup( vals[0] ) ) {
    cnt  = tvGetTup( vals[0] );
    vals = --regs.sp - cnt;
}
if( cnt != opr )
    stateErrFmtA(
        state, ten_ERR_ASSIGN,
        "Tuple assignment with mismatched sizes"
    );

TVal* keys = vals - opr;

TVal dst = keys[-1];
if( !tvIsObj( dst ) || datGetTag( tvGetObj( dst ) ) != OBJ_REC )
    stateErrFmtA(
        state, ten_ERR_ASSIGN,
        "Record assignment to non-Rec value"
    );

Record* dRec = tvGetObj( dst );
for( uint i = 0 ; i < cnt ; i++ )
    recDef( state, dRec, keys[i], vals[i] );

regs.sp -= 2*opr;
regs.sp[-1] = tvUdf();
