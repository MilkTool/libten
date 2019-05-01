TVal var = regs.sp[-2];
TVal val = regs.sp[-1];
if( tvIsTup( val ) )
    stateErrFmtA(
        state, ten_ERR_ASSIGN,
        "Tuple assignment without tuple pattern"
    );

tenAssert( tvIsRef( var ) );

RefT  ref = tvGetRef( var );
refDef( ref, val );
regs.sp -= 1;
regs.sp[-1] = tvUdf();
