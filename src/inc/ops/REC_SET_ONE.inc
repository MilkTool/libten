if( tvIsTup( regs.sp[-1] ) )
    stateErrFmtA(
        state, ten_ERR_ASSIGN,
        "Tuple assignment without tuple pattern"
    );

TVal dst = regs.sp[-3];
if( !tvIsObj( dst ) || datGetTag( tvGetObj( dst ) ) != OBJ_REC )
    stateErrFmtA(
        state, ten_ERR_ASSIGN,
        "Record assignment to non-Rec value"
    );

TVal key = regs.sp[-2];
TVal val = regs.sp[-1];

recSet( state, tvGetObj( dst ), key, val );
regs.sp -= 2;
regs.sp[-1] = tvUdf();
