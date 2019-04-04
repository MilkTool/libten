TVal var = regs.sp[-2];
TVal val = regs.sp[-1];
tenAssert( tvIsRef( var ) );

if( tvIsTup( val ) )
    stateErrFmtA(
        state, ten_ERR_ASSIGN,
        "Tuple assignment without tuple pattern"
    );

RefT  ref = tvGetRef( var );
refSet( ref, val );
regs.sp -= 1;
regs.sp[-1] = tvUdf();
